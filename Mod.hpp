#pragma once

#include "CPPCLI.hpp"

#ifndef _FILESYSTEM_
#include <filesystem>
#endif

#ifndef _RANGES_
#include <ranges>
#endif



namespace Path
{
	using namespace std::filesystem;

	inline path ModDirectory;
	inline path TargetLangDirectory;
	inline path TargetLangDefInjected;
	inline path TargetLangKeyed;
	inline path TargetLangStrings;

	namespace Lang
	{
		inline path CRC;
	}

	namespace Source
	{
		inline path Strings;
		inline bool HasStrings = false;
	}

	void Resolve(std::string_view path_to_mod, std::string_view target_lang) noexcept;
	path RelativeToLang(path const& hPath) noexcept;
}

#include "RimWorldClasses.hpp" //inline classinfo_dict_t gRimWorldClasses;
inline classinfo_dict_t gModClasses;
inline sv_set_t gAllNamespaces{ std::from_range, gRimWorldClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace) };
inline constexpr classinfo_dict_t const* ALL_DICTS[] = { &gRimWorldClasses, &gModClasses, };

[[nodiscard]]
extern class_info_t const* GetRootDefClassName(class_info_t const& info, std::span<classinfo_dict_t const* const> dicts = ALL_DICTS) noexcept;

inline void CheckStringForXML(std::string* s) noexcept
{
	for (auto& c : *s)
	{
		if (c == '\\' || c == '/')
			c = '.';
		else if (std::isspace(c))
			c = '_';
	}
}