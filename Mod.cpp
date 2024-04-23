//#define USING_MULTITHREAD

#include "Precompiled.hpp"
#include "Mod.hpp"

import Application;
import CRC64;
import Style;

using namespace tinyxml2;
using namespace std::literals;

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

using cppcoro::generator;
using cppcoro::recursive_generator;

using localization_t = pair<string, string>;
using crc_dict_t = std::unordered_map<string, uint64_t, std::hash<string_view>, std::equal_to<string_view>>;

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

enum struct EDecision
{
	Error,
	NoOp,
	Skipped,
	Patched,
	Created,
};

#ifdef EXPORT
#undef EXPORT
#endif
#define EXPORT

EXPORT [[nodiscard]]
class_info_t const* GetRootDefClassName(class_info_t const& info, std::span<classinfo_dict_t const* const> dicts) noexcept
{
	if (info.m_Base.empty())
		return &info;

	if (info.m_Base == "Verse.Def")
		return &info;

	for (auto&& dict : dicts)
	{
		if (dict->contains(info.m_Base))
			return GetRootDefClassName(dict->at(info.m_Base), dicts);
	}

	return nullptr;
}

EXPORT [[nodiscard]]
generator<string> ListModFolder(const fs::path& hModFolder) noexcept
{
	XMLDocument doc;
	doc.LoadFile((hModFolder / "LoadFolders.xml").u8string().c_str());

	auto loadFolders = doc.FirstChildElement("loadFolders");
	for (auto i = loadFolders->FirstChildElement(); i; i = i->NextSiblingElement())
		for (auto li = i->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
			co_yield i->GetText();
}

static [[nodiscard]]
recursive_generator<localization_t> ExtractTranslationKeyValues(const string &szAccumulatedName, XMLElement *elem) noexcept	// #UPDATE_AT_CPP23 std::generator. It can do recrusive thing according to standard.
{
	if (elem->FirstChildElement())
	{
		for (auto i = elem->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			auto const szClassName = string_view{ i->Name() };
			auto const szFieldName = string_view{ i->Value() };
			auto const szNextAccumulatedName = std::format("{}.{}", szAccumulatedName, szFieldName);

			// Case 1: this is a key we should translate!
			if (std::ranges::contains(g_rgszNodeShouldLocalise, szFieldName))
				co_yield{ szNextAccumulatedName, i->GetText() ? i->GetText() : ""s };

			// Case 2: this is an array of strings!
			else if (std::ranges::contains(g_rgszNodeShouldLocaliseAsArray, szFieldName))
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

static [[nodiscard]]
recursive_generator<localization_t> ExtractTranslationKeyValues(const fs::path& XmlPath) noexcept	// extract from a file.
{
	XMLDocument doc;
	if (doc.LoadFile(XmlPath.u8string().c_str()) != XML_SUCCESS)
		co_return;

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

static
EDecision GenerateDummyForFile(const fs::path& hXmlEnglish, const fs::path& hXmlOtherLang, sv_set_t const& rgszExistedEntries, sv_set_t* prgszDeadEntries, crc_dict_t const& mCRC, EDecision LastAction) noexcept
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

	/*
	
	1. Anything to add into file?
	2. Anything to remove from file?
	
	*/

	bool const bXmlOtherLangExists = fs::exists(hXmlOtherLang);
	dictionary_t const mAllEntriesInFile{ std::from_range, ExtractTranslationKeyValues(hXmlEnglish) | std::views::as_rvalue };
	dict_view_t const mEntriesToInsert{ std::from_range, mAllEntriesInFile | std::views::filter(fnDirtOrNew) };

	if (mEntriesToInsert.empty() && (prgszDeadEntries->empty() || !bXmlOtherLangExists))
	{
	LAB_SKIP:;
		fmt::print(Style::Skipping, "{1}Skipping: {0}\n", hXmlEnglish.u8string(), LastAction == EDecision::Skipped ? "" : "\n");
		return EDecision::Skipped;
	}

	EDecision decision = EDecision::NoOp;
	XMLDocument dest;
	XMLElement* LanguageData = nullptr;

	if (bXmlOtherLangExists)
	{
		if (auto const err = dest.LoadFile(hXmlOtherLang.u8string().c_str()); err != XMLError::XML_SUCCESS)
		{
			fmt::print(Style::Error, "{0}: {1}\n", XMLDocument::ErrorIDToName(err), hXmlOtherLang.u8string());
			return EDecision::Error;
		}

		LanguageData = dest.FirstChildElement("LanguageData");
		dest.SetBOM(true);

		// There are two types of entries to delete:
		// 1. dead data
		// 2. dirty data
		vector<string> rgszLog{};
		vector<XMLElement*> PendingDeletion{};
		PendingDeletion.reserve(mEntriesToInsert.size());

		for (auto i = LanguageData->FirstChildElement(); i != nullptr; i = i->NextSiblingElement())
		{
			string_view const szName{ i->Name() };

			// Find and delete dirty entry (entries which changed since last mod update.)

			if (mEntriesToInsert.contains(szName))
			{
				PendingDeletion.emplace_back(i);
				rgszLog.emplace_back(fmt::format("Deleting dirt entry \"{}\"\n", szName));
			}

			// find and delete noxref entries.

			if (prgszDeadEntries->contains(szName))
			{
				prgszDeadEntries->erase(szName);
				PendingDeletion.emplace_back(i);
				rgszLog.emplace_back(fmt::format("Deleting dead entry: \"{}\"\n", szName));
			}
		}

		// Nothing to delete? Skip.
		if (PendingDeletion.empty() && mEntriesToInsert.empty())
		{
			goto LAB_SKIP;
		}

		// Mark today for reconition.
		LanguageData->InsertNewComment(
			fmt::format("Generated at: {:%Y-%m-%d}", ch::system_clock::now()).c_str()
		);

		fmt::print(Style::Action, "\nPatching file: {}\n", fmt::styled(hXmlOtherLang.u8string(), Style::Name));
		decision = EDecision::Patched;

		// Removing dirt entry in original file.
		std::ranges::for_each(PendingDeletion, std::bind_front(&XMLElement::DeleteChild, LanguageData));
		PendingDeletion.clear();

		// Makes sure the file operation happens after main log.
		for (auto&& szLog : rgszLog)
			fmt::print(Style::Info, "{}", szLog);
	}
	else
	{
		dest.InsertFirstChild(dest.NewDeclaration());
		LanguageData = dest.NewElement("LanguageData");
		dest.InsertEndChild(LanguageData);
		dest.SetBOM(true);

		fmt::print(Style::Action, "\nCreating file: {}\n", fmt::styled(hXmlOtherLang.u8string(), Style::Name));
		decision = EDecision::Created;
	}

	for (const auto& [szEntry, szText] : mEntriesToInsert)
	{
		LanguageData->InsertNewChildElement(szEntry.data())->InsertNewText(szText.data());
		fmt::print("Inserting entry \"{}\"\n", szEntry);
	}

	fs::create_directories(hXmlOtherLang.parent_path());
#ifndef _DEBUG
	dest.SaveFile(hXmlOtherLang.u8string().c_str());
#else
	assert(dest.SaveFile((hXmlOtherLang.parent_path().u8string() + '\\' + hXmlOtherLang.stem().u8string() + "_RWPHG_DEBUG.xml").c_str()) == XML_SUCCESS);
#endif

	return decision;
}

static [[nodiscard]]
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

static [[nodiscard]]
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

static [[nodiscard]]
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

EXPORT
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

	crc_dict_t mCRC{};	// Existing crc64

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
						//assert(bNewEntry);

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

	dictionary_t const mOriginalTexts{ std::from_range, GetAllOriginalTextsOfMod(hModFolder) | std::views::as_rvalue };
	dictionary_t const mAllExistingLoc{ std::from_range, GetAllExistingLocOfMod(hModFolder, szLanguage) | std::views::as_rvalue };
	sv_set_t const rgszExistingEntries{ std::from_range, mAllExistingLoc | std::views::keys };
	sv_set_t rgszDeadEntries{ std::from_range, mAllExistingLoc | std::views::keys | std::views::filter([&](string_view key) noexcept { return !mOriginalTexts.contains(key); }) };
	EDecision LastAction = EDecision::NoOp;

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
		LastAction = GenerateDummyForFile(
#endif // USING_MULTITHREAD

			hPath,
			hModFolder / L"Languages" / szLanguage / L"DefInjected" / fs::relative(hPath, hModFolder / L"Defs"),
			rgszExistingEntries,
			&rgszDeadEntries,
			mCRC,
			LastAction
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
			LastAction = GenerateDummyForFile(
#endif // USING_MULTITHREAD

				hPath,
				hModFolder / L"Languages" / szLanguage / L"Keyed" / fs::relative(hPath, hKeyedEntry),
				rgszExistingEntries,
				&rgszDeadEntries,
				mCRC,
				LastAction
			);
		}
	}

	// We can't do much about the filler txt file. As they are not mapping one-on-one.

	fmt::println("");

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

	// Alerting about noxref translation entries.

	if (!rgszDeadEntries.empty())
	{
		fmt::println("");

		for (auto&& szKey : rgszDeadEntries)
			fmt::print(Style::Info, "Localisation entry '{}' existed but no original text found!\n", szKey);
	}
}

EXPORT
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

static [[nodiscard]]
generator<fs::path> GetAllOriginalTextFilesOfMod(fs::path const& hModFolder) noexcept
{
	// Handle DefInjection
	for (auto&& hPath :
		fs::recursive_directory_iterator(hModFolder / L"Defs")
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		)
	{
		co_yield std::remove_cvref_t<decltype(hPath)>{ hPath };	// #UPDATE_AT_CPP23 auto{x}
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
			co_yield std::remove_cvref_t<decltype(hPath)>{ hPath };
		}
	}

	co_return;
}

EXPORT
void InspectDuplicatedOriginalText(fs::path const& hModFolder) noexcept
{
	std::vector<fs::path> const rgszXmlFiles{ std::from_range, GetAllOriginalTextFilesOfMod(hModFolder) | std::views::as_rvalue };
	std::map<fs::path, dictionary_t, std::less<>> mXml{};

	for (auto&& hPath : rgszXmlFiles)
	{
		mXml.try_emplace(
			std::remove_cvref_t<decltype(hPath)>{ hPath },	// #UPDATE_AT_CPP23 auto{x}
			std::from_range, ExtractTranslationKeyValues(hPath) | std::views::as_rvalue	// because of the eval order, we cannot actually move the value in vector in.
		);
	}

	for (auto it = mXml.cbegin(); it != mXml.cend(); ++it)
	{
		auto&& [XmlPath, mLoc] = *it;

		for (auto&& key : mLoc | std::views::keys)
		{
			for (auto it2 = it; it2 != mXml.cend(); ++it2)
			{
				auto&& [XmlPath2, mLoc2] = *it2;

				if (XmlPath == XmlPath2) [[unlikely]]	// String cmp!!
					continue;

				if (mLoc2.contains(key))
					fmt::print(Style::Warning, "Key '{}' duplicated in '{}' and in '{}'\n", key, fs::relative(XmlPath, hModFolder).u8string(), fs::relative(XmlPath2, hModFolder).u8string());
			}
		}
	}
}

[[nodiscard]]
static generator<fs::path> GetAllDefFiles(fs::path const& hModFolder) noexcept
{
	// Handle DefInjection
	for (auto&& hPath :
		fs::recursive_directory_iterator(hModFolder / L"Defs")
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		)
	{
		co_yield std::remove_cvref_t<decltype(hPath)>{ hPath };	// #UPDATE_AT_CPP23 auto{x}
	}

	co_return;
}
