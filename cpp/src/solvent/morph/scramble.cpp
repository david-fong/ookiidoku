#include "solvent/morph/scramble.hpp"
#include "solvent/rng.hpp"

#include <array>
#include <algorithm>   // shuffle, ranges::copy
#include <cassert>

namespace solvent::morph {

	template<Order O>
	requires (is_order_compiled(O))
	void scramble(const grid_span_t<O> grid) {
		using val_t = size<O>::ord2x_least_t;
		using ord1i_t = size<O>::ord1i_t;
		using ord2i_least_t = size<O>::ord2i_least_t;
		using ord2i_t = size<O>::ord2i_t;
		static constexpr ord1i_t O1 = O;
		static constexpr ord2i_t O2 = O*O;
		static constexpr typename size<O>::ord4i_t O4 = O*O*O*O;
		std::array<ord2i_least_t, O4> orig_grid;
		std::copy(grid.begin(), grid.end(), orig_grid.begin());

		std::array<val_t, O2> label_map;
		bool transpose = false;
		std::array<std::array<val_t, O1>, O1> row_map;
		std::array<std::array<val_t, O1>, O1> col_map;

		for (ord2i_t i {0}; i < O2; ++i) {
			label_map[i]        = static_cast<val_t>(i);
			row_map[i/O1][i%O1] = static_cast<val_t>(i);
			col_map[i/O1][i%O1] = static_cast<val_t>(i);
		}
		std::ranges::shuffle(label_map, shared_mt_rng_);
		// std::ranges::shuffle(row_map, shared_mt_rng_);
		// std::ranges::shuffle(col_map, shared_mt_rng_);
		// for (ord1i_t chute {0}; chute < O1; ++chute) {
		// 	std::ranges::shuffle(row_map[chute], shared_mt_rng_);
		// 	std::ranges::shuffle(col_map[chute], shared_mt_rng_);
		// }
		// transpose = static_cast<bool>(shared_mt_rng_() % 2);
		// TODO.high uncomment once canon_label seems to be working.
		
		for (ord2i_t row {0}; row < O2; ++row) {
			for (ord2i_t col {0}; col < O2; ++col) {
				ord2i_t mapped_row = row_map[row/O1][row%O1];
				ord2i_t mapped_col = col_map[col/O1][col%O1];
				if (transpose) { std::swap(mapped_row, mapped_col); }
				auto orig_label = orig_grid[(O2*row)+col];
				grid[(O2*mapped_row)+mapped_col] = orig_label == O2 ? O2 : label_map[orig_label];
			}
		}
		assert(is_sudoku_valid<O>(grid));
	}


	// Note: this is currently not used anywhere and has no explicit template expansions.
	template<class T>
	requires std::is_integral_v<T>
	void scramble(Order order, std::span<T> grid) {
		assert(is_order_compiled(order));
		assert(grid.size() >= order*order*order*order);
		switch (order) {
		#define M_SOLVENT_TEMPL_TEMPL(O_) \
			case O_: { \
				constexpr unsigned O4 = O_*O_*O_*O_; \
				using val_t = size<O_>::ord2i_least_t; \
				std::array<val_t,O4> grid_resize; \
				for (unsigned i {0}; i < O4; ++i) { grid_resize[i] = static_cast<val_t>(grid[i]); } \
				scramble<O_>(std::span(grid_resize)); \
				for (unsigned i {0}; i < O4; ++i) { grid[i] = static_cast<T>(grid_resize[i]); } \
				break; \
			}
		M_SOLVENT_INSTANTIATE_ORDER_TEMPLATES
		#undef M_SOLVENT_TEMPL_TEMPL
		}
	}


	#define M_SOLVENT_TEMPL_TEMPL(O_) \
		template void scramble<O_>(const grid_span_t<O_>);
	M_SOLVENT_INSTANTIATE_ORDER_TEMPLATES
	#undef M_SOLVENT_TEMPL_TEMPL
}