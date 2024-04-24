#include "Precompiled.hpp"
#include "Mod.hpp"

import Application;
import CRC64;
import Style;

using namespace tinyxml2;
using namespace std::literals;

namespace fs = std::filesystem;
namespace ch = std::chrono;

using std::array;
using std::function;
using std::pair;
using std::span;
using std::string;
using std::string_view;
using std::vector;
using std::wstring_view;

using cppcoro::generator;
using cppcoro::recursive_generator;

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
	tr_view_t(auto&& TargetFile, auto&& Identifier, auto&& Text) noexcept
		: m_TargetFile{ std::forward<decltype(TargetFile)>(TargetFile) }, m_Identifier{ std::forward<decltype(Identifier)>(Identifier) }, m_Text{ std::forward<decltype(Text)>(Text) } {}
	tr_view_t(translation_t const& t) noexcept
		: tr_view_t(t.m_TargetFile.native(), t.m_Identifier, t.m_Text) {}

	wstring_view m_TargetFile{};
	string_view m_Identifier{};
	string_view m_Text{};

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
using sorted_loc_view_t = std::map<wstring_view, vector<pair<string_view, string_view>>, std::less<>>;
using crc_dict_t_2 = std::unordered_map<string, uint64_t>;

inline vector<translation_t> gAllSourceTexts;
inline sorted_loc_view_t gSortedSourceTexts;
inline xmls_t gAllLocFiles;
inline crc_dict_t_2 gPrevCRC, gCurCRC;

inline std::size_t HashCombine(auto&&... vals) noexcept
{
	// Ref: https://stackoverflow.com/questions/4948780/magic-number-in-boosthash-combine
	constexpr std::size_t UINT_MAX_OVER_PHI =
#ifdef _M_X64
		0x9e3779b97f4a7c16;
#else
		0x9e3779b9;
#endif

	std::size_t ret = 0;
	auto const functors = std::tuple{ std::hash<std::remove_cvref_t<decltype(vals)>>{}... };

	[&] <size_t... I>(std::index_sequence<I...>&&)
	{
		((ret ^= std::get<I>(functors)(vals) + UINT_MAX_OVER_PHI + (ret << 6) + (ret >> 2)), ...);
	}
	(std::index_sequence_for<decltype(vals)...>{});

	return ret;
}

size_t std::hash<::tr_view_t>::operator()(::tr_view_t const& t) const noexcept
{
	return HashCombine(t.m_TargetFile, t.m_Identifier, t.m_Text);
}

void Path::Resolve(string_view path_to_mod, string_view target_lang) noexcept
{
	ModDirectory = fs::absolute(path_to_mod);

	TargetLangDirectory = ModDirectory / target_lang;
	TargetLangDefInjected = TargetLangDirectory / L"DefInjected";
	TargetLangKeyed = TargetLangDirectory / L"Keyed";
	TargetLangStrings = TargetLangDirectory / L"Strings";

	Source::Strings = ModDirectory / L"Languages" / L"English" / L"Strings";
	Source::HasStrings = fs::exists(Source::Strings) && fs::is_directory(Source::Strings);
}

fs::path Path::RelativeToLang(fs::path const& hPath) noexcept
{
	static std::error_code ec{};

	return fs::relative(hPath, TargetLangDirectory, ec);
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

//	Get all english texts
//	Generate current CRC
//	Get all CRC in record
//	Build a library of all loc XMLs
//	Get all existing translations
//	Remove altered entries from existing translations.

[[nodiscard]]
static generator<fs::path> GetAllXmlSourceFiles(fs::path const& hModFolder) noexcept
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
	if (auto const hKeyedEntry = hModFolder / L"Languages" / L"English" / L"Keyed"; fs::exists(hKeyedEntry))
	{
		for (auto&& hPath :
			fs::recursive_directory_iterator(hKeyedEntry)
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
	fs::path const& DefInjected = Path::TargetLangDefInjected
) noexcept
{
	// #TODO no global vars involved?

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
					DefInjected / GetClassFolderName(*pClassInfo) / szFileName,
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
						DefInjected / GetClassFolderName(*pClassInfo) / szFileName,
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
					li
				);
			}
		}

		// Default: Do nothing. This is not a field that can be translated.
	}
}

[[nodiscard]]
static recursive_generator<translation_t> ExtractAllEntriesFromFile(fs::path const& file, fs::path const& Keyed = Path::TargetLangKeyed) noexcept
{
	XMLDocument xml;
	xml.LoadFile(file.u8string().c_str());

	auto const szFileName = file.filename().native();

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
			co_yield{
				Keyed / szFileName,
				entry->Name(), entry->GetText(),
			};
		}
	}
}

[[nodiscard]]
static recursive_generator<translation_t> GetAllTranslationEntries(fs::path const& hModFolder) noexcept
{
	for (auto&& file : GetAllXmlSourceFiles(hModFolder))
		co_yield ExtractAllEntriesFromFile(file);
}

[[nodiscard]]
static sorted_loc_view_t GetSortedLocView(span<translation_t const> source = gAllSourceTexts) noexcept
{
	sorted_loc_view_t ret{};

	for (auto&& [hPath, szEntry, szWords] : source)
		ret[hPath.native()].emplace_back(szEntry, szWords);

	return ret;
}

static void CollectAllLocFiles(xmls_t* pret = &gAllLocFiles, sorted_loc_view_t const& SortedLocView = gSortedSourceTexts) noexcept
{
	auto& ret = *pret;

	for (auto&& wcsPath : SortedLocView | std::views::keys)
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
	}
}

static void GenerateCRC(span<translation_t const> source = gAllSourceTexts, crc_dict_t_2* pret = &gCurCRC, bool bCheckStringFillers = Path::Source::HasStrings, fs::path const& StringFillerFolder = Path::Source::Strings) noexcept
{
	for (auto&& elem : source)
	{
		auto&& [iter, bNewEntry] = pret->try_emplace(
			elem.GetCrcIdentifier(),
			CRC64::CheckStream((std::byte*)elem.m_Text.data(), elem.m_Text.size())
		);

		if (!bNewEntry)
			fmt::print(Style::Warning, "[Warning] Duplicated record for {}::{}", elem.m_TargetFile.filename().u8string(), elem.m_Identifier);
	}

	if (bCheckStringFillers)
	{
		for (auto&& hPath :
			fs::recursive_directory_iterator(StringFillerFolder)
			| std::views::filter([](auto&& entry) noexcept { return !entry.is_directory(); })
			| std::views::transform(&fs::directory_entry::path)
			| std::views::filter([](auto&& path) noexcept { return path.has_extension() && _wcsicmp(path.extension().c_str(), L".txt") == 0; })
			)
		{
			auto ret = fs::relative(hPath, StringFillerFolder).u8string();
			CheckStringForXML(&ret);

			pret->try_emplace(
				std::move(ret),
				CRC64::CheckFile(hPath.u8string().c_str())
			);
		}
	}
}

static void LoadCRC(fs::path const& prev_records, crc_dict_t_2* pret = &gPrevCRC) noexcept
{
	auto const prev_records_str = prev_records.u8string();
	XMLDocument xml;

	if (auto err = xml.LoadFile(prev_records_str.c_str()); err != XML_SUCCESS) [[unlikely]]
		fmt::print(Style::Error, "XMLDocument::LoadFile returns: {1} ({0})\n", XMLDocument::ErrorIDToName(err), std::to_underlying(err));

	auto const Records = xml.FirstChildElement("Records");
	if (!Records) [[unlikely]]
	{
		fmt::print(Style::Warning, "Bad CRC record file: {}\n", prev_records_str);
		return;
	}

	for (auto i = Records->FirstChildElement(); i; i = i->NextSiblingElement())
	{
		auto&& [iter, bNewEntry] = pret->try_emplace(
			i->Name(),
			i->Unsigned64Attribute("CRC")
		);
	
		if (!bNewEntry) [[unlikely]]
			fmt::print(Style::Warning, "[Warning] Duplicated previous CRC record entry: '{}'", iter->first);
	}
}

static void SaveCRC(fs::path const& save_to, crc_dict_t_2 const& records = gCurCRC)
{
	XMLDocument xml;
	xml.InsertFirstChild(xml.NewDeclaration());
	xml.SetBOM(true);

	auto const Version = xml.NewElement("Version");
	xml.InsertEndChild(Version);
	Version->SetAttribute("Major", APP_VERSION.m_major);
	Version->SetAttribute("Minor", APP_VERSION.m_minor);
	Version->SetAttribute("Revision", APP_VERSION.m_revision);
	Version->SetAttribute("Build", APP_VERSION.m_build);
	Version->SetAttribute("Date", BUILD_NUMBER);


	auto const Records = xml.NewElement("Records");
	xml.InsertEndChild(Records);

	auto const t = fmt::gmtime(std::time(nullptr));
	Records->SetAttribute("GMT", true);
	Records->SetAttribute("Date", fmt::format("{:%Y-%m-%d}", t).c_str());
	Records->SetAttribute("Time", fmt::format("{:%H:%M:%S}", t).c_str());

	for (auto&& [key, value] : records)
		Records->InsertNewChildElement(key.c_str())->SetAttribute("CRC", value);

	xml.SaveFile(save_to.u8string().c_str());
}

void PrepareModData() noexcept
{
	if (gAllSourceTexts.empty())
		gAllSourceTexts = GetAllTranslationEntries(Path::ModDirectory) | std::ranges::to<vector>();

	if (gSortedSourceTexts.empty())
		gSortedSourceTexts = GetSortedLocView(gAllSourceTexts);

	GenerateCRC();
	LoadCRC("test.xml");
	SaveCRC(L"test.xml");
}