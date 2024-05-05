module;

#define HYDROGENIUM_APPLICATION_VERMGR_VER 20240505L

#include <stdint.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <format>
#include <ranges>

export module Application;

constexpr auto LocalBuildNumber() noexcept
{
#define COMPILE_DATE __DATE__

	constexpr std::string_view mon[12] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	constexpr uint8_t mond[12] =
	{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

	// #UPDATE_AT_CPP23 P2647R1
	constexpr auto today_m = std::string_view{ COMPILE_DATE, 3 };
	constexpr auto today_d = (COMPILE_DATE[5] - '0') + (COMPILE_DATE[4] != ' ' ? (COMPILE_DATE[4] - '0') : 0) * 10;
	constexpr auto today_y = (COMPILE_DATE[10] - '0') + (COMPILE_DATE[9] - '0') * 10 + (COMPILE_DATE[8] - '0') * 100 + (COMPILE_DATE[7] - '0') * 1000;

	constexpr auto this_leap = std::chrono::year{ today_y }.is_leap();

	constexpr auto m = std::ranges::find(mon, today_m) - std::ranges::begin(mon) + 1;	// "Jan" at index 0
	constexpr auto d = std::ranges::fold_left(mond | std::views::take(m - 1), today_d - (this_leap ? 0 : 1), std::plus<>{});
	constexpr auto y = today_y - 1900;

	auto m_nBuildNumber = d + static_cast<decltype(d)>((y - 1) * 365.25);
	m_nBuildNumber -= 44277;	// Mar 24 2022

	return m_nBuildNumber;

#undef COMPILE_DATE
}

struct app_version_t final
{
	uint8_t m_major{};
	uint8_t m_minor{};
	uint8_t m_revision{};
	uint8_t m_build{};

	[[nodiscard]]
	std::string ToString() const noexcept
	{
		return std::format("{}.{}.{}.{}", m_major, m_minor, m_revision, m_build);
	}

	[[nodiscard]]
	constexpr uint32_t U32() const noexcept
	{
		return std::bit_cast<uint32_t>(*this);
	}

	[[nodiscard]]
	static constexpr app_version_t Parse(uint32_t i) noexcept
	{
		return std::bit_cast<app_version_t>(i);
	}
};

static_assert(sizeof(app_version_t) == sizeof(uint32_t));

export inline constexpr auto BUILD_NUMBER = LocalBuildNumber();

export inline constexpr app_version_t APP_VERSION
{
	.m_major = 1,
	.m_minor = 3,
	.m_revision = 0,
	.m_build = static_cast<uint8_t>(BUILD_NUMBER % 255),
};

export inline constexpr uint32_t APP_VERSION_COMPILED = APP_VERSION.U32();
