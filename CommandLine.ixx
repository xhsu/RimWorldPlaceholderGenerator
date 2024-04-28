module;

#define HYDROGENIUM_COMMAND_LINE_VER 20240428L

#include <assert.h>

#include <ranges>
#include <source_location>
#include <span>
#include <stacktrace>
#include <string_view>

#ifdef _DEBUG
#include <vector>
#endif

#include <fmt/color.h>
#include <fmt/ranges.h>

export module CommandLine;

import Style;

using std::span;
using std::string;
using std::string_view;

using namespace std::literals;


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

export constexpr bool CommandLineArgSanity(span<string_view const> expected) noexcept
{
	if (expected.empty() || expected.front().empty() || expected.front()[0] != '-')
		return false;

	auto optional_removed = expected
		| std::views::reverse
		| std::views::drop_while(&IsOptionalArgument) | std::views::reverse;

	auto const iVariadicArgCount = std::ranges::count_if(expected, &IsVariadicArgument);

	return
		(iVariadicArgCount == 0 || iVariadicArgCount == 1 && IsVariadicArgument(expected.back()))
		&& std::ranges::count_if(optional_removed, &IsOptionalArgument) == 0;
}

export bool CommandLineWrapper(string_view desc, span<string_view const> expected, span<string_view const> received, void(*pfn)(span<string_view const>)) noexcept
{
	auto const iReceivedArgCount = std::ssize(received);
	auto const iOptionalArgCount = std::ranges::count_if(expected, &IsOptionalArgument);
	auto const iExpectedArgCount = std::ssize(expected);
	auto const bIsVariadic = std::ranges::count_if(expected, &IsVariadicArgument) > 0;
	auto const diff = iExpectedArgCount - iReceivedArgCount;	// < 0: more arg than needed; > 0: less arg than needed

	assert(iReceivedArgCount > 0 && iExpectedArgCount > 0);
	assert(CommandLineArgSanity(expected));

	if (iReceivedArgCount <= 0 || expected.front() != received.front())	// not calling function if the command won't match
		return false;

	if ((diff < 0 && !bIsVariadic) || diff > iOptionalArgCount)
	{
		fmt::print(Style::Error, R"(Expected argument count for command "{}" is {}, but {} received.)" "\n", expected[0], iExpectedArgCount - 1, iReceivedArgCount - 1);

		string underscore{};

		fmt::print(Style::Info, "        [Desc] {}\n", desc);
		fmt::print(Style::Info, "        [Args] {} ", expected.front());
		underscore += string(fmt::formatted_size("        [Args] {} ", expected.front()), ' ');

		for (auto&& [ArgName, RecContent] :
			std::views::zip(expected | std::views::drop(1), received | std::views::drop(1)))
		{
			fmt::print(Style::Positive, "{}", ArgName);
			fmt::print(Style::Info, ": ");
			fmt::print(Style::Action, "{}", RecContent);
			fmt::print(Style::Info, ", ");

			underscore += string(fmt::formatted_size("{}: {}, ", ArgName, RecContent), ' ');
		}

		if (diff < 0)	// more arg than needed
		{
			for (auto&& RecContent : received | std::views::drop(iExpectedArgCount))
			{
				fmt::print(Style::Skipping, "<unexpected>");
				fmt::print(Style::Info, ": ");
				fmt::print(Style::Warning, "{}", RecContent);
				fmt::print(Style::Info, ", ");

				underscore += string(strlen("<unexpected>: "), '~');
				underscore += '^';
				underscore += string(RecContent.length() - 1, '~');
				underscore += "  ";	// for ", "
			}
		}
		else if (diff > 0)	// less arg than needed
		{
			for (auto&& ArgName : expected | std::views::drop(iReceivedArgCount))
			{
				bool const bOptionalArg = IsOptionalArgument(ArgName);
				string_view const szRejected{ bOptionalArg ? "<optional>" : "<required>" };

				fmt::print(bOptionalArg ? Style::Skipping : Style::Warning, "{}", ArgName);
				fmt::print(bOptionalArg ? Style::Skipping : Style::Info, ": ");
				fmt::print(Style::Skipping, "{}", szRejected);
				fmt::print(Style::Info, ", ");

				underscore += '^';
				underscore += string(fmt::formatted_size("{}: {}", ArgName, szRejected) - 1, '~');
				underscore += "  ";	// for ", "
			}
		}

		underscore += '\n';
		fmt::print(Style::Info, "\n{}", underscore);
		return false;
	}

	try
	{
		(*pfn)(received | std::views::drop(1));
	}
	catch (const std::exception& e)
	{
		auto const stack_trace = std::stacktrace::current();

		fmt::print(Style::Error, "[{}] Unhandled exception: {}\n", std::source_location::current().function_name(), e.what());
		fmt::print(Style::Info, "\tStack trace: {}\n", std::to_string(stack_trace));
	}
	catch (...)
	{
		auto const stack_trace = std::stacktrace::current();

		fmt::print(Style::Error, "[{}] Unhandled exception with unknown type.", std::source_location::current().function_name());
		fmt::print(Style::Info, "\tStack trace: {}\n", std::to_string(stack_trace));
	}

	return true;
}

export void CommandLineUnitTest() noexcept
{
#ifdef _DEBUG
	using std::vector;

	static constexpr string_view arg_sanity_check_0[] = { "sanity", };
	static constexpr string_view arg_sanity_check_1[] = { "-sanity", "arg1" };
	static constexpr string_view arg_sanity_check_2[] = { "-sanity", "arg1", "[opt1]" };
	static constexpr string_view arg_sanity_check_3[] = { "-sanity", "arg1", "[opt1]", "arg2" };
	static constexpr string_view arg_sanity_check_4[] = { "-sanity", "[var...]" };
	static constexpr string_view arg_sanity_check_5[] = { "-sanity", "arg1", "var1...", "[var2...]"};
	static constexpr string_view arg_sanity_check_6[] = { "-sanity", "arg1", "[var]...", "[opt1]" };
	static constexpr string_view arg_sanity_check_7[] = { "-sanity", "arg1", "[opt1]", "[var]..."};

	static_assert(!CommandLineArgSanity({}));
	static_assert(!CommandLineArgSanity(arg_sanity_check_0));
	static_assert(CommandLineArgSanity(arg_sanity_check_1));
	static_assert(CommandLineArgSanity(arg_sanity_check_2));
	static_assert(!CommandLineArgSanity(arg_sanity_check_3));
	static_assert(CommandLineArgSanity(arg_sanity_check_4));
	static_assert(!CommandLineArgSanity(arg_sanity_check_5));
	static_assert(!CommandLineArgSanity(arg_sanity_check_6));
	static_assert(CommandLineArgSanity(arg_sanity_check_7));


	auto do_nothing = +[](span<string_view const> a) noexcept { fmt::println("Test case passed: {}", a); };
	vector<string_view> expected_1{ "-test1", "arg1", "[opt1]", };
	vector<string_view> args_1_a{ "-test1", };
	vector<string_view> args_1_b{ "-test1", "arg1", };
	vector<string_view> args_1_c{ "-test1", "arg1", "opt1", };
	vector<string_view> args_1_d{ "-test1", "arg1", "opt1", "ext1", };

	//auto test_case_1 =
	//	[&](vector<string_view> const& args, string_view desc) noexcept
	//	{
	//		return CommandLineWrapper(desc, expected_1, args, do_nothing);
	//	};

	assert(!CommandLineWrapper("Lack of arg. Test expected to be failed.", expected_1, args_1_a, do_nothing));
	assert(CommandLineWrapper("Ignoring optional arg. Test expected to be passed.", expected_1, args_1_b, do_nothing));
	assert(CommandLineWrapper("Filling all args. Test expected to be passed.", expected_1, args_1_c, do_nothing));
	assert(!CommandLineWrapper("Overfeeding args. Test expected to be failed.", expected_1, args_1_d, do_nothing));

	vector<string_view> expected_2{ "-test2", "arg1", "[opt1]", "[var]..." };
	vector<string_view> args_2_a{ "-test2", };
	vector<string_view> args_2_b{ "-test2", "arg1", };
	vector<string_view> args_2_c{ "-test2", "arg1", "opt1", };
	vector<string_view> args_2_d{ "-test2", "arg1", "opt1", "any1", };
	vector<string_view> args_2_e{ "-test2", "arg1", "opt1", "any1", "any2" };

	assert(!CommandLineWrapper("Lack of arg. Test expected to be failed.", expected_2, args_2_a, do_nothing));
	assert(CommandLineWrapper("Ignoring optional arg. Test expected to be passed.", expected_2, args_2_b, do_nothing));
	assert(CommandLineWrapper("Filling optional arg. Test expected to be passed.", expected_2, args_2_c, do_nothing));
	assert(CommandLineWrapper("Filling first variadic arg. Test expected to be passed.", expected_2, args_2_d, do_nothing));
	assert(CommandLineWrapper("Filling second variadic arg. Test expected to be passed.", expected_2, args_2_e, do_nothing));
#endif
}
