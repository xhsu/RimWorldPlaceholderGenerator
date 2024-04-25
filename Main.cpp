// RimWorldPlaceholderGenerator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Precompiled.hpp"
#include "Mod.hpp"

import Application;
import Style;

using namespace std::literals;

namespace fs = std::filesystem;

using std::array;
using std::string_view;

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


static void Execute(const char* path_to_mod, string_view target_lang) noexcept
{
	GetModClasses(path_to_mod, &gModClasses);
	gAllNamespaces.insert_range(gModClasses | std::views::values | std::views::transform(&class_info_t::m_Namespace));

	Path::Resolve(path_to_mod, target_lang);
	ProcessMod();
}

int main(int argc, char *argv[]) noexcept
{
	switch (argc)
	{
	case 1:
		fmt::print("No folder selected.\nHint: Drag and drop mod folder on this console app.\n");
		while (std::cin.get() != '\n') {}
		return EXIT_SUCCESS;

	case 2:
	{
		if (_stricmp(argv[1], "-version") == 0)
		{
			fmt::println("App version: {}", APP_VERSION.ToString());
			fmt::println("Compile at: {}", __DATE__);
			fmt::println("");

			return EXIT_SUCCESS;
		}

		fmt::print(Style::Info, "Selected mod path: {}\n", argv[1]);
		fmt::print("Input target language.\n");

		auto sz = ""s;

		for (char c = std::cin.get(); c != '\n'; c = std::cin.get())
			sz.push_back(c);

		if (sz.empty())
			return EXIT_SUCCESS;

		sz[0] = std::toupper(sz[0]);
		for (size_t i = 1; i < sz.size(); ++i)
			sz[i] = std::tolower(sz[i]);

		for (auto &&lang : g_rgszAutoCompleteLanguages)
		{
			if (lang.starts_with(sz))
			{
				sz = lang;
				break;
			}
		}

		fmt::print("Selected language: {}\n\n", sz);
#ifndef _DEBUG
		std::this_thread::sleep_for(1s);
#endif
		Execute(argv[1], sz);

#ifndef _DEBUG
		fmt::print(Style::Positive, "\nDONE.\nPress Enter to exit.");
		while (std::cin.get() != '\n') {}
#endif
		return EXIT_SUCCESS;
	}

	case 3:
		fmt::print(Style::Info, "Selected mod path: {}\n", argv[1]);
		fmt::print(Style::Info, "Selected language: {}\n\n", argv[2]);
#ifndef _DEBUG
		std::this_thread::sleep_for(1s);
#endif
		Execute(argv[1], argv[2]);

#ifndef _DEBUG
		fmt::print(Style::Positive, "\nDONE.\nPress Enter to exit.");
		while (std::cin.get() != '\n') {}
#endif
		return EXIT_SUCCESS;

	default:
		fmt::print("Too many arguments.\nExpected: 1 or 2, but {} received.\n", argc - 1);
		while (std::cin.get() != '\n') {}
		return EXIT_SUCCESS;
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
