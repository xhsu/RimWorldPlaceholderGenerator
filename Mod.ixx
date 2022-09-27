module;

#define USING_MULTITHREAD

#include <assert.h>
#include <time.h>

#include <array>
#include <chrono>
#include <compare>
#include <filesystem>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>

#ifdef USING_MULTITHREAD
#include <future>
#include <thread>
#endif

#include "tinyxml2/tinyxml2.h"
#include <fmt/color.h>


export module Mod;


using namespace tinyxml2;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace fs = std::filesystem;
namespace ch = std::chrono;

#ifdef USING_MULTITHREAD
using std::future;
using std::jthread;
#endif // USING_MULTITHREAD

using std::array;
using std::list;
using std::pair;
using std::string;
using std::unordered_map;
using std::function;

struct Cell_t
{
	string m_szEntry{ "NULL ENTRY" };
	string m_szOriginalText{ "NO ORIGINAL TEXT" };
	string m_szLocalizedText{ "NO LOCALIZED TEXT" };
};

using EntryList_t = list<string>;
using Localization_t = pair<string, string>;

enum TranslationCell_e : size_t
{
	CELL_ENTRY = 0u,
	CELL_ORIGINAL_TEXT,
	CELL_LOCALIZED_TEXT,
};

inline constexpr array g_rgszNodeShouldLocalise =
{
	"beginLetter"sv,
	"beginLetterLabel"sv,
	"description"sv,
	"fixedName"sv,
	"gerund"sv,
	"gerundLabel"sv,
	"helpText"sv,
	"ingestCommandString"sv,
	"ingestReportString"sv,
	"inspectLine"sv,
	"jobString"sv,
	"label"sv,
	"labelShort"sv,
	"letterLabel"sv,
	"letterText"sv,
	"pawnLabel"sv,
	"pawnsPlural"sv,
	"recoveryMessage"sv,
	"reportString"sv,
	"skillLabel"sv,
	"text"sv,
	"useLabel"sv,
	"verb"sv,
};

inline constexpr array g_rgszNodeShouldLocaliseAsArray =
{
	"rulesStrings"sv,
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

void fnRecursive(const function<void(const string& /*Entry name*/, const char* /*Original Text*/)>& fnCallback, const string& szAccumulatedName, XMLElement* elem) noexcept
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (std::find(std::begin(g_rgszNodeShouldLocalise), std::end(g_rgszNodeShouldLocalise), i->Value()) != std::end(g_rgszNodeShouldLocalise))
				fnCallback(szAccumulatedName + "."s + i->Value(), i->GetText());
			else if (std::find(std::begin(g_rgszNodeShouldLocaliseAsArray), std::end(g_rgszNodeShouldLocaliseAsArray), i->Value()) != std::end(g_rgszNodeShouldLocaliseAsArray))
			{
				unsigned index = 0;
				auto szAccumulatedName2 = szAccumulatedName + "."s + i->Value() + "."s;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					fnCallback(szAccumulatedName2 + std::to_string(index), li->GetText());
			}
			else if (i->FirstChildElement("li"))
			{
				unsigned index = 0;
				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					li->SetValue(std::to_string(index).c_str());

				fnRecursive(fnCallback, szAccumulatedName + "."s + i->Value(), i);
			}
			else
				fnRecursive(fnCallback, szAccumulatedName + "."s + i->Value(), i);
		}
	}
}

[[nodiscard]]
list<string> GetLocalizableEntriesOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	list<string> rgszEntries{};
	auto Defs = doc.FirstChildElement("Defs");
	const auto fn = [&rgszEntries](const string& szEntry, const char* pszOriginalText) { rgszEntries.emplace_back(szEntry); };

	for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
	{
		if (auto defName = i->FirstChildElement("defName"); defName)
			fnRecursive(fn, defName->GetText(), i);
	}

	return rgszEntries;
}

[[nodiscard]]
list<Localization_t> GetOriginalTextsOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	list<Localization_t> rgsz{};
	auto Defs = doc.FirstChildElement("Defs");
	const auto fn = [&rgsz](const string& szEntry, const char* pszOriginalText) { rgsz.emplace_back(szEntry, pszOriginalText); };

	for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
	{
		if (auto defName = i->FirstChildElement("defName"); defName)
			fnRecursive(fn, defName->GetText(), i);
	}

	return rgsz;
}

void GenerateDummyForFile(const fs::path& hXMLSource, const fs::path& hXMLDest, const list<string>& rgszExistedEntries = {}) noexcept
{
	auto rgsz = GetOriginalTextsOfFile(hXMLSource);

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
		fmt::print(fg(fmt::color::gray), "Skipping: {}\n", hXMLDest.string());
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

		fmt::print("\nPatching file: {}\n", fmt::styled(hXMLDest.string(), fg(fmt::color::cyan)));
	}
	else
	{
		dest.InsertFirstChild(dest.NewDeclaration());
		LanguageData = dest.NewElement("LanguageData");
		dest.InsertEndChild(LanguageData);
		dest.SetBOM(true);

		fmt::print("\nCreating file: {}\n", fmt::styled(hXMLDest.string(), fg(fmt::color::cyan)));
	}

	for (const auto& [szEntry, szText] : rgsz)
	{
		LanguageData->InsertNewChildElement(szEntry.c_str())->InsertNewText(szText.c_str());
		fmt::print("Inserting entry \"{}\"\n", szEntry);
	}

	//fs::create_directories(hXMLDest.parent_path());
#ifndef _DEBUG
	dest.SaveFile(hXMLDest.string().c_str());
#else
	//assert(dest.SaveFile(hXMLDest.string().c_str()) == XML_SUCCESS);
#endif

	fmt::print("\n");
}

[[nodiscard]]
list<Localization_t> GetExistingLocOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	list<Localization_t> rgsz;
	if (auto LanguageData = doc.FirstChildElement("LanguageData"); LanguageData)
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			rgsz.emplace_back(i->Value(), i->GetText());
	}

	return rgsz;
}

[[nodiscard]]
EntryList_t GetExistingLocOfFile_EntryOnly(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	EntryList_t rgsz;
	if (auto LanguageData = doc.FirstChildElement("LanguageData"); LanguageData)
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			rgsz.emplace_back(i->Value());
	}

	return rgsz;
}

export [[nodiscard]]
unordered_map<fs::path, list<string>> GetTranslatableEntriesOfMod(const fs::path& hModFolder) noexcept
{
#ifdef USING_MULTITHREAD
	unordered_map<fs::path, future<list<string>>> rgrgszModEntries_f;
#endif // USING_MULTITHREAD

	unordered_map<fs::path, list<string>> rgrgszModEntries;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

#ifdef USING_MULTITHREAD
		rgrgszModEntries_f[hEntry.path()] = std::async(std::launch::async, GetLocalizableEntriesOfFile, hEntry.path());
#else
		rgrgszModEntries[hEntry.path()] = GetLocalizableEntriesOfFile(hEntry.path());
#endif // USING_MULTITHREAD
	}

#ifdef USING_MULTITHREAD
	for (auto& [hPath, elem] : rgrgszModEntries_f)
		rgrgszModEntries[hPath] = elem.get();
#endif

	return rgrgszModEntries;
}

[[nodiscard]]
EntryList_t GetExistingLocalizationsOfMod_EntryOnly(const fs::path& hModFolder, const string& szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	list<future<EntryList_t>> rets_f;
#endif

	EntryList_t ret;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder))
	{
		if (!hEntry.is_directory())
			continue;

		if (hEntry.path().filename().string() != "Languages" || !fs::exists(hEntry.path() / szLanguage))
			continue;

		for (const auto& hDoc : fs::recursive_directory_iterator(hEntry.path() / szLanguage))
		{
			if (!_stricmp(hDoc.path().extension().string().c_str(), ".xml"))
#ifdef USING_MULTITHREAD
				rets_f.emplace_back(std::async(std::launch::async, GetExistingLocOfFile_EntryOnly, hDoc));
#else
				ret.splice(ret.end(), GetExistingLocOfFile_EntryOnly(hDoc));
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

export [[nodiscard]]
list<Localization_t> GetOriginalTextsOfMod(const fs::path& hModFolder) noexcept
{
#ifdef USING_MULTITHREAD
	list<future<list<Localization_t>>> rgsz_f;
#endif // USING_MULTITHREAD

	list<Localization_t> rgsz;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

#ifdef USING_MULTITHREAD
		rgsz_f.emplace_back(std::async(std::launch::async, GetOriginalTextsOfFile, hEntry.path()));
#else
		rgsz.splice(rgsz.end(), GetOriginalTextsOfFile(hEntry));
#endif
	}

#ifdef USING_MULTITHREAD
	for (auto& f : rgsz_f)
		rgsz.splice(rgsz.end(), f.get());
#endif // USING_MULTITHREAD

	rgsz.sort();
	rgsz.unique();

	return rgsz;
}

export void GenerateDummyForMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	// Save the handle, otherwise it would instantly destruct when falling out of current FOR loop.
	// In the case of std::jthread, it would join the main thread and thus no actual multi-thread will happen.
	list<jthread> rgThreads;
#endif // USING_MULTITHREAD

	auto rgszExistingEntries = GetExistingLocalizationsOfMod_EntryOnly(hModFolder, szLanguage);

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

// The path will be the path in Defs instead of InjectedDefs.
export [[nodiscard]]
unordered_map<fs::path, Cell_t> GetTranslationCellOfMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
	unordered_map<fs::path, Cell_t> Cells;
	auto r = GetExistingLocalizationsOfMod_EntryOnly(hModFolder, szLanguage);

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

		XMLDocument doc;
		doc.LoadFile(hEntry.path().string().c_str());

		auto Defs = doc.FirstChildElement("Defs");
		const auto fn = [&Cells, &hEntry](const string& szEntry, const char* pszOriginalText) { Cells[hEntry.path()].m_szEntry = szEntry; Cells[hEntry.path()].m_szOriginalText = pszOriginalText; };

		for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (auto defName = i->FirstChildElement("defName"); defName)
				fnRecursive(fn, defName->GetText(), i);
		}
	}

	return Cells;
}
