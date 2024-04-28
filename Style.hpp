#pragma once

#if !defined(FMT_COLOR_H_)
#include <fmt/color.h>
#endif

#ifndef EXPORT
#define EXPORT
#endif

EXPORT namespace Style
{
	inline constexpr auto Error = fmt::fg(fmt::color::red);
	inline constexpr auto Warning = fmt::fg(fmt::color::golden_rod);
	inline constexpr auto Info = fmt::fg(fmt::color::light_gray);
	inline constexpr auto Skipping = fmt::fg(fmt::color::gray);
	inline constexpr auto Name = fmt::emphasis::underline | fmt::fg(fmt::color::cyan);
	inline constexpr auto Action = fmt::emphasis::bold | fmt::fg(fmt::color::sky_blue);
	inline constexpr auto Positive = fmt::fg(fmt::color::lime_green);
	inline constexpr auto Debug = fmt::fg(fmt::color::chocolate);
	inline constexpr auto Conclusion = fmt::fg(fmt::color::dark_blue);
}
