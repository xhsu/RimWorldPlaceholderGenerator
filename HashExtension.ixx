module;

#include <functional>
#include <tuple>

export module HashExtension;

export inline std::size_t HashCombine(auto&&... vals) noexcept
{
	// Ref: https://stackoverflow.com/questions/4948780/magic-number-in-boosthash-combine
	constexpr std::size_t UINT_MAX_OVER_PHI =
#ifdef _M_X64
		0x9e3779b97f4a7c16;
#else
		0x9e3779b9;
#endif

	std::size_t ret = 0;
	auto const functors = std::tuple{ std::hash<std::remove_cvref_t<decltype(vals)>>{}... };

	[&] <size_t... I>(std::index_sequence<I...>&&)
	{
		((ret ^= std::get<I>(functors)(vals) + UINT_MAX_OVER_PHI + (ret << 6) + (ret >> 2)), ...);
	}
	(std::index_sequence_for<decltype(vals)...>{});

	return ret;
}
