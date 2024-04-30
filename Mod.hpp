#pragma once

#include "CPPCLI.hpp"

#ifndef _FILESYSTEM_
#include <filesystem>
#endif

#ifndef _OPTIONAL_
#include <optional>
#endif

#ifndef _RANGES_
#include <ranges>
#endif



namespace Path
{
	using namespace std::filesystem;

	inline path ModDirectory;	// Dir

	namespace Lang
	{
		inline path CRC;	// File

		inline path Directory;	// Dir
		inline path DefInjected;// Dir
		inline path Keyed;		// Dir
		inline path Strings;	// Dir
	}

	namespace Source
	{
		inline std::optional<path> Keyed;	// Dir;
		inline std::optional<path> Strings;	// Dir;
	}

	void Resolve(std::string_view path_to_mod, std::string_view target_lang) noexcept;
	path RelativeToLang(path const& hPath) noexcept;
	void ClearDebugFiles() noexcept;
}

struct sv_iless_t final
{
	using is_transparent = int;

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	auto operator() (std::wstring_view lhs, std::wstring_view rhs) const noexcept
	{
		return _wcsnicmp(
			lhs.data(),
			rhs.data(),
			std::min(lhs.length(), rhs.length())
		) < 0;
	}

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	auto operator() (std::filesystem::path const& lhs, std::filesystem::path const& rhs) const noexcept
	{
		return _wcsnicmp(
			lhs.c_str(),
			rhs.c_str(),
			std::min(lhs.native().length(), rhs.native().length())
		) < 0;
	}
};

inline constexpr auto cast_to_sv = [](auto&&... args) /*#UPDATE_AT_CPP23 static*/ noexcept { return std::string_view{ std::forward<decltype(args)>(args)... }; };
inline constexpr auto as_string_view = std::views::transform(cast_to_sv);

#include "RimWorldClasses.hpp" //inline classinfo_dict_t gRimWorldClasses;
inline classinfo_dict_t gModClasses;
inline sv_set_t gAllNamespaces{ std::from_range, gRimWorldClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace) };
inline constexpr classinfo_dict_t const* ALL_DICTS[] = { &gRimWorldClasses, &gModClasses, };

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

extern void ProcessMod() noexcept;
extern void NoXRef() noexcept;
extern void FileMergingSuggestion(bool bShouldWrite) noexcept;
