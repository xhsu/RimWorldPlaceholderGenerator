#pragma once

// Because of the use of C++/CLI, Modules from C++20 cannot be used.

#ifndef _MAP_
#include <map>
#endif

#ifndef _SPAN_
#include <span>
#endif

#ifndef _STRING_VIEW_
#include <string_view>
#endif

#ifndef _STRING_
#include <string>
#endif

#ifndef _VECTOR_
#include <vector>
#endif

struct sv_less_t final
{
	using is_transparent = int;

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	constexpr auto operator() (std::string_view&& lhs, std::string_view&& rhs) const noexcept
	{
		return lhs < rhs;
	}

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	constexpr auto operator() (std::string_view const& lhs, std::string_view const& rhs) const noexcept
	{
		return lhs < rhs;
	}
};

struct class_info_t final
{
	struct objarr_info_t final
	{
		std::string m_ElemType{};
		std::string m_FieldName{};
	};

	std::string m_Namespace{};
	std::string m_Name{};
	std::string m_Base{};
	std::vector<std::string> m_MustTranslates{};
	std::vector<std::string> m_ArraysMustTranslate{};
	std::vector<objarr_info_t> m_ObjectArrays{};

	[[nodiscard]]
	inline std::string FullName() const noexcept
	{
		if (m_Namespace.empty())
			return m_Name;

		return m_Namespace + "." + m_Name;
	}
};

using classinfo_dict_t = std::map<std::string, class_info_t, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_map

inline constexpr char CLASSNAME_VERSE_DEF[] = "Verse.Def";

#include "RimWorldClasses.hpp"
//inline classinfo_dict_t gRimWorldClasses;
inline classinfo_dict_t gModClasses;
inline constexpr classinfo_dict_t const* ALL_DICTS[] = { &gRimWorldClasses, &gModClasses, };

[[nodiscard]]
extern class_info_t const* GetRootDefClassName(class_info_t const& info, std::span<classinfo_dict_t const*> dicts) noexcept;

[[nodiscard]]
extern classinfo_dict_t GetVanillaClassInfo(const wchar_t* rim_world_dll = LR"(D:\SteamLibrary\steamapps\common\RimWorld\RimWorldWin64_Data\Managed\Assembly-CSharp.dll)");

[[nodiscard]]
extern classinfo_dict_t GetModClasses(const char* path_to_mod);
