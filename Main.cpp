// RimWorldPlaceholderGenerator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Precompiled.hpp"
#include "Mod.hpp"

import Application;
import CommandLine;
import Style;

using namespace std::literals;

namespace fs = std::filesystem;

using std::array;
using std::pair;
using std::set;
using std::span;
using std::string;
using std::string_view;
using std::tuple;
using std::vector;

inline constexpr array g_rgszAutoCompleteLanguages =
{
	"Catalan"sv,
	"ChineseSimplified"sv,
	"ChineseTraditional"sv,
	"Czech"sv,
	"Danish"sv,
	"Dutch"sv,
	"Estonian"sv,
	"Finnish"sv,
	"French"sv,
	"German"sv,
	"Greek"sv,
	"Hungarian"sv,
	"Italian"sv,
	"Japanese"sv,
	"Korean"sv,
	"Norwegian"sv,
	"Polish"sv,
	"Portuguese"sv,
	"PortugueseBrazilian"sv,
	"Romanian"sv,
	"Russian"sv,
	"Slovak"sv,
	"Spanish"sv,
	"SpanishLatin"sv,
	"Swedish"sv,
	"Turkish"sv,
	"Ukrainian"sv,
};


static sv_set_t AutoCompletion(string_view langShort) noexcept
{
	auto const ToLower =
		+[](char c) noexcept
		{
			if (c <= 'Z' && c >= 'A')
				return static_cast<char>(c + 0x20);

			return c;
		};

	auto const auto_completion =
		[&](string_view const& lang) noexcept
		{
			return std::ranges::contains_subrange(lang, langShort, {}, ToLower, ToLower);
		};

	return {
		std::from_range,
		g_rgszAutoCompleteLanguages
		| std::views::filter(auto_completion)
	};
}

static void ShowVersion(span<string_view const> args) noexcept
{
	fmt::println("\nApp version: {}", APP_VERSION.ToString());
	fmt::println("Compile at: " __DATE__);

	if (!args.empty())
		fmt::println("\tVersion as uint32: {}", APP_VERSION_COMPILED);

	fmt::println("");
}

static __forceinline void Default(const char* path_to_mod, string_view target_lang) noexcept
{
	GetModClasses(path_to_mod, &gModClasses);
	gAllNamespaces.insert_range(gModClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace));

	Path::Resolve(path_to_mod, target_lang);
	ProcessMod();
}

static void Default(span<string_view const> args) noexcept
{
	return Default(args[0].data(), args[1]);
}

static void NoXRef(span<string_view const> args) noexcept
{
	auto& path_to_mod = args[0];
	auto& target_lang = args[1];

	GetModClasses(path_to_mod.data(), &gModClasses);
	gAllNamespaces.insert_range(gModClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace));

	Path::Resolve(path_to_mod, target_lang);
	NoXRef();
}

static void ClrDbg(span<string_view const> args) noexcept
{
	auto& path_to_mod = args[0];
	auto& target_lang = args[1];

	Path::Resolve(path_to_mod, target_lang);
	Path::ClearDebugFiles();
}

static void ClearConsole(span<string_view const>) noexcept
{
	system("cls");
}

static void XmlMerging(span<string_view const> args) noexcept
{
	auto& path_to_mod = args[0];
	auto& target_lang = args[1];
	bool bShouldWrite = args.size() < 3 || !TextToBoolean(args[2]);

	GetModClasses(path_to_mod.data(), &gModClasses);
	gAllNamespaces.insert_range(gModClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace));

	Path::Resolve(path_to_mod, target_lang);
	FileMergingSuggestion(bShouldWrite);
}

#pragma region Command line stuff
inline constexpr string_view ARG_DESC_HELP[] = { "-help" };
inline constexpr string_view ARG_DESC_VERSION[] = { "-version", "[bool:show_extra]", };
inline constexpr string_view ARG_DESC_CLRDBG[] = { "-clrdbg", "mod_dir", "target_lang", };
inline constexpr string_view ARG_DESC_NOXREF[] = { "-noxref", "mod_dir", "target_lang", };
inline constexpr string_view ARG_DESC_GENPH[] = { "-genph", "mod_dir", "target_lang", };
inline constexpr string_view ARG_DESC_CLR[] = { "-cls", };
inline constexpr string_view ARG_DESC_XMLMERG[] = { "-xmlmerg","mod_dir", "target_lang", "[bool:print_only]" };

extern void ShowHelp(span<string_view const>) noexcept;

// #UPDATE_AT_CPP26 span over initializer list
inline constexpr tuple<span<string_view const>, void(*)(span<string_view const>), string_view> CMD_HANDLER[] =
{
	{ ARG_DESC_HELP, &ShowHelp, "Show all commands of this application." },
	{ ARG_DESC_VERSION, &ShowVersion, "Display version of this application." },
	{ ARG_DESC_CLRDBG, &ClrDbg, "Clearing all files generated by this application under debug mode." },
	{ ARG_DESC_NOXREF, &NoXRef, "Finding all localization files which has no reference from current mod." },
	{ ARG_DESC_GENPH, &Default, "Generate English-based placeholders for a certain language." },
	{ ARG_DESC_CLR, &ClearConsole, "Clear the entire console output screen." },
	{ ARG_DESC_XMLMERG, &XmlMerging, "Merging possible misplaced xmls and their entries." },
};

void ShowHelp(span<string_view const>) noexcept
{
	fmt::print(Style::Info, "\n");

	constexpr auto max_len = std::ranges::max(
		CMD_HANDLER
		| std::views::elements<0>
		| std::views::transform(&span<string_view const>::front)
		| std::views::transform(&string_view::length)
	);

	for (auto&& [arg_desc, pfn, desc] : CMD_HANDLER)
	{
		fmt::print(Style::Action, "{}\n", desc);
		fmt::print(Style::Name, "\t{}", arg_desc[0]);
		fmt::print(Style::Info, "{}", string(max_len - arg_desc[0].length(), ' '));

		for (auto&& arg : arg_desc | std::views::drop(1))
			fmt::print(IsOptionalArgument(arg) ? Style::Skipping : Style::Info, " {}", arg);

		fmt::print(Style::Info, "\n\n");
	}
}

static_assert(
	// Compile-time sanity check - make our life easier.
	[]() consteval -> bool
	{
		for (auto&& arg_desc : CMD_HANDLER | std::views::elements<0>)
		{
			if (!CommandLineArgSanity(arg_desc))
				return false;
		}

		return true;
	}()
);
#pragma endregion Command line stuff

static void DragAndDropMode(span<string_view const> args) noexcept
{
	switch (args.size())
	{
	case 1:
	{
		fmt::print(Style::Info, "Selected mod path: {}\n", fmt::styled(args[0], Style::Name));
		fmt::print("Input target language.\n");

	LAB_SEL_LANG:;
		string sz{};
		for (char c = std::cin.get(); c != '\n'; c = std::cin.get())
			sz.push_back(c);

		if (sz.empty())
		{
		LAB_EMPTY:;
			fmt::print(Style::Warning, "Please input the target language.\n");
			goto LAB_SEL_LANG;
		}

		auto const possible_langs = AutoCompletion(sz);
		auto const possibilities = possible_langs.size();

		if (possibilities == 0)
			goto LAB_EMPTY;
		else if (possibilities > 1)
		{
			fmt::print(Style::Warning, "{} matches found.\nPlease specify your input.\n", possibilities);
			fmt::print(Style::Info, "\tPossible match: {}\n", fmt::join(possible_langs, ", "));
			goto LAB_SEL_LANG;
		}
		else
			sz = string{ *possible_langs.begin() };

		fmt::print("Selected language: {}\n\n", fmt::styled(sz, Style::Name));
#ifndef _DEBUG
		std::this_thread::sleep_for(1s);
#endif
		Default(args[0].data(), sz);

#ifndef _DEBUG
		fmt::print(Style::Positive, "\nDONE.\nPress Enter to exit.");
		while (std::cin.get() != '\n') {}
#endif
		break;
	}
	case 2:
		fmt::print(Style::Info, "Selected mod path: {}\n", fmt::styled(args[0], Style::Name));
		fmt::print(Style::Info, "Selected language: {}\n\n", fmt::styled(args[1], Style::Name));
#ifndef _DEBUG
		std::this_thread::sleep_for(1s);
#endif
		Default(args[0].data(), args[1]);

#ifndef _DEBUG
		fmt::print(Style::Positive, "\nDONE.\nPress Enter to exit.");
		while (std::cin.get() != '\n') {}
#endif
		break;

	default:
		fmt::print(Style::Error, "Too many arguments.\n");
		fmt::print(Style::Info, "Expected: 1 or 2, but {} received.\n", args.size());
		fmt::print(Style::Skipping, R"(	Expected arguments: "mod_dir" [lang])" "\n");

		while (std::cin.get() != '\n') {}
		break;
	}
}


int main(int argc, char *argv[]) noexcept
{
	//constexpr char const* arr[] =
	//{
	//	"location/app.exe",
	//	"arg1",
	//	"arg2",
	//	"-dir",
	//	"Test/1.3",
	//	"-lang",
	//	"English",
	//	"ChineseTraditional",
	//	"-noxref",
	//	"-clrdbg",
	//};

	auto const args =
		std::span(argv, (size_t)argc)
		| as_string_view
		| std::ranges::to<vector>();

	// take_while is not sizable
	// https://stackoverflow.com/questions/71092690/ranges-views-size-does-not-compile
	auto const uncommanded_arg_count = std::ranges::distance(
		args | std::views::take_while([](auto&& a) noexcept { return a[0] != '-'; })
	);

	auto const uncommand_args = span(	// they serve as Default() call.
		args | std::views::take(uncommanded_arg_count) | std::views::drop(1)	// the first one is always app path
	);

	if (uncommand_args.size())
	{
		// support for those don't know how to use command line.
		DragAndDropMode(uncommand_args);
	}

	// nothing further to do.
	if (uncommanded_arg_count >= argc)
	{
		if (argc == 1)	// directly executing application.
		{
			fmt::print(Style::Error, "No folder selected.\n");
			fmt::print(
				Style::Info,
				"Hint: {} and {} desired mod folder on this executable.\n",
				fmt::styled("Drag", Style::Action),
				fmt::styled("drop", Style::Action)
			);

			fmt::print(
				Style::Info,
				"Or execute this program with command line argument {} for extra information and functionality.\n",
				fmt::styled("-help", Style::Name)
			);

			while (std::cin.get() != '\n') {}
		}

		return EXIT_SUCCESS;
	}

	for (auto&& arg_list :
		args
		| std::views::drop(uncommanded_arg_count)	// the first one is always the path to this application.
		| std::views::chunk_by(+[](string_view const& lhs, string_view const& rhs) noexcept { return rhs[0] != '-'; }))
	{
		if (arg_list.empty() || arg_list.front().empty())
			continue;

		for (auto&& [arg_desc, pfn, desc] : CMD_HANDLER)
			if (CommandLineWrapper(desc, arg_desc, arg_list, pfn))
				break;
	}

	return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
