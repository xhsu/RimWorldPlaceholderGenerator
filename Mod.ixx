module;

#include <filesystem>
#include <future>
#include <list>
#include <string>
#include <thread>

#include "tinyxml2/tinyxml2.h"

#ifdef _DEBUG
#include <iostream>
#endif

export module Mod;

using namespace tinyxml2;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace fs = std::filesystem;

using std::future;
using std::list;
using std::pair;
using std::string;

const string g_rgszNodeShouldLocalise[] =
{
	"beginLetter",
	"beginLetterLabel",
	"description",
	"fixedName",
	"gerund",
	"gerundLabel",
	"helpText",
	"ingestCommandString",
	"ingestReportString",
	"inspectLine",
	"jobString",
	"label",
	"labelShort",
	"letterLabel",
	"letterText",
	"pawnLabel",
	"pawnsPlural",
	"recoveryMessage",
	"reportString",
	"skillLabel",
	"text",
	"useLabel",
	"verb",
};

const string g_rgszNodeShouldLocaliseAsArray[] =
{
	"rulesStrings",
};

export void ListModFolder(const fs::path& hModFolder) noexcept
{
	XMLDocument doc;
	doc.LoadFile((hModFolder / "LoadFolders.xml").string().c_str());

	auto loadFolders = doc.FirstChildElement("loadFolders");
	for (auto i = loadFolders->FirstChildElement(); i; i = i->NextSiblingElement())
		for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
			std::cout << li->GetText() << '\n';
}

void fnRecursive(list<string>* prgszResults, const string& szAccumulatedName, const XMLElement* elem) noexcept
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (std::find(std::begin(g_rgszNodeShouldLocalise), std::end(g_rgszNodeShouldLocalise), i->Value()) != std::end(g_rgszNodeShouldLocalise))
				prgszResults->emplace_back(szAccumulatedName + "."s + i->Value());
			else if (std::find(std::begin(g_rgszNodeShouldLocaliseAsArray), std::end(g_rgszNodeShouldLocaliseAsArray), i->Value()) != std::end(g_rgszNodeShouldLocaliseAsArray))
			{
				int index = 0;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					prgszResults->emplace_back(szAccumulatedName + "."s + i->Value() + "."s + std::to_string(index));
			}
			else
				fnRecursive(prgszResults, szAccumulatedName + "."s + i->Value(), i);
		}
	}
}

void fnRecursive(list<pair<string, string>>& rgszResults, const string& szAccumulatedName, const XMLElement* elem) noexcept
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (std::find(std::begin(g_rgszNodeShouldLocalise), std::end(g_rgszNodeShouldLocalise), i->Value()) != std::end(g_rgszNodeShouldLocalise))
				rgszResults.emplace_back(szAccumulatedName + "."s + i->Value(), i->GetText());
			else if (std::find(std::begin(g_rgszNodeShouldLocaliseAsArray), std::end(g_rgszNodeShouldLocaliseAsArray), i->Value()) != std::end(g_rgszNodeShouldLocaliseAsArray))
			{
				int index = 0;
				auto szAccumulatedName2 = szAccumulatedName + "."s + i->Value() + "."s;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					rgszResults.emplace_back(szAccumulatedName2 + std::to_string(index), li->GetText());
			}
			else
				fnRecursive(rgszResults, szAccumulatedName + "."s + i->Value(), i);
		}
	}
}

export [[nodiscard]]
list<string> GetTranslatableEntriesOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	list<string> rgszEntries{};
	auto Defs = doc.FirstChildElement("Defs");

	for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
	{
		if (auto defName = i->FirstChildElement("defName"); defName)
			fnRecursive(&rgszEntries, defName->GetText(), i);
	}

	return rgszEntries;
}

export void GetDummyLocalizationOfFile(const fs::path& hXMLSource, XMLDocument* ret) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXMLSource.string().c_str());

	list<pair<string, string>> rgsz{};
	auto Defs = doc.FirstChildElement("Defs");

	for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
	{
		if (auto defName = i->FirstChildElement("defName"); defName)
			fnRecursive(rgsz, defName->GetText(), i);
	}

	XMLElement* LanguageData = ret->FirstChildElement("LanguageData");
	if (!LanguageData)
	{
		LanguageData = ret->NewElement("LanguageData");
		ret->InsertEndChild(LanguageData);
	}

	for (const auto& [szEntry, szText] : rgsz)
	{
		LanguageData->InsertNewChildElement(szEntry.c_str())->InsertNewText(szText.c_str());
	}
}

export [[nodiscard]]
auto GetTranslatableEntriesOfMod(const fs::path& hModFolder) noexcept
{
	list<future<list<string>>> rgrgszModEntries_f;
	list<list<string>> rgrgszModEntries;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

		rgrgszModEntries_f.emplace_back(std::async(GetTranslatableEntriesOfFile, hEntry.path()));
	}

	for (auto& elem : rgrgszModEntries_f)
		rgrgszModEntries.emplace_back(elem.get());

	return rgrgszModEntries;
}
