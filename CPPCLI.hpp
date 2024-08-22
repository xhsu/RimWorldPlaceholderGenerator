#pragma once

// Because of the use of C++/CLI, Modules from C++20 cannot be used.


#ifndef _COMPARE_
#include <compare>
#endif

#ifndef _FUNCTIONAL_
#include <functional>
#endif

#ifndef _MAP_
#include <map>
#endif

#ifndef _SET_
#include <set>
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

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	constexpr auto operator() (std::wstring_view&& lhs, std::wstring_view&& rhs) const noexcept
	{
		return lhs < rhs;
	}

	[[nodiscard]]	/*#UPDATE_AT_CPP23 static*/
	constexpr auto operator() (std::wstring_view const& lhs, std::wstring_view const& rhs) const noexcept
	{
		return lhs < rhs;
	}
};

using str_set_t = std::set<std::string, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_set
using sv_set_t = std::set<std::string_view, std::less<>>;
using dictionary_t = std::map<std::string, std::string, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_map
using dict_view_t = std::map<std::string_view, std::string_view, std::less<>>;

struct class_info_t final
{
	std::string m_Namespace{};
	std::string m_Name{};
	std::string m_Base{};
	str_set_t m_MustTranslates{};
	str_set_t m_ArraysMustTranslate{};
	dictionary_t m_ObjectArrays{};	// key: FieldName, value: FieldType
	dictionary_t m_Objects{};	// key: FieldName, value: FieldType

	[[nodiscard]]
	inline std::string FullName() const noexcept
	{
		if (m_Namespace.empty())
			return m_Name;

		return m_Namespace + "." + m_Name;
	}
};

using classinfo_dict_t = std::map<std::string, class_info_t, sv_less_t>;	// #UPDATE_AT_CPP23 std::flat_map

extern void GetModClasses(const char* path_to_mod, classinfo_dict_t* pret);
