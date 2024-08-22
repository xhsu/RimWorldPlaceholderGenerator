#include "Precompiled.hpp"
#include "Mod.hpp"

import Application;
import CRC64;
import HashExtension;
import Style;

using namespace tinyxml2;
using namespace std::literals;

namespace fs = std::filesystem;
namespace ch = std::chrono;

using std::array;
using std::function;
using std::optional;
using std::pair;
using std::span;
using std::string;
using std::string_view;
using std::vector;
using std::wstring_view;

using cppcoro::generator;
using cppcoro::recursive_generator;

enum struct EDecision
{
	Error,
	NoOp,
	Skipped,
	Patched,
	Created,
};

struct translation_t final
{
	fs::path m_TargetFile{};
	string m_Identifier{};
	string m_Text{};

	[[nodiscard]]
	inline wstring_view GetCSharpClass() const& noexcept	// we are returning a view. It must be called from a lvalue.
	{
		if (m_TargetFile.empty())
			return L"";

		auto const parent_path = fs::_Parse_parent_path(m_TargetFile.native());
		auto const folder_name = fs::_Parse_filename(parent_path);

		return folder_name;
	}

	[[nodiscard]]
	inline string GetCrcIdentifier() const noexcept
	{
		// #UPDATE_AT_CPP26	P2845R6 std::formatter<std::filesystem::path>
		auto ret = std::format("{}\\{}", Path::RelativeToLang(m_TargetFile).u8string(), m_Identifier);
		CheckStringForXML(&ret);
		return ret;
	}
};

struct tr_view_t final
{
	tr_view_t() noexcept = default;
	tr_view_t(auto&& TargetFile, auto&& Identifier) noexcept
		: m_TargetFile{ std::forward<decltype(TargetFile)>(TargetFile) }, m_Identifier{ std::forward<decltype(Identifier)>(Identifier) } {}
	tr_view_t(translation_t const& t) noexcept
		: tr_view_t(t.m_TargetFile.native(), t.m_Identifier) {}

	wstring_view m_TargetFile{};
	string_view m_Identifier{};

	constexpr auto operator<=> (tr_view_t const&) const noexcept = default;
};

namespace std
{
	template <>
	struct hash<::tr_view_t> final
	{
		// #UPDATE_AT_CPP23 static operator()
		size_t operator()(tr_view_t const& t) const noexcept;
	};
}

using xmls_t = std::map<fs::path, XMLDocument, std::less<>>;
using sorted_loc_view_t = std::map<wstring_view, dict_view_t, std::less<>>;
using dirty_entries_t = std::unordered_set<tr_view_t>;
using txt_crc_dict_t = std::map<fs::path, uint64_t, sv_iless_t>;

inline vector<translation_t> gAllSourceTexts;
inline sorted_loc_view_t gSortedSourceTexts;
inline xmls_t gAllLocFiles;
inline dirty_entries_t gDirtyEntries;
inline txt_crc_dict_t gStringFillerCRC;

inline void ResetGlobals() noexcept
{
	gAllSourceTexts.clear();
	gSortedSourceTexts.clear();
	gAllLocFiles.clear();
	gDirtyEntries.clear();
	gStringFillerCRC.clear();
}

size_t std::hash<::tr_view_t>::operator()(::tr_view_t const& t) const noexcept
{
	return HashCombine(t.m_TargetFile, t.m_Identifier);
}

void Path::Resolve(string_view path_to_mod, string_view target_lang) noexcept
{
	static constexpr auto fnSetupOptional =
		+[](std::optional<path>& op, path&& obj) noexcept /*static #UPDATE_AT_CPP23*/
		{
			op.reset();
			if (!fs::exists(obj) || !fs::is_directory(obj))
				return;

			op.emplace(std::forward<path>(obj));
		};

	ModDirectory = fs::absolute(path_to_mod);

#ifdef _DEBUG
	ClearDebugFiles();
#endif

	Lang::Directory = ModDirectory / L"Languages" / target_lang;
	Lang::DefInjected = Lang::Directory / L"DefInjected";
	Lang::Keyed = Lang::Directory / L"Keyed";
	Lang::Strings = Lang::Directory / L"Strings";
	Lang::CRC = Lang::Directory / L"CRC.RWPHG";

	fnSetupOptional(Source::Keyed, ModDirectory / L"Languages" / L"English" / L"Keyed");
	fnSetupOptional(Source::Strings, ModDirectory / L"Languages" / L"English" / L"Strings");
}

fs::path Path::RelativeToLang(fs::path const& hPath) noexcept
{
	static std::error_code ec{};

	return fs::relative(hPath, Lang::Directory, ec);
}

void Path::ClearDebugFiles() noexcept
{
	for (auto&& hPath :
		fs::recursive_directory_iterator(ModDirectory)
		| std::views::filter([](auto&& elem) noexcept { return !elem.is_directory(); })	// fucking function overloading.
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& elem) noexcept { return elem.stem().native().ends_with(L"_RWPHG_DEBUG"); })	// Windows only.
		)
	{
		fmt::print("Cleaning DEBUG file: {0}\n", fmt::styled(hPath.u8string(), Style::Debug));
		fs::remove(hPath);
	}

	fmt::print("\n");
}

[[nodiscard]]
static class_info_t const* SearchClassName(string_view szClassName) noexcept
{
	// Attempt directly search first
	for (auto&& dict : ALL_DICTS)
		if (auto const iter = dict->find(szClassName); iter != dict->cend())
			return std::addressof(iter->second);	// #UPDATE_AT_CPP26 __cpp_lib_associative_heterogeneous_insertion 202311L at()

	// Then with all potential namespaces.
	for (auto&& szNamespace : gAllNamespaces)
	{
		auto const szPotentialName = std::format("{}.{}", szNamespace, szClassName);

		for (auto&& dict : ALL_DICTS)
			if (dict->contains(szPotentialName))
				return &dict->at(szPotentialName);
	}

	return nullptr;
}

[[nodiscard]]
static string GetClassFolderName(class_info_t const& info) noexcept
{
	auto szFullName = info.FullName();

	if (auto it = gRimWorldClasses.find(szFullName); it != gRimWorldClasses.cend())
		return it->second.m_Name;	// for vanilla classes, the namespace part can be dropped.

	return szFullName;
}

// Placeholder Generator:
//	Get all english texts
//	Generate current CRC
//	Get all CRC in record
//	Build a library of all loc XMLs
//	Get all existing translations
//	Remove altered entries from existing translations.

[[nodiscard]]
static generator<fs::path> GetAllXmlSourceFiles(fs::path const& hModFolder = Path::ModDirectory, optional<fs::path> const& pKeyedEntry = Path::Source::Keyed) noexcept
{
	// DefInjected
	for (auto&& hPath :
		fs::recursive_directory_iterator(hModFolder / L"Defs")
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		)
	{
		co_yield fs::absolute(hPath);
	}

	// Keyed
	if (pKeyedEntry)
	{
		for (auto&& hPath :
			fs::recursive_directory_iterator(*pKeyedEntry)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
			)
		{
			co_yield fs::absolute(hPath);
		}
	}

	co_return;
}

[[nodiscard]]
static recursive_generator<translation_t> ExtractAllEntriesFromObject(
	string szPrevIdentifier, string_view szTypeName, wstring_view szFileName, XMLElement* def,
	optional<string>&& FolderOverride = std::nullopt/* for list elems as they are meant to place in same folder as their declarer */,
	fs::path const& DefInjected = Path::Lang::DefInjected
) noexcept
{
	auto const pClassInfo = SearchClassName(szTypeName);

	for (auto field = def->FirstChildElement(); field; field = field->NextSiblingElement())
	{
		string_view szFieldName{ field->Name() };
		string szThisIdentifier = std::format("{}.{}", szPrevIdentifier, szFieldName);

		// Case 1: this is a key we should translate!
		if (pClassInfo->m_MustTranslates.contains(szFieldName))
		{
			[[unlikely]]
			if (!field->GetText())
			{
				fmt::print(
					Style::Warning,
					"Field applied with [MustTranslate] {}::{}::{} was found empty in instance '{}'.\n",
					pClassInfo->m_Namespace, pClassInfo->m_Name, szFieldName, szPrevIdentifier
				);
			}
			else
			{
				co_yield{
					DefInjected / FolderOverride.value_or(GetClassFolderName(*pClassInfo)) / szFileName,
					std::move(szThisIdentifier), field->GetText(),
				};

			}
		}

		// Case 2: this is an array of strings!
		else if (pClassInfo->m_ArraysMustTranslate.contains(szFieldName))
		{
			uint_fast16_t idx = 0;

			for (auto li = field->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++idx)
			{
				[[unlikely]]
				if (!li->GetText())
				{
					fmt::print(
						Style::Warning,
						"Field applied with [MustTranslate] {}::{}::{}[{}] was found empty in instance '{}'.\n",
						pClassInfo->m_Namespace, pClassInfo->m_Name, szFieldName, idx, szPrevIdentifier
					);
				}
				else
				{
					co_yield{
						DefInjected / FolderOverride.value_or(GetClassFolderName(*pClassInfo)) / szFileName,
						std::format("{}.{}", szThisIdentifier, idx), li->GetText(),
					};
				}
			}
		}

		// Case 3: this is an array of objects!
		else if (auto const iter = pClassInfo->m_ObjectArrays.find(szFieldName); iter != pClassInfo->m_ObjectArrays.cend())
		{
			uint_fast16_t idx = 0;

			for (auto li = field->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++idx)
			{
				// Everything wrapped in <li/> would be considered as one individual object.
				co_yield ExtractAllEntriesFromObject(
					std::format("{}.{}", szThisIdentifier, idx),
					iter->second,
					szFileName,
					li,

					// The object in List<> must kept in same file as its declarer.
					optional<string>{ std::in_place, FolderOverride.value_or(GetClassFolderName(*pClassInfo)), }
				);
			}
		}

		// Case 4: this is an object that contains a translatable field!
		else if (auto const iter = pClassInfo->m_Objects.find(szFieldName); iter != pClassInfo->m_Objects.cend())
		{
			co_yield ExtractAllEntriesFromObject(
				szThisIdentifier,	// no need for indexing, function will append at its head.
				iter->second,
				szFileName,
				field,	// the field is the entry itself, not further finding.

				// The object in List<> must kept in same file as its declarer.
				optional<string>{ std::in_place, FolderOverride.value_or(GetClassFolderName(*pClassInfo)), }
			);
		}

		// Default: Do nothing. This is not a field that can be translated.
	}
}

[[nodiscard]]
static recursive_generator<translation_t> ExtractAllEntriesFromFile(fs::path const& file, fs::path const& Keyed = Path::Lang::Keyed) noexcept
{
	XMLDocument xml;
	xml.LoadFile(file.u8string().c_str());

	auto const szFileName = fs::_Parse_filename(file.native());

	// DefInjected
	for (auto defs = xml.FirstChildElement("Defs"); defs; defs = defs->NextSiblingElement("Defs"))
	{
		for (auto def = defs->FirstChildElement(); def; def = def->NextSiblingElement())
		{
			if (auto defName = def->FirstChildElement("defName"); defName)
				co_yield ExtractAllEntriesFromObject(defName->GetText(), def->Name(), szFileName, def);
		}
	}

	// Keyed #UNTESTED
	for (auto LanguageData = xml.FirstChildElement("LanguageData");
		LanguageData != nullptr;
		LanguageData = LanguageData->NextSiblingElement("LanguageData"))
	{
		for (auto entry = LanguageData->FirstChildElement();
			entry != nullptr;
			entry = entry->NextSiblingElement())
		{
			auto const pszText = entry->GetText();

			co_yield{
				Keyed / szFileName,
				entry->Name(), pszText == nullptr ? "" : pszText,
			};
		}
	}
}

[[nodiscard]]
static recursive_generator<translation_t> GetAllTranslationEntries() noexcept
{
	for (auto&& file : GetAllXmlSourceFiles())
		co_yield ExtractAllEntriesFromFile(file);
}

[[nodiscard]]
static sorted_loc_view_t GetSortedLocView(span<translation_t const> source = gAllSourceTexts) noexcept
{
	sorted_loc_view_t ret{};

	for (auto&& [hPath, szEntry, szWords] : source)
	{
		auto&&[iter, bNewEntry] = ret[hPath.native()].try_emplace(szEntry, szWords);

		if (!bNewEntry) [[unlikely]]
			fmt::print(
				Style::Warning, "[Warning] Entry '{}' appears twice in file '{}'.\n\tText '{}' was therefore discarded.",
				szEntry, Path::RelativeToLang(hPath).u8string(), szWords
			);
	}

	return ret;
}

[[nodiscard]]
static EDecision ProcessXml(XMLDocument* xml, wstring_view wcsFile, dict_view_t const& EnglishTexts, dirty_entries_t const& DirtyEntries = gDirtyEntries, fs::path const& ModDir = Path::ModDirectory) noexcept
{
	//	If a file already exists:
	//		Remove all dirty entries
	//		Insert all new entries
	//	Else:
	//		Insert all entries known

	thread_local static EDecision LastAction = EDecision::NoOp;

	// Couldn't find this entry? Good, it's a new file.
	auto LanguageData = xml->FirstChildElement("LanguageData");

	// For printing
	auto const szFile = fs::relative(wcsFile, ModDir).u8string();

	if (LanguageData)
	{
		// Sort all entries out.

		std::unordered_set<string_view> existed;
		vector<XMLElement*> dirty, dead;

		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (DirtyEntries.contains({ wcsFile, i->Name() }))	// Is dirty?
				dirty.emplace_back(i);
			else if (!EnglishTexts.contains(i->Name()))	// A dead entry?
				dead.emplace_back(i);
			else
				existed.emplace(i->Name());	// Entries we are going to keep.
		}

#pragma region File Conclusion
		auto const bIsSkipping = dirty.empty() && dead.empty() && existed.size() == EnglishTexts.size();
		if (bIsSkipping)
		{
			fmt::print(Style::Skipping, "{1}Skipping: {0}\n", szFile, LastAction == EDecision::Skipped ? "" : "\n");
			LastAction = EDecision::Skipped;
		}
		else
		{
			fmt::print(Style::Action, "\nPatching File: ");
			fmt::print(Style::Name, "{}\n", szFile);
			LastAction = EDecision::Patched;
		}
#pragma endregion File Conclusion

		// Remove all dirty entries

		for (auto&& entry : dirty)
		{
			fmt::print(Style::Info, "Deleting altered entry \"{}\"\n", entry->Name());
			LanguageData->DeleteChild(entry);
		}

		for (auto&& entry : dead)
		{
			fmt::print(Style::Info, "Removing unreferenced entry \"{}\"\n", entry->Name());
			LanguageData->DeleteChild(entry);
		}

		dirty.clear();
		dead.clear();

		// Insert all new entries

		if (!bIsSkipping)
		{
			LanguageData->InsertNewComment(
				std::format("Generated at: {:%Y-%m-%d}", ch::system_clock::now()).c_str()
			);

			for (auto&& [entry, text] : EnglishTexts)
			{
				if (existed.contains(entry))
					continue;

				fmt::print(Style::Info, "Inserting entry \"{}\"\n", entry);
				LanguageData->InsertNewChildElement(entry.data())->SetText(text.data());
			}
		}
	}
	else
	{
		fmt::print(Style::Action, "\nCreating File: ");
		fmt::print(Style::Name, "{}\n", szFile);
		LastAction = EDecision::Created;

		LanguageData = xml->NewElement("LanguageData");
		xml->InsertEndChild(LanguageData);

		for (auto&& [entry, text] : EnglishTexts)
		{
			fmt::print(Style::Info, "Inserting entry \"{}\"\n", entry);
			LanguageData->InsertNewChildElement(entry.data())->SetText(text.data());
		}
	}

	return LastAction;
}

static void ProcessEveryXml(xmls_t* pret = &gAllLocFiles, sorted_loc_view_t const& SortedLocView = gSortedSourceTexts) noexcept
{
	auto& ret = *pret;

	for (auto&& [wcsPath, EnglishTexts] : SortedLocView)
	{
		auto&& [iter, bNewEntry] = ret.try_emplace(wcsPath);
		auto&& [hPath, xml] = *iter;

		if (fs::exists(hPath))
		{
			xml.LoadFile(hPath.u8string().c_str());
		}
		else
		{
			if (auto const ParentPath = hPath.parent_path(); !fs::exists(ParentPath))
				fs::create_directories(ParentPath);

			xml.InsertFirstChild(xml.NewDeclaration());
			xml.SetBOM(true);
		}

		switch (ProcessXml(&xml, wcsPath, EnglishTexts))
		{
		case EDecision::Created:
		case EDecision::Patched:
#ifdef _DEBUG
			xml.SaveFile(
				std::format("{}\\{}{}", hPath.parent_path().u8string(), hPath.stem().u8string(), "_RWPHG_DEBUG.xml").c_str()
			);
#else
			xml.SaveFile(hPath.u8string().c_str());
#endif
			break;

		default:
			break;
		}
	}
}

static void LoadCRC(
	fs::path const&				prev_records = Path::Lang::CRC,
	sorted_loc_view_t const&	MappedSourceTexts = gSortedSourceTexts,
	dirty_entries_t*			pret = &gDirtyEntries,
	txt_crc_dict_t*				txt_crc_dict = &gStringFillerCRC,
	fs::path const&				LangDir = Path::Lang::Directory,
	optional<fs::path> const&	pStringsDir = Path::Source::Strings
) noexcept
{
	auto const prev_records_str = prev_records.u8string();

	if (!fs::exists(prev_records))
	{
		fmt::print(Style::Warning, "CRC checksum record '{}", fmt::styled(prev_records_str, Style::Name));	// fmtlib cannot restore to main style after any alteration.
		fmt::print(Style::Warning, "' no found.\nSkipping dirt check.\n\n");
		return;
	}

	XMLDocument xml;
	if (auto err = xml.LoadFile(prev_records_str.c_str()); err != XML_SUCCESS) [[unlikely]]
		fmt::print(Style::Error, "XMLDocument::LoadFile returns: {0} ({1})\n", XMLDocument::ErrorIDToName(err), std::to_underlying(err));

	auto const Records = xml.FirstChildElement("Records");
	auto const StringFillers = xml.FirstChildElement("StringFillers");
	uint32_t iCount = 0;
	sv_set_t DeadFiles{};	// in case some files get warned like crazy.

	if (!Records)
	{
		fmt::print(Style::Warning, "Bad record file: missing entry 'Records'.\n");
		goto LAB_SKIP_RECORDS;
	}

	for (auto Record = Records->FirstChildElement(); Record; Record = Record->NextSiblingElement(), ++iCount)
	{
		string_view const szPrevFile{ Record->Attribute("File") };
		fs::path const PrevFile{ LangDir / szPrevFile };	// no matter whether or not the translation exists, the m_TargetingFile is always pointing to the supposely file.
		string_view const PrevIdentifier{ Record->Attribute("Identifier") };
		uint64_t const PrevCRC{ Record->Unsigned64Attribute("CRC") };

		auto const itCurFile = MappedSourceTexts.find(PrevFile);
		if (itCurFile == MappedSourceTexts.cend())
		{
			if (!DeadFiles.contains(szPrevFile))
			{
				fmt::print(Style::Info, "Dead file: {}\n", szPrevFile);
				DeadFiles.emplace(szPrevFile);
			}

			continue;
		}

		auto const& CurIdentifiers = itCurFile->second;
		auto const itCurIdentifier = CurIdentifiers.find(PrevIdentifier);
		if (itCurIdentifier == CurIdentifiers.cend())
		{
			fmt::print(Style::Info, "Dead entry '{}' found in file '{}'\n", PrevIdentifier, szPrevFile);
			continue;
		}

		auto const& CurText = itCurIdentifier->second;
		auto const CurCRC = CRC64::CheckStream((std::byte*)CurText.data(), CurText.size());
		if (PrevCRC != CurCRC)
		{
			fmt::print(Style::Skipping, "Dirt entry found: {}\\{}\n", szPrevFile, PrevIdentifier);

			// The compare result view must be built on top of current identifier.
			// 1. the object lifetime of prev series is about the end.
			// 2. we are going to searching with current text.
			pret->emplace(itCurFile->first, itCurIdentifier->first);
		}
	}

LAB_SKIP_RECORDS:;

	if (pStringsDir)
	{
		if (!StringFillers)
		{
			fmt::print(Style::Warning, "Bad record file: missing entry 'StringFillers'.\n");
			goto LAB_END;
		}

		for (auto StringFiller = StringFillers->FirstChildElement(); StringFiller; StringFiller = StringFiller->NextSiblingElement(), ++iCount)
		{
			txt_crc_dict->try_emplace(
				*pStringsDir / StringFiller->Attribute("File"),
				StringFiller->Unsigned64Attribute("CRC")
			);
		}
	}
	else if (!pStringsDir && StringFillers)
	{
		fmt::print(Style::Warning, "String filler records found, but no string filler exists in current version anymore.\n");
		goto LAB_END;
	}

LAB_END:;
	fmt::print(Style::Positive, "{} CRC record{} retrieved and compared.\n\n", iCount, iCount < 2 ? "" : "s");
}

static void SaveCRC(
	fs::path const&				save_to = Path::Lang::CRC,
	span<translation_t const>	source = gAllSourceTexts,
	optional<fs::path> const&	pStringFillerFolder = Path::Source::Strings
) noexcept
{
	XMLDocument xml;
	xml.InsertFirstChild(xml.NewDeclaration());
	xml.SetBOM(true);

	auto const Version = xml.NewElement("Version");
	xml.InsertEndChild(Version);
	Version->SetAttribute("Major", APP_VERSION.m_major);
	Version->SetAttribute("Minor", APP_VERSION.m_minor);
	Version->SetAttribute("Revision", APP_VERSION.m_revision);
	Version->SetAttribute("Build", BUILD_NUMBER);
	Version->SetAttribute("Checksum", APP_VERSION_COMPILED);

	auto const Timestamp = xml.NewElement("Timestamp");
	xml.InsertEndChild(Timestamp);
	auto const t = fmt::gmtime(std::time(nullptr));
	Timestamp->SetAttribute("GMT", true);
	Timestamp->SetAttribute("Date", fmt::format("{:%Y-%m-%d}", t).c_str());
	Timestamp->SetAttribute("Time", fmt::format("{:%H:%M:%S}", t).c_str());

	auto const Records = xml.NewElement("Records");
	xml.InsertEndChild(Records);

	for (auto&& [File, Identifier, Text] : source)
	{
		auto const Record = Records->InsertNewChildElement("Record");

		Record->SetAttribute("File", Path::RelativeToLang(File).u8string().c_str());
		Record->SetAttribute("Identifier", Identifier.c_str());
		Record->SetAttribute("CRC", CRC64::CheckStream((std::byte*)Text.data(), Text.size()));
	}

	if (pStringFillerFolder)
	{
		// Do not leave an empty node in the document.
		auto const StringFillers = xml.NewElement("StringFillers");
		xml.InsertEndChild(StringFillers);

		for (auto&& hPath :
			fs::recursive_directory_iterator(*pStringFillerFolder)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".txt") == 0; })
			)
		{
			auto const StringFiller = StringFillers->InsertNewChildElement("StringFiller");

			StringFiller->SetAttribute("File", fs::relative(hPath, *pStringFillerFolder).u8string().c_str());
			StringFiller->SetAttribute("CRC", CRC64::CheckFile(hPath.c_str()));
		}
	}

	xml.SaveFile(save_to.u8string().c_str());
}

static void ProcessEveryTxt(optional<fs::path> const& pStringFillerSourceDir = Path::Source::Strings, fs::path const& StringFillerDestDir = Path::Lang::Strings, txt_crc_dict_t const& dict = gStringFillerCRC) noexcept
{
	if (!pStringFillerSourceDir)
		return;

	bool bEndingSentence = true;
	fmt::print(Style::Action, "\nInspecting all string filler files...\n");

	// Handle deletion and alteration.
	for (auto&& [PrevFile, PrevCRC] : dict)
	{
		auto const szPrevFile = fs::relative(PrevFile, *pStringFillerSourceDir).u8string();

		if (!fs::exists(PrevFile))
		{
			bEndingSentence = false;
			fmt::print(Style::Info, "File removed in current version: {}\n", fmt::styled(szPrevFile, Style::Name));

			if (fs::exists(Path::Lang::Strings / szPrevFile))
				fmt::print(Style::Skipping, "\tIt is also suggested to remove the corresponding file from your localization folder.\n");

			continue;
		}

		auto const CurCRC = CRC64::CheckFile(PrevFile.c_str());

		if (CurCRC != PrevCRC)
		{
			bEndingSentence = false;
			fmt::print(Style::Info, "Dirty file: {}\n", fmt::styled(szPrevFile, Style::Name));	// that's all we can do - a warning.
		}
	}

	// Handle the additions.
	for (auto&& hPath :
		fs::recursive_directory_iterator(*pStringFillerSourceDir)
		| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".txt") == 0; })
		)
	{
		auto const RelPath = fs::relative(hPath, *pStringFillerSourceDir).u8string();
		auto const Corresponding = StringFillerDestDir / RelPath;

		if (!fs::exists(Corresponding))
		{
			bEndingSentence = false;
			fmt::print(Style::Info, "File without corresponding localization: {}\n", fmt::styled(RelPath, Style::Name));
		}
	}

	if (bEndingSentence)
		fmt::print(Style::Positive, "Inspection finished without any notable info. (Up-to-date)\n");
}

void ProcessMod() noexcept
{
	ResetGlobals();

	if (gAllSourceTexts.empty())
		gAllSourceTexts = GetAllTranslationEntries() | std::ranges::to<vector>();

	if (gSortedSourceTexts.empty())
		gSortedSourceTexts = GetSortedLocView();

	LoadCRC();
	ProcessEveryXml();
	ProcessEveryTxt();
	SaveCRC(
#ifdef _DEBUG
		Path::Lang::Directory / L"CRC_RWPHG_DEBUG.XML"
#endif
	);
}

// noxref mode:
//	Get all source files
//	iterate all translation file and see whether they are needed

inline constexpr bool (fs::directory_entry::* mfnIsDir)() const = &fs::directory_entry::is_directory;	// fucking C++ function overloading

void NoXRef() noexcept
{
	ResetGlobals();

	if (gAllSourceTexts.empty())
		gAllSourceTexts = GetAllTranslationEntries() | std::ranges::to<vector>();

	if (gSortedSourceTexts.empty())
		gSortedSourceTexts = GetSortedLocView();

	for (auto&& hPath :
		fs::recursive_directory_iterator(Path::Lang::Directory)
		| std::views::filter(std::not_fn(mfnIsDir))	// is NOT dir.
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		| std::views::filter([](auto&& path) noexcept { return !gSortedSourceTexts.contains(path.native()); })
		)
	{
		fmt::print("{}\n", fs::relative(hPath, Path::Lang::Directory).u8string());
	}
}

// File merging mode:
//	Get all noxref file
//	Get all untranslated entry
//	Check uniqueness

[[nodiscard]]
static auto ExtractExistingTranslationFromFile(fs::path const& file) noexcept
{
	XMLDocument xml;
	xml.LoadFile(file.u8string().c_str());

	vector<translation_t> ExistingTranslations{};

	for (auto LanguageData = xml.FirstChildElement("LanguageData"); LanguageData; LanguageData = LanguageData->NextSiblingElement("LanguageData"))
	{
		for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
		{
			if (!i->Name() || !i->GetText())
				continue;

			ExistingTranslations.emplace_back(file, i->Name(), i->GetText());
		}
	}

	return ExistingTranslations;
}

[[nodiscard]]
static auto ExtractAllLastTranslation(sorted_loc_view_t const& SortedSourceView = gSortedSourceTexts) noexcept
{
	std::deque<translation_t> LastTranslations{};	// using vector will cause the address stored in view being invalidated after few insertions.
	sorted_loc_view_t SortedLastTranslations{};	// the ownership of this view belongs to 'LastTranslations'

	for (XMLDocument xml; auto&& file : SortedSourceView | std::views::keys)
	{
		auto const pf = _wfopen(file.data(), L"rb");

		if (pf == nullptr)
			continue;

		xml.LoadFile(pf);
		fclose(pf);

		for (auto LanguageData = xml.FirstChildElement("LanguageData"); LanguageData; LanguageData = LanguageData->NextSiblingElement("LanguageData"))
		{
			for (auto i = LanguageData->FirstChildElement(); i; i = i->NextSiblingElement())
			{
				if (!i->Name() || !i->GetText())
					continue;

				auto const& tr = LastTranslations.emplace_back(file, i->Name(), i->GetText());
				SortedLastTranslations[tr.m_TargetFile.native()].try_emplace(tr.m_Identifier, tr.m_Text);
			}
		}
	}

	return pair{ std::move(LastTranslations), std::move(SortedLastTranslations) };
}

[[nodiscard]]
static bool SimiliarFit(wstring_view const& lhs, wstring_view const& rhs) noexcept
{
	return _wcsnicmp(
		lhs.data(), rhs.data(),
		std::min(lhs.length(), rhs.length())
	) == 0;
}

[[nodiscard]]
static auto GetAllUntranslated(sorted_loc_view_t const& SortedSourceTexts, sorted_loc_view_t const& SortedLastTranslations) noexcept	// the return value is a view built on the first argument.
{
	sorted_loc_view_t ret;

	for (auto&& [file, dict] : SortedSourceTexts)
	{
		auto it1 = SortedLastTranslations.find(file);
		if (it1 == SortedLastTranslations.cend())	// no file in last translation => this entire file is missing.
		{
			ret.try_emplace(file, dict);	// yes, it's a copy.
			continue;
		}

		for (auto&& [id, text] : dict)
		{
			if (!it1->second.contains(id))	// no record of such id => the entry is missing.
				ret[file].try_emplace(id, text);
		}
	}

	return ret;
}

void FileMergingSuggestion(bool bShouldWrite) noexcept
{
	ResetGlobals();

	if (gAllSourceTexts.empty())
		gAllSourceTexts = GetAllTranslationEntries() | std::ranges::to<vector>();

	if (gSortedSourceTexts.empty())
		gSortedSourceTexts = GetSortedLocView();

	auto const UselessFiles =
		fs::recursive_directory_iterator(Path::Lang::Directory)
		| std::views::filter(std::not_fn(mfnIsDir))	// is NOT dir. #UPDATE_AT_CPP26 __cpp_lib_not_fn >= 202306L
		| std::views::transform(&fs::directory_entry::path)
		| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".xml") == 0; })
		| std::views::filter([](auto&& path) noexcept { return !gSortedSourceTexts.contains(path.native()); })
		| std::ranges::to<vector>();

	auto const [LastTranslations, SortedLastTranslations] = ExtractAllLastTranslation();
	auto const Untranslated = GetAllUntranslated(gSortedSourceTexts, SortedLastTranslations);

	for (auto&& NoXRefFile : UselessFiles)
	{
		auto const NoXRefEntries = ExtractExistingTranslationFromFile(NoXRefFile);
		auto const CurFileName = fs::_Parse_filename(NoXRefFile.native());
		[[maybe_unused]] bool bHandled = false;

		for (auto&& [wcsMissingFile, MissingEntries] : Untranslated)
		{
			// Have same filename but in diff dir??
			if (!SimiliarFit(fs::_Parse_filename(wcsMissingFile), CurFileName))
				continue;

			auto const iKeyMaxLen = std::ranges::max(MissingEntries | std::views::keys, {}, &string_view::length).length();
			auto const iEnTxtMaxLen = std::ranges::max(MissingEntries | std::views::values, {}, &string_view::length).length() + (size_t)2;	// #UPDATE_AT_CPP23 ssz literal

			XMLDocument xml;
			XMLElement* LanguageData = nullptr;

			if (auto const f = _wfopen(wcsMissingFile.data(), L"rb"); f != nullptr)
			{
				xml.LoadFile(f);
				fclose(f);

				LanguageData = xml.FirstChildElement("LanguageData");
			}
			else	// new file
			{
				xml.InsertFirstChild(xml.NewDeclaration());
				xml.SetBOM(true);

				LanguageData = xml.NewElement("LanguageData");
				xml.InsertEndChild(LanguageData);
			}

			vector<string> records{};

			// any key we are in need is presenting in the noxref file entries?
			for (auto&& [missing_id, en_text] : MissingEntries)
			{
				auto it = std::ranges::find(NoXRefEntries, missing_id, &translation_t::m_Identifier);
				if (it == NoXRefEntries.cend())
					continue;

				if (records.empty())	// first inserted item!
					LanguageData->InsertNewComment(fmt::format("Merging from file '{}' at {:%Y-%m-%d}", Path::RelativeToLang(wcsMissingFile), ch::system_clock::now()).c_str());

				records.emplace_back(
					fmt::format(u8R"([{0:<{3}}] (EN){1:>{4}?} => {2:?})", it->m_Identifier, en_text, it->m_Text, iKeyMaxLen, iEnTxtMaxLen)
				);

				LanguageData
					->InsertNewChildElement(it->m_Identifier.c_str())
					->SetText(it->m_Text.c_str());

				bHandled = true;
			}

			// something actually patched.
			if (!records.empty())
			{
				fmt::print(Style::Info, "\nUnreferenced file {} has the following translations which would fit into {}\n",
					fmt::styled(Path::RelativeToLang(NoXRefFile), Style::Name),
					fmt::styled(Path::RelativeToLang(wcsMissingFile), Style::Name)
				);

				for (auto&& record : records)
					fmt::print(Style::Info, u8"{}\n", record);

				if (bShouldWrite)
				{
#ifdef _DEBUG
					xml.SaveFile(
						fmt::format("{}\\{}{}", fs::path{ fs::_Parse_parent_path(wcsMissingFile) }, fs::path{ fs::_Parse_stem(wcsMissingFile) }, "_RWPHG_DEBUG.xml").c_str()
					);
#else
					if (auto const f = _wfopen(wcsMissingFile.data(), L"w"); f != nullptr)
					{
						xml.SaveFile(f);
						fclose(f);
					}
					else
						fmt::print(Style::Error, "[::FileMergingSuggestion] Unable to save file \"{}\"", fs::path{ wcsMissingFile });
#endif
				}
			}
		}
	}
}
