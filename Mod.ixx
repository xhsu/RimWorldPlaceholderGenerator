module;

//#define USING_MULTITHREAD

#include <assert.h>
#include <time.h>

#include <array>
#include <chrono>
#include <compare>
#include <filesystem>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <span>
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
#include <cppcoro/recursive_generator.hpp>	// #UPDATE_AT_CPP23 generator


export module Mod;

import Application;
import CRC64;
import Style;


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
using std::span;
using std::string;
using std::string_view;
using std::vector;

using cppcoro::recursive_generator;
using cppcoro::generator;

struct cell_t
{
	string m_szEntry{ "NULL ENTRY" };
	string m_szOriginalText{ "NO ORIGINAL TEXT" };
	string m_szLocalizedText{ "NO LOCALIZED TEXT" };
};

struct sv_less_t final
{
	using is_transparent = int;

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	constexpr auto operator() (string_view lhs, string_view rhs) const noexcept
	{
		return lhs < rhs;
	}
};

using localization_t = pair<string, string>;
using string_set_t = std::set<string, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_set
using crc_dictionary_t = std::unordered_map<string, uint64_t, std::hash<string_view>, std::equal_to<string_view>>;
using new_translation_t = std::map<string, string, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_map

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
	doc.LoadFile((hModFolder / "LoadFolders.xml").u8string().c_str());

	auto loadFolders = doc.FirstChildElement("loadFolders");
	for (auto i = loadFolders->FirstChildElement(); i; i = i->NextSiblingElement())
		for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
			co_yield i->GetText();
}

[[nodiscard]]
recursive_generator<localization_t> ExtractTranslationKeyValues(const string &szAccumulatedName, XMLElement *elem) noexcept	// #UPDATE_AT_CPP23 std::generator. It can do recrusive thing according to standard.
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
			if (std::ranges::contains(g_rgszNodeShouldLocalise, szIdentifier))
				co_yield{ szNextAccumulatedName, i->GetText() ? i->GetText() : ""s };

			// Case 2: this is an array of strings!
			else if (std::ranges::contains(g_rgszNodeShouldLocaliseAsArray, szIdentifier))
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
recursive_generator<localization_t> ExtractTranslationKeyValues(const fs::path& hXML) noexcept	// extract from a file.
{
	XMLDocument doc;
	doc.LoadFile(hXML.u8string().c_str());

	auto Defs = doc.FirstChildElement("Defs");
	if (!Defs)	// Keyed file??
	{
		// Assume it is a Keyed.xml type file.
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

void GenerateDummyForFile(const fs::path& hXmlEnglish, const fs::path& hXmlOtherLang, string_set_t const& rgszExistedEntries, crc_dictionary_t const& mCRC) noexcept
{
	auto const fnDirtOrNew =
		[&](localization_t const& kv) noexcept -> bool
		{
			// Is new entry?
			if (!std::ranges::contains(rgszExistedEntries, kv.first))
				return true;

			if (auto const it = mCRC.find(kv.first); it != mCRC.cend())
				return it->second != CRC64::CheckStream((std::byte*)kv.second.c_str(), kv.second.size());

			// contains in existing translation, but no found in crc. Treats as 'skip'
			return false;
		};

	new_translation_t mEntriesToInsert{};
	for (auto&& [entry, str] : ExtractTranslationKeyValues(hXmlEnglish) | std::views::filter(fnDirtOrNew))
	{
		// #UPDATE_AT_CPP26 discard var '_'
		auto const&& [iter, bIsNew] = mEntriesToInsert.try_emplace(std::move(entry), std::move(str));
		assert(bIsNew);
	}

	if (mEntriesToInsert.empty())
	{
		fmt::print(Style::Skipping, "Skipping: {}\n", hXmlEnglish.u8string());
		return;
	}

	XMLDocument dest;
	XMLElement* LanguageData = nullptr;

	if (fs::exists(hXmlOtherLang))
	{
		if (auto const err = dest.LoadFile(hXmlOtherLang.u8string().c_str()); err != XMLError::XML_SUCCESS)
		{
			fmt::print(Style::Error, "{0}: {1}\n", XMLDocument::ErrorIDToName(err), hXmlOtherLang.u8string());
			return;
		}

		LanguageData = dest.FirstChildElement("LanguageData");
		dest.SetBOM(true);

		// Mark today for reconition.
		LanguageData->InsertNewComment(
			fmt::format("Auto generated: {:%Y-%m-%d %H:%M:%S}", ch::system_clock::now()).c_str()
		);

		fmt::print(Style::Action, "\nPatching file: {}\n", fmt::styled(hXmlOtherLang.u8string(), Style::Name));

		// Removing dirt entry in original file.

		vector<XMLElement*> rgpToDelete{};
		rgpToDelete.reserve(mEntriesToInsert.size());

		for (auto i = LanguageData->FirstChildElement(); i != nullptr; i = i->NextSiblingElement())
		{
			if (string_view const szName{ i->Name() }; mEntriesToInsert.contains(szName))
			{
				rgpToDelete.emplace_back(i);
				fmt::print("Deleting dirt entry \"{}\"\n", szName);
			}
		}

		std::ranges::for_each(rgpToDelete, std::bind_front(&XMLElement::DeleteChild, LanguageData));
		rgpToDelete.clear();
	}
	else
	{
		dest.InsertFirstChild(dest.NewDeclaration());
		LanguageData = dest.NewElement("LanguageData");
		dest.InsertEndChild(LanguageData);
		dest.SetBOM(true);

		fmt::print(Style::Action, "\nCreating file: {}\n", fmt::styled(hXmlOtherLang.u8string(), Style::Name));
	}

	for (const auto& [szEntry, szText] : mEntriesToInsert)
	{
		LanguageData->InsertNewChildElement(szEntry.c_str())->InsertNewText(szText.c_str());
		fmt::print("Inserting entry \"{}\"\n", szEntry);
	}

	fs::create_directories(hXmlOtherLang.parent_path());
#ifndef _DEBUG
	dest.SaveFile(hXmlOtherLang.u8string().c_str());
#else
	assert(dest.SaveFile((hXmlOtherLang.parent_path().u8string() + '\\' + hXmlOtherLang.stem().u8string() + "_RWPHG_DEBUG.xml").c_str()) == XML_SUCCESS);
#endif

	fmt::print("\n");
}

[[nodiscard]]
recursive_generator<localization_t> GetAllExistingLocOfFile(const fs::path& hXML) noexcept
{
	XMLDocument doc;
	doc.LoadFile(hXML.u8string().c_str());

	if (auto LanguageData = doc.FirstChildElement("LanguageData"); LanguageData)
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			if (i->Value() && i->GetText())
				co_yield{ i->Value(), i->GetText() };
	}
}

[[nodiscard]]
recursive_generator<localization_t> GetAllExistingLocOfMod(const fs::path& hModFolder, string_view szLanguage) noexcept
{
	for (const auto& hEntry : fs::recursive_directory_iterator(hModFolder))
	{
		if (!hEntry.is_directory())
			continue;

		if (hEntry.path().filename().u8string() != "Languages" || !fs::exists(hEntry.path() / szLanguage))
			continue;

		for (const auto& hDoc : fs::recursive_directory_iterator(hEntry.path() / szLanguage))
		{
			if (!_stricmp(hDoc.path().extension().u8string().c_str(), ".xml"))
				co_yield GetAllExistingLocOfFile(hDoc);
		}
	}
}

export [[nodiscard]]
recursive_generator<localization_t> GetAllOriginalTextsOfMod(const fs::path& hModFolder) noexcept
{
	// Handle DefInjection
	for (auto&& hPath :
		fs::recursive_directory_iterator(hModFolder / L"Defs")
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		)
	{
		co_yield ExtractTranslationKeyValues(hPath);
	}

	if (auto const hKeyedEntry = hModFolder / L"Languages" / L"English" / L"Keyed"; fs::exists(hKeyedEntry))
	{
		for (auto&& hPath :
			fs::recursive_directory_iterator(hKeyedEntry)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
			)
		{
			co_yield ExtractTranslationKeyValues(hPath);
		}
	}

	co_return;
}

export
void GenerateDummyForMod(const fs::path& hModFolder, string_view szLanguage) noexcept
{
#ifdef USING_MULTITHREAD
	// Save the handle, otherwise it would instantly destruct when falling out of current FOR loop.
	// In the case of std::jthread, it would join the main thread and thus no actual multi-thread will happen.
	deque<jthread> rgThreads;
#endif // USING_MULTITHREAD

#ifdef _DEBUG

	for (auto &&hPath :
		fs::recursive_directory_iterator(hModFolder)
		| std::views::filter([](auto&& elem) noexcept { return !elem.is_directory(); })	// fucking function overloading.
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& elem) noexcept { return elem.stem().native().ends_with(L"_RWPHG_DEBUG"); })	// Windows only.
		)
	{
		fmt::print("Cleaning DEBUG file: {0}\n", fmt::styled(hPath.u8string(), Style::Debug));
		fs::remove(hPath);
	}

#endif

	crc_dictionary_t mCRC{};	// Existing crc64

	if (auto const hChecksumPath = hModFolder / L"Languages" / szLanguage / L"checksum.xml.RWPHG"; fs::exists(hChecksumPath))
	{
		fmt::print(Style::Positive, "CRC checksum dictionary loaded!\n\n");

		XMLDocument xml;
		xml.LoadFile(hChecksumPath.u8string().c_str());

		auto const fnReadCRC =
			[&](const char* pszName) noexcept
			{
				if (auto const LanguageData = xml.FirstChildElement(pszName))
				{
					for (auto i = LanguageData->FirstChildElement(); i != nullptr; i = i->NextSiblingElement())
					{
						auto&& [iter, bNewEntry] = mCRC.try_emplace(i->Name(), i->Unsigned64Attribute("CRC64"));
						assert(bNewEntry);

						[[unlikely]]
						if (!bNewEntry)
							fmt::print(Style::Warning, "[Warning] Naming clash: '{}'\n", i->Name());
					}
				}
			};

		fnReadCRC("LanguageData");
		fnReadCRC("StringFillers");
	}
	else
	{
		fmt::print(Style::Warning, "CRC checksum dictionary '{}", fmt::styled(hChecksumPath.u8string(), Style::Name));	// fmtlib cannot restore to main style after any alteration.
		fmt::print(Style::Warning, "' no found.\nSkipping dirt check.\n\n");
	}

	string_set_t rgszExistingEntries{};
	for (auto&& entry : GetAllExistingLocOfMod(hModFolder, szLanguage) | std::views::keys)
		rgszExistingEntries.emplace(std::move(entry));


	// Handle DefInjection
	for (auto&& hPath :
		fs::recursive_directory_iterator(hModFolder / L"Defs")
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		)
	{
#ifdef USING_MULTITHREAD
		rgThreads.emplace_back(
			GenerateDummyForFile,
#else
		GenerateDummyForFile(
#endif // USING_MULTITHREAD

			hPath,
			hModFolder / L"Languages" / szLanguage / L"DefInjected" / fs::relative(hPath, hModFolder / L"Defs"),
			rgszExistingEntries,
			mCRC
		);
	}

	if (auto const hKeyedEntry = hModFolder / L"Languages" / L"English" / L"Keyed"; fs::exists(hKeyedEntry))
	{
		for (auto &&hPath :
			fs::recursive_directory_iterator(hKeyedEntry)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
			)
		{
#ifdef USING_MULTITHREAD
			rgThreads.emplace_back(
				GenerateDummyForFile,
#else
			GenerateDummyForFile(
#endif // USING_MULTITHREAD
				hPath,
				hModFolder / L"Languages" / szLanguage / L"Keyed" / fs::relative(hPath, hKeyedEntry),
				rgszExistingEntries,
				mCRC
			);
		}
	}

	// We can't do much about the filler txt file. As they are not mapping one-on-one.

	if (auto const szStringFillerFolder = hModFolder / L"Languages" / L"English" / L"Strings";
		fs::exists(szStringFillerFolder) && fs::is_directory(szStringFillerFolder)
		)
	{
		for (auto&& hPath :
			fs::recursive_directory_iterator(szStringFillerFolder)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".txt") == 0; })
			)
		{
			auto const checksum = CRC64::CheckFile(hPath.u8string().c_str());
			auto const RelativePath = fs::relative(hPath, szStringFillerFolder);

			// Avoiding '\' and '/' issue.
			auto Text{ RelativePath.u8string() };
			for (auto& c : Text)
				if (c == '\\' || c == '/')
					c = '.';

			auto const CorrespondingFile{ hModFolder / L"Languages" / szLanguage / L"Strings" / RelativePath };
			auto const bLocExists = fs::exists(CorrespondingFile);	// Is it a new filler?
			auto const it = mCRC.find(Text);
			auto const bCRCExists = it != mCRC.cend();
			auto const bDirty = bCRCExists && it->second != checksum;

			if (bLocExists && bDirty)
				fmt::print(Style::Warning, "Altered filler file: {}\n", fmt::styled(hPath.u8string(), Style::Name));
			else if (bLocExists && !bCRCExists)
				fmt::print(Style::Skipping, "String filler file has no previous CRC checksum: {}\n", hPath.u8string());
			else if (bLocExists && !bDirty)
				fmt::print(Style::Skipping, "Skipping string filler: {}\n", hPath.u8string());
			else if (!bLocExists)
			{
				fs::create_directories(CorrespondingFile.parent_path());

#ifndef _DEBUG
				fs::copy(hPath, CorrespondingFile);
#else
				fs::copy(
					hPath,
					CorrespondingFile.parent_path() / (CorrespondingFile.stem().native() + L"_RWPHG_DEBUG.txt")
				);
#endif
				fmt::print(Style::Action, "Creating filler file: {}\n", fmt::styled(CorrespondingFile.u8string(), Style::Name));
			}
			else [[unlikely]]
				fmt::print(Style::Error, "Unhandled exception on file: {}.\n\tbLocExists: {}, bCRCExists: {}, bDirty: {}\n", Text, bLocExists, bCRCExists, bDirty);
		}
	}
}

export
void GenerateCrcRecordForMod(fs::path const& hModFolder, string_view szLanguage) noexcept
{
	XMLDocument dest;
	dest.InsertFirstChild(dest.NewDeclaration());

	auto const Version = dest.NewElement("RWPHG.Version");
	dest.InsertEndChild(Version);
	dest.SetBOM(true);
	Version->SetAttribute("Major", APP_VERSION.m_major);
	Version->SetAttribute("Minor", APP_VERSION.m_minor);
	Version->SetAttribute("Revision", APP_VERSION.m_revision);
	Version->SetAttribute("Build", APP_VERSION.m_build);
	Version->SetAttribute("Date", BUILD_NUMBER);

	auto const LanguageData = dest.NewElement("LanguageData");
	dest.InsertEndChild(LanguageData);

	for (auto&& [szKey, szLoc] : GetAllOriginalTextsOfMod(hModFolder))
	{
		auto const checksum = CRC64::CheckStream((std::byte*)szLoc.c_str(), szLoc.size());
		LanguageData->InsertNewChildElement(szKey.c_str())->SetAttribute("CRC64", checksum);
	}
	
	// As least we have some way to see if the filler pool gets touched.

	if (auto const szStringFillerFolder = hModFolder / L"Languages" / L"English" / L"Strings";
		fs::exists(szStringFillerFolder) && fs::is_directory(szStringFillerFolder)
		)
	{
		auto const StringFillers = dest.NewElement("StringFillers");
		dest.InsertEndChild(StringFillers);

		for (auto&& hPath :
			fs::recursive_directory_iterator(szStringFillerFolder)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".txt") == 0; })
			)
		{
			auto const checksum = CRC64::CheckFile(hPath.u8string().c_str());
			auto const RelativePath = fs::relative(hPath, szStringFillerFolder);

			// Avoiding '\' and '/' issue.
			auto Text{ RelativePath.u8string() };
			for (auto& c : Text)
				if (c == '\\' || c == '/')
					c = '.';

			StringFillers->InsertNewChildElement(Text.c_str())->SetAttribute("CRC64", checksum);
		}
	}

#ifndef _DEBUG
	auto const crc_file_path{ (hModFolder / L"Languages" / szLanguage / L"checksum.xml.RWPHG").u8string() };
	if (auto const err = dest.SaveFile(crc_file_path.c_str()); err != XML_SUCCESS)
	{
		fmt::print(Style::Error, "Error: {}({})", XMLDocument::ErrorIDToName(err), std::to_underlying(err));
		return;
	}
#else
	auto const crc_file_path{ (hModFolder / L"Languages" / szLanguage / L"checksum_RWPHG_DEBUG.xml").u8string() };
	assert(dest.SaveFile(crc_file_path.c_str()) == XML_SUCCESS);
#endif

	fmt::print(Style::Action, "\nCRC checksum dictionary saved: {}\n", fmt::styled(crc_file_path, Style::Name));
}
