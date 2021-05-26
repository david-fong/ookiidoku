#ifndef HPP_SOLVENT_LIB_SIZE
#define HPP_SOLVENT_LIB_SIZE

#include "../buildflag.hpp"

#include <type_traits>
#include <bit>
#include <cstdint>


namespace solvent {
	//
	using Order = std::uint8_t;
	constexpr Order MAX_REASONABLE_ORDER = 10u;

	// Note: when printing, make sure to cast uint8_t to int.
	template<Order O>
	struct size {
	 public:
		// mask width `order^2` bits.
		// order:  2   3   4   5   6   7   8   9  10  11
		// width:  4   9  16  25  36  49  64  81 100 121
		// round:  8  16  16  32  64  64  64 128 128 128
		using has_mask_t =
			std::conditional_t<(O < 3), std::uint_fast8_t,
			std::conditional_t<(O < 5), std::uint_fast16_t,
			std::conditional_t<(O < 6), std::uint_fast32_t,
			std::conditional_t<(O < 9), std::uint_fast64_t,
			unsigned long long
		>>>>; // std::bitset not used for performance reasons

		// uint range [0, order].
		// not used in any critical code, so it doesn't need to be fast type.
		using ord1_t = std::uint8_t;

		// uint range [0, order^2].
		// order:   2    3    4    5    6    7    8    9   10   11   12   13   14   15   16
		// length:  4    9   16   25   36   49   64   81  100  121  144  169  196  225  256
		// bits:    3    3    5    5    6    6    7    7    7    7    8    8    8    8    9
		using ord2_t =
			std::conditional_t<(O < 16), std::uint_fast8_t,
			std::uint_fast16_t
		>;

		// uint range [0, order^4].
		// order:   2    3    4    5     6     7     8     9     10
		// area:   16   81  256  625  1296  2401  4096  6561  10000
		// bits:    5    7    9    9    11    12    17    17     18
		using ord4_t =
			std::conditional_t<(O <   4), std::uint8_t,
			std::conditional_t<(O <   8), std::uint16_t,
			std::conditional_t<(O < 256), std::uint32_t,
			std::uint64_t
		>>>;
	};
}
#endif