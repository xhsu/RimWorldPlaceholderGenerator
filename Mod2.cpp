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

struct trnsltn_t final
{
	fs::path m_TargetFile{};
	string m_Identifier{};
	string m_Text{};
};

void Path::Resolve(string_view path_to_mod, string_view target_lang) noexcept
{
	ModDirectory = fs::absolute(path_to_mod);

	TargetLangDirectory = ModDirectory / target_lang;
	TargetLangDefInjected = TargetLangDirectory / L"DefInjected";
	TargetLangKeyed = TargetLangDirectory / L"Keyed";
	TargetLangStrings = TargetLangDirectory / L"Strings";
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
		co_yield fs::absolute(hPath);
	}

	co_return;
}

[[nodiscard]]
static recursive_generator<trnsltn_t> ExtractAllEntriesFromObject(string szPrevIdentifier, string_view szTypeName, wstring_view szFileName, XMLElement* def) noexcept
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
					Path::TargetLangDefInjected / GetClassFolderName(*pClassInfo) / szFileName,
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
						Path::TargetLangDefInjected / GetClassFolderName(*pClassInfo) / szFileName,
						std::format("{}.{}", szThisIdentifier, idx), li->GetText(),
					};
				}
			}
		}

		// Case 3: this is an array of objects!
		else if (auto const iter = pClassInfo->m_ObjectArrays.find(szFieldName);
			iter != pClassInfo->m_ObjectArrays.cend())
		{
			uint_fast16_t idx = 0;

			for (auto li = field->FirstChildElement("li"); li; li = li->NextSiblingElement("li"), ++idx)
			{
				//li->SetName(std::to_string(idx).c_str());
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
static recursive_generator<trnsltn_t> ExtractAllEntriesFromFile(fs::path const& file) noexcept
{
	XMLDocument xml;
	xml.LoadFile(file.u8string().c_str());

	auto const szFileName = file.filename().native();

	for (auto defs = xml.FirstChildElement("Defs"); defs; defs = defs->NextSiblingElement("Defs"))
	{
		for (auto def = defs->FirstChildElement(); def; def = def->NextSiblingElement())
		{
			if (auto defName = def->FirstChildElement("defName"); defName)
				co_yield ExtractAllEntriesFromObject(defName->GetText(), def->Name(), szFileName, def);
		}
	}
}

void GetAllTranslationEntries(fs::path const& hModFolder) noexcept
{
	for (auto&& file : GetAllDefFiles(hModFolder))
	{
		for (auto&& TranslationEntry : ExtractAllEntriesFromFile(file))
		{
			fmt::println(
				"{} => {} == {}",
				Path::RelativeToLang(TranslationEntry.m_TargetFile).u8string(),
				TranslationEntry.m_Identifier, TranslationEntry.m_Text
			);
		}
	}
}