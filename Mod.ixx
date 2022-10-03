module;

//#define USING_MULTITHREAD

#include <assert.h>
#include <time.h>

#include <array>
#include <chrono>
#include <compare>
#include <deque>
#include <filesystem>
#include <functional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USING_MULTITHREAD
#include <future>
#include <thread>
#endif

#include "tinyxml2/tinyxml2.h"
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <range/v3/all.hpp>
#include <cppcoro/recursive_generator.hpp>


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
using std::function;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using std::deque;
using std::string_view;

using cppcoro::recursive_generator;
using cppcoro::generator;

struct Cell_t
{
	string m_szEntry{ "NULL ENTRY" };
	string m_szOriginalText{ "NO ORIGINAL TEXT" };
	string m_szLocalizedText{ "NO LOCALIZED TEXT" };
};

using Localization_t = pair<string, string>;

inline constexpr array g_rgszNodeShouldLocalise =
{
	"adjective"sv,
	"approachOrderString"sv,
	"approachingReportString"sv,
	"arrivalTextEnemy"sv,
	"arrivalTextFriendly"sv,
	"arrivedLetter"sv,
	"arrivedLetterLabelPart"sv,
	"baseInspectLine"sv,
	"battleStateLabel"sv,
	"beginLetter"sv,
	"beginLetterLabel"sv,
	"calledOffMessage"sv,
	"categoryLabel"sv,
	"chargeNoun"sv,
	"confirmationDialogText"sv,
	"countdownLabel"sv,
	"customLabel"sv,
	"customSummary"sv,
	"deathMessage"sv,
	"desc"sv,
	"descOverride"sv,
	"description"sv,
	"descriptionFuture"sv,
	"discoveredLetterText"sv,
	"discoveredLetterTitle"sv,
	"effectDesc"sv,
	"endMessage"sv,
	"eventLabel"sv,
	"expectedThingLabelTip"sv,
	"extraTextPawnDeathLetter"sv,
	"extraTooltip"sv,
	"failMessage"sv,
	"finishedMessage"sv,
	"fixedName"sv,
	"formatString"sv,
	"formatStringUnfinalized"sv,
	"gerund"sv,
	"gerundLabel"sv,
	"graphLabelY"sv,
	"groupLabel"sv,
	"headerTip"sv,
	"helpText"sv,
	"helpTextController"sv,
	"ideoName"sv,
	"ingestCommandString"sv,
	"ingestReportString"sv,
	"ingestReportStringEat"sv,
	"inspectStringLabel"sv,
	"instantlyPermanentLabel"sv,
	"jobString"sv,
	"label"sv,
	"labelAbstract"sv,
	"labelAnimals"sv,
	"labelFemale"sv,
	"labelFemalePlural"sv,
	"labelMale"sv,
	"labelMalePlural"sv,
	"labelMechanoids"sv,
	"labelNoun"sv,
	"labelNounPretty"sv,
	"labelOverride"sv,
	"labelPlural"sv,
	"labelShort"sv,
	"labelSocial"sv,
	"letterInfoText"sv,
	"letterLabel"sv,
	"letterLabelEnemy"sv,
	"letterLabelFriendly"sv,
	"letterText"sv,
	"letterTitle"sv,
	"meatLabel"sv,
	"member"sv,	// RimWorld.IdeoSymbolPack.member : string @0400261F
	"message"sv,
	"missingDesc"sv,
//	"name"sv,	// RimWorld.Scenario.name : string @040038B1
	"noCandidatesGizmoDesc"sv,
	"offMessage"sv,
	"onMapInstruction"sv,
	"output"sv,	// Verse.Grammar.Rule_String.output : string @0400194E
	"overrideLabel"sv,
	"overrideTooltip"sv,
	"pawnLabel"sv,
	"pawnSingular"sv,
	"pawnsPlural"sv,
	"permanentLabel"sv,
	"potentialExtraOutcomeDesc"sv,
	"recoveryMessage"sv,
	"rejectInputMessage"sv,
	"removeRecipeLabelOverride"sv,
	"reportString"sv,
	"ritualExpectedDesc"sv,
	"ritualExplanation"sv,
	"royalFavorLabel"sv,
	"shortDescOverride"sv,
	"skillLabel"sv,
	"spectatorGerund"sv,
	"spectatorsLabel"sv,
	"successMessage"sv,
	"successMessageNoNegativeThought"sv,
	"successfullyRemovedHediffMessage"sv,
	"summary"sv,
	"targetPrefix"sv,
//	"text"sv,	// RimWorld.InstructionDef.text : string @040028CC, RimWorld.RitualStageAction_Message.text : string @040037CC
	"textController"sv,
	"textEnemy"sv,
	"textFriendly"sv,
	"textWillArrive"sv,
	"theme"sv,	// RimWorld.IdeoSymbolPack.theme : string @0400261D
	"tipLabelOverride"sv,
//	"type"sv,	// RimWorld.DeityNameType.type : string @040025E0
	"useLabel"sv,
//	"verb"sv,	// RimWorld.WorkGiverDef.verb : string @040052CB, Verse.WorkTypeDef.verb : string @0400079C
	"worshipRoomLabel"sv,
};

inline constexpr array g_rgszNodeShouldLocaliseAsArray =
{
	"rulesStrings"sv,
	"extraInfoLines"sv,
	"extraPredictedOutcomeDescriptions"sv,
	"rulesStrings"sv,
	"thoughtStageDescriptions"sv,
};

export [[nodiscard]]
generator<string> ListModFolder(const fs::path& hModFolder) noexcept
{
	XMLDocument doc;
	doc.LoadFile((hModFolder / "LoadFolders.xml").string().c_str());

	auto loadFolders = doc.FirstChildElement("loadFolders");
	for (auto i = loadFolders->FirstChildElement(); i; i = i->NextSiblingElement())
		for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
			co_yield i->GetText();
}

[[nodiscard]]
recursive_generator<Localization_t> ExtractTranslationKeyValues(const string &szAccumulatedName, XMLElement *elem) noexcept	// #UPDATE_AT_CPP23 std::generator. It can do recrusive thing according to standard.
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			auto const szNextAccumulatedName = szAccumulatedName + "."s + i->Value();
			auto const szIdentifier =	// #TODO hard to tell since inheritance exists.
				/*(szAccumulatedName.find_last_of('.') == string::npos ?
					szAccumulatedName.empty() ?
					""s :
					(szAccumulatedName + '.') :
					(szAccumulatedName.substr(szAccumulatedName.find_last_of('.')) + '.')
					) + */i->Value();

			// Case 1: this is a key we should translate!
			if (::ranges::contains(g_rgszNodeShouldLocalise, szIdentifier))	// #UPDATE_AT_CPP23
				co_yield{ szNextAccumulatedName, i->GetText() ? i->GetText() : ""s };

			// Case 2: this is an array of strings!
			else if (::ranges::contains(g_rgszNodeShouldLocaliseAsArray, szIdentifier))
			{
				unsigned index = 0;

				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)	// Thank God that it contains no 2-dimension string array.
					co_yield{ szNextAccumulatedName + '.' + std::to_string(index), li->GetText() ? li->GetText() : ""s };
			}

			// Case 3: this is an array of objects!
			else if (i->FirstChildElement("li"))
			{
				unsigned index = 0;

				for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++index)
					li->SetValue(std::to_string(index).c_str());

				co_yield ExtractTranslationKeyValues(szNextAccumulatedName, i);
			}

			// Default: To be honest, I don't think we could ever get here.
			else [[unlikely]]
				co_yield ExtractTranslationKeyValues(szNextAccumulatedName, i);
		}
	}
}

[[nodiscard]]
recursive_generator<Localization_t> ExtractTranslationKeyValues(const fs::path& hXML) noexcept	// extract from a file.
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	auto Defs = doc.FirstChildElement("Defs");
	if (!Defs)	// Keyed file??
	{
		if (Defs = doc.FirstChildElement("LanguageData"); Defs)
			for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
				if (i->Value() && i->GetText())
					co_yield{ i->Value(), i->GetText() };
	}
	else
	{
		for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
			if (auto defName = i->FirstChildElement("defName"); defName)
				co_yield ExtractTranslationKeyValues(defName->GetText(), i);
	}
}

void GenerateDummyForFile(const fs::path& hXmlEnglish, const fs::path& hXmlOtherLang, const deque<string>& rgszExistedEntries = {}) noexcept
{
	auto const rgsz = ExtractTranslationKeyValues(hXmlEnglish)
		| ::ranges::to<deque>
		| std::views::filter([&](Localization_t const &Pair) { return !::ranges::contains(rgszExistedEntries, Pair.first); })	// only the keys does not existed in translated file shall pass.
		| std::views::common
		| ::ranges::to<vector>;	// #UPDATE_AT_CPP23

	if (rgsz.empty())
	{
		fmt::print(fg(fmt::color::gray), "Skipping: {}\n", hXmlOtherLang.string());
		return;
	}

	XMLDocument dest;
	XMLElement* LanguageData = nullptr;

	if (fs::exists(hXmlOtherLang))
	{
		if (auto const err = dest.LoadFile(hXmlOtherLang.string().c_str()); err != XMLError::XML_SUCCESS)
		{
			fmt::print(fg(fmt::color::red), "{0}: {1}\n", XMLDocument::ErrorIDToName(err), hXmlOtherLang.string());
			return;
		}

		LanguageData = dest.FirstChildElement("LanguageData");
		dest.SetBOM(true);

		auto hCurTime = ch::system_clock::to_time_t(ch::system_clock::now());
		LanguageData->InsertNewComment(("Auto generated: "s + ctime(&hCurTime)).c_str());	// #UNDONE_DATE

		fmt::print("\nPatching file: {}\n", fmt::styled(hXmlOtherLang.string(), fg(fmt::color::cyan)));
	}
	else
	{
		dest.InsertFirstChild(dest.NewDeclaration());
		LanguageData = dest.NewElement("LanguageData");
		dest.InsertEndChild(LanguageData);
		dest.SetBOM(true);

		fmt::print("\nCreating file: {}\n", fmt::styled(hXmlOtherLang.string(), fg(fmt::color::cyan)));
	}

	for (const auto& [szEntry, szText] : rgsz)
	{
		LanguageData->InsertNewChildElement(szEntry.c_str())->InsertNewText(szText.c_str());
		fmt::print("Inserting entry \"{}\"\n", szEntry);
	}

	fs::create_directories(hXmlOtherLang.parent_path());
#ifndef _DEBUG
	dest.SaveFile(hXmlOtherLang.string().c_str());
#else
	assert(dest.SaveFile((hXmlOtherLang.parent_path().string() + '\\' + hXmlOtherLang.stem().string() + "_RWPHG_DEBUG.xml").c_str()) == XML_SUCCESS);
#endif

	fmt::print("\n");
}

[[nodiscard]]
recursive_generator<Localization_t> GetAllExistingLocOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.string().c_str());

	if (auto LanguageData = doc.FirstChildElement("LanguageData"); LanguageData)
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			if (i->Value() && i->GetText())
				co_yield{ i->Value(), i->GetText() };
	}
}

[[nodiscard]]
recursive_generator<Localization_t> GetAllExistingLocOfMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder))
	{
		if (!hEntry.is_directory())
			continue;

		if (hEntry.path().filename().string() != "Languages" || !fs::exists(hEntry.path() / szLanguage))
			continue;

		for (const auto& hDoc : fs::recursive_directory_iterator(hEntry.path() / szLanguage))
		{
			if (!_stricmp(hDoc.path().extension().string().c_str(), ".xml"))
				co_yield GetAllExistingLocOfFile(hDoc);
		}
	}
}

export [[nodiscard]]
recursive_generator<Localization_t> GetAllOriginalTextsOfMod(const fs::path& hModFolder) noexcept
{
	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

		co_yield ExtractTranslationKeyValues(hEntry.path());
	}
}

export void GenerateDummyForMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	// Save the handle, otherwise it would instantly destruct when falling out of current FOR loop.
	// In the case of std::jthread, it would join the main thread and thus no actual multi-thread will happen.
	deque<jthread> rgThreads;
#endif // USING_MULTITHREAD

#ifdef _DEBUG
	deque<fs::path> rgszFilesToRemove{};

	for (const auto &hEntry : fs::recursive_directory_iterator(hModFolder))
		if (!hEntry.is_directory() && hEntry.path().string().ends_with("_RWPHG_DEBUG.xml"))
			rgszFilesToRemove.emplace_back(hEntry.path());

	for (auto &&hPath : rgszFilesToRemove)
	{
		fmt::print("Cleaning DEBUG file: {0}\n", fmt::styled(hPath.string(), fg(fmt::color::chocolate)));
		fs::remove(hPath);
	}

	if (!rgszFilesToRemove.empty())
	{
		rgszFilesToRemove.clear();
		fmt::print("\n");
	}
#endif

	auto rgszExistingEntries = GetAllExistingLocOfMod(hModFolder, szLanguage)
		| ::ranges::to<deque>
		| std::views::keys
		| ::ranges::to<deque>;	// #UPDATE_AT_CPP23

	std::ranges::sort(rgszExistingEntries);
	auto const [first, last] = std::ranges::unique(rgszExistingEntries.begin(), rgszExistingEntries.end());
	rgszExistingEntries.erase(first, last);

	// Handle DefInjection
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

	// No way we can handle txts.
	for (const auto &hEntry : fs::recursive_directory_iterator(hModFolder / "Languages" / "English" / "Keyed"))
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
			hModFolder / "Languages" / szLanguage / "Keyed" / fs::relative(hEntry.path(), hModFolder / "Languages" / "English" / "Keyed"),
			rgszExistingEntries
		);
	}
}

// The path will be the path in Defs instead of InjectedDefs.
export [[nodiscard]]
unordered_map<fs::path, Cell_t> GetTranslationCellOfMod(const fs::path& hModFolder, const string& szLanguage) noexcept
{
	unordered_map<fs::path, Cell_t> Cells;

	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder / "Defs"))
	{
		if (hEntry.is_directory() || !hEntry.path().has_extension())
			continue;

		if (const auto szExt = hEntry.path().extension().string(); _stricmp(szExt.c_str(), ".xml"))
			continue;

		XMLDocument doc;
		doc.LoadFile(hEntry.path().string().c_str());

		auto Defs = doc.FirstChildElement("Defs");

		for (auto i = Defs->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (auto defName = i->FirstChildElement("defName"); defName)
				for (auto &&[szEntry, szOriginalText] : ExtractTranslationKeyValues(defName->GetText(), i))
					Cells[hEntry.path()] = Cell_t{ .m_szEntry = std::move(szEntry), .m_szOriginalText = std::move(szOriginalText) };
		}
	}

	return Cells;
}
