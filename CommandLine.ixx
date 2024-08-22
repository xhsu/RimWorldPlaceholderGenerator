module;

#define HYDROGENIUM_COMMAND_LINE_VER 20240428L


#include <algorithm>
#include <ranges>
#include <span>
#include <string_view>
#include <string>


export module CommandLine;


using std::span;
using std::string;
using std::string_view;


export constexpr bool IsVariadicArgument(string_view arg) noexcept
{
	return arg.ends_with("...") || arg.ends_with("...]");
}

export constexpr bool IsOptionalArgument(string_view arg) noexcept
{
	return !arg.empty()
		&& ((arg.front() == '[' && arg.back() == ']')
			|| IsVariadicArgument(arg));
}

export bool TextToBoolean(string_view sz) noexcept
{
	return
		_strnicmp(sz.data(), "true", 4) == 0
		|| _strnicmp(sz.data(), "yes", 3) == 0;
}

export constexpr bool CommandLineArgSanity(span<string_view const> expected) noexcept
{
	// 1. must be a valid array, and the command must starts with '-'
	if (expected.empty() || expected.front().empty() || expected.front()[0] != '-')
		return false;

	// 2. there shall be no required argument after optional arguments.
	auto optional_removed = expected
		| std::views::reverse
		| std::views::drop_while(&IsOptionalArgument) | std::views::reverse;

	// 3. only one variadic argument can be present, and it shall be the last argument
	auto const iVariadicArgCount = std::ranges::count_if(expected, &IsVariadicArgument);

	return
		(iVariadicArgCount == 0 || iVariadicArgCount == 1 && IsVariadicArgument(expected.back()))
		&& std::ranges::count_if(optional_removed, &IsOptionalArgument) == 0;
}

export extern "C++" bool CommandLineWrapper(string_view desc, span<string_view const> expected, span<string_view const> received, void(*pfn)(span<string_view const>)) noexcept;

export extern "C++" void CommandLineUnitTest() noexcept;
