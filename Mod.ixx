module;

#define USING_MULTITHREAD

#include <cassert>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <list>
#include <string>
#include <unordered_map>

#ifdef USING_MULTITHREAD
#include <future>
#include <thread>
#endif

#include "tinyxml2/tinyxml2.h"


export module Mod;

import UtlWinConsole;

using namespace tinyxml2;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace fs = std::filesystem;
namespace ch = std::chrono;

#ifdef USING_MULTITHREAD
using std::future;
using std::jthread;
#endif // USING_MULTITHREAD

using std::list;
using std::pair;
using std::string;
using std::unordered_map;

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

export [[nodiscard]]
list<string> ListModFolder(const fs::path& hModFolder) noexcept
{
	XMLDocument doc;
	doc.LoadFile((hModFolder / "LoadFolders.xml").string().c_str());

	list<string> ret;
	auto loadFolders = doc.FirstChildElement("loadFolders");
	for (auto i = loadFolders->FirstChildElement(); i; i = i->NextSiblingElement())
		for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
			ret.emplace_back(i->GetText());

	return ret;
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

void fnRecursive(list<pair<string, string>>& rgszResults, const string& szAccumulatedName, XMLElement* elem) noexcept
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (std::find(std::begin(g_rgszNodeShouldLocalise), std::end(g_rgszNodeShouldLocalise), i->Value()) != std::end(g_rgszNodeShouldLocalise))
				rgszResults.emplace_back(szAccumulatedName + "."s + i->Value(), i->GetText());
			else if (std::find(std::begin(g_rgszNodeShouldLocaliseAsArray), std::end(g_rgszNodeShouldLocaliseAsArray), i->Value()) != std::end(g_rgszNodeShouldLocaliseAsArray))
			{
				unsigned index = 0;
				auto szAccumulatedName2 = szAccumulatedName + "."s + i->Value() + "."s;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					rgszResults.emplace_back(szAccumulatedName2 + std::to_string(index), li->GetText());
			}
			else if (i->FirstChildElement("li"))
			{
				unsigned index = 0;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					li->SetValue(std::to_string(index).c_str());

				fnRecursive(rgszResults, szAccumulatedName + "."s + i->Value(), i);
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

export void GenerateDummyForFile(const fs::path& hXMLSource, const fs::path& hXMLDest, const list<string>& rgszExistedEntries = {}) noexcept
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

	if (!rgszExistedEntries.empty())
	{
		for (auto iter = rgsz.begin(); iter != rgsz.end(); /* Does nothing. */)
		{
			if (std::find(rgszExistedEntries.cbegin(), rgszExistedEntries.cend(), iter->first) != rgszExistedEntries.cend())
				iter = rgsz.erase(iter);
			else
				++iter;
		}
	}

	if (rgsz.empty())
	{
		cout_gray() << "Skipping: " << hXMLDest.string() << '\n';
		return;
	}

	XMLDocument dest;
	XMLElement* LanguageData = nullptr;

	if (fs::exists(hXMLDest))
	{
		dest.LoadFile(hXMLDest.string().c_str());
		LanguageData = dest.FirstChildElement("LanguageData");
		dest.SetBOM(true);

		auto hCurTime = ch::system_clock::to_time_t(ch::system_clock::now());
		LanguageData->InsertNewComment(("Auto generated: "s + ctime(&hCurTime)).c_str());	// #UNDONE_DATE

		cout_w() << "Patching file: " << cyan_text << hXMLDest.string() << '\n';
	}
	else
	{
		dest.InsertFirstChild(dest.NewDeclaration());
		LanguageData = dest.NewElement("LanguageData");
		dest.InsertEndChild(LanguageData);
		dest.SetBOM(true);

		cout_w() << "Creating file: " << cyan_text << hXMLDest.string() << '\n';
	}

	for (const auto& [szEntry, szText] : rgsz)
	{
		LanguageData->InsertNewChildElement(szEntry.c_str())->InsertNewText(szText.c_str());
		cout_w() << "Inserting entry " << std::quoted(szEntry) << '\n';
	}

	//fs::create_directories(hXMLDest.parent_path());
#ifndef _DEBUG
	dest.SaveFile(hXMLDest.string().c_str());
#else
	//assert(dest.SaveFile(hXMLDest.string().c_str()) == XML_SUCCESS);
#endif

	cout_w() << '\n';
}

export [[nodiscard]]
list<string> GetLocalizedEntries(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	list<string> rgszExistedEntires;
	if (auto LanguageData = doc.FirstChildElement("LanguageData"); LanguageData)
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			rgszExistedEntires.emplace_back(i->Value());
	}

	return rgszExistedEntires;
}

export [[nodiscard]]
list<list<string>> GetTranslatableEntriesOfMod(const fs::path& hModFolder) noexcept
{
#ifdef USING_MULTITHREAD
	list<future<list<string>>> rgrgszModEntries_f;
#endif // USING_MULTITHREAD

	list<list<string>> rgrgszModEntries;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;
#ifdef USING_MULTITHREAD
		rgrgszModEntries_f.emplace_back(std::async(std::launch::async, GetTranslatableEntriesOfFile, hEntry.path()));
#else
		rgrgszModEntries.emplace_back(GetTranslatableEntriesOfFile(hEntry.path()));
#endif // USING_MULTITHREAD
	}

#ifdef USING_MULTITHREAD
	for (auto& elem : rgrgszModEntries_f)
		rgrgszModEntries.emplace_back(elem.get());
#endif

	return rgrgszModEntries;
}

export [[nodiscard]]
list<string> GetExistingLocEntries(const fs::path& hFolder, const string& szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	list<future<list<string>>> rets_f;
#endif

	list<string> ret;

	for (const auto& hEntry : fs::recursive_directory_iterator(hFolder))
	{
		if (!hEntry.is_directory())
			continue;

		if (hEntry.path().filename().string() != "Languages" || !fs::exists(hEntry.path() / szLanguage))
			continue;

		for (const auto& hDoc : fs::recursive_directory_iterator(hEntry.path() / szLanguage))
		{
			if (!_stricmp(hDoc.path().extension().string().c_str(), ".xml"))
#ifdef USING_MULTITHREAD
				rets_f.emplace_back(std::async(std::launch::async, GetLocalizedEntries, hDoc));
#else
				ret.splice(ret.end(), GetLocalizedEntries(hDoc));
#endif
		}
	}

#ifdef USING_MULTITHREAD
	for (auto& ret_f : rets_f)
		ret.splice(ret.end(), ret_f.get());
#endif

	ret.sort();
	ret.unique();

	return ret;
}

export void GenerateDummyForMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	list<jthread> rgThreads;
#endif // USING_MULTITHREAD

	auto rgszExistingEntries = GetExistingLocEntries(hModFolder, szLanguage);

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

#ifdef USING_MULTITHREAD
		rgThreads.emplace_back(
			GenerateDummyForFile,
#else
		GenerateDummyForFile(
#endif // USING_MULTITHREAD

			hEntry.path(),
			hModFolder / "Languages" / szLanguage / "DefInjected" / fs::relative(hEntry.path(), hModFolder / "Defs"),
			rgszExistingEntries
		);
	}
}
