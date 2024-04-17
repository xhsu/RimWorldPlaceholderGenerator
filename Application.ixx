module;

#include <stdint.h>

#include <bit>
#include <charconv>
#include <format>
#include <ranges>

export module Application;

constexpr auto LocalBuildNumber(void) noexcept
{
#define COMPILE_DATE __DATE__

	// #UPDATE_AT_CPP23 P2647R1
	constexpr auto today_m = std::string_view{ COMPILE_DATE, 3 };
	constexpr auto today_d = []() consteval { int d{}; std::from_chars(&COMPILE_DATE[4], &COMPILE_DATE[6], d); return d; }();
	constexpr auto today_y = []() consteval { int d{}; std::from_chars(&COMPILE_DATE[7], &COMPILE_DATE[11], d); return d; }();

	constexpr std::string_view mon[12] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	constexpr char mond[12] =
	{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

	int m = 0;
	int d = 0;
	int y = 0;

	for (auto&& [szMonth, iDayCount] : std::views::zip(mon, mond))
	{
		if (today_m == szMonth)
			break;

		d += iDayCount;
	}

	d += today_d - 1;
	y = today_y - 1900;

	auto m_nBuildNumber = d + static_cast<int>((y - 1) * 365.25);

	if (((y % 4) == 0) && m > 1)
	{
		m_nBuildNumber += 1;
	}

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
	constexpr uint32_t AsInt32() const noexcept
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
	.m_minor = 1,
	.m_revision = 2,
	.m_build = static_cast<uint8_t>(BUILD_NUMBER % 255),
};
