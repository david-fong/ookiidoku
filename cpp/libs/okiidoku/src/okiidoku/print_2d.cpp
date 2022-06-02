#include <okiidoku/print_2d.hpp>
#include <okiidoku/print_2d.emoji.hpp>

#include <iostream>
#include <random> // minstd_rand
#include <algorithm>
#include <numeric> // iota
#include <vector>
#include <tuple>
#include <cassert>

namespace okiidoku { namespace {

	// current implementation is pretty simple (dumb?)
	std::vector<size_t> make_random_emoji_set(const Order O, const rng_seed_t rng_seed) noexcept {
		const unsigned O2 {O*O};
		const auto& prefs {emoji::top_set_preferences};
		std::vector<size_t> shuffled_sets(emoji::sets.size());
		std::iota(shuffled_sets.begin(), shuffled_sets.end(), size_t{0});
		{
			using rng_t = std::minstd_rand; // other good LCG parameters: https://arxiv.org/pdf/2001.05304v3.pdf
			rng_t rng {rng_seed};
			for (
				size_t b {0}, e_i_ {0}, e {prefs[e_i_]};
				b != prefs.back();
				b = e, ++e_i_, e = (e_i_ == prefs.size() ? emoji::sets.size() : prefs[e_i_])
			) {
				std::shuffle(
					std::next(shuffled_sets.begin(), static_cast<long>(b)),
					std::next(shuffled_sets.begin(), static_cast<long>(e)),
					rng
				);
			}
		}
		// first try to find a single set large enough:
		for (
			size_t b {0}, e_i_ {0}, e {prefs[e_i_]};
			e != prefs.back(); // unlike before, don't use anything not in top prefs list here.
			b = e, ++e_i_, e = prefs[e_i_]
		) {
			for (size_t i {b}; i < e; ++i) {
				if (emoji::sets.at(shuffled_sets.at(i)).entries.size() >= O2) {
					return {shuffled_sets.at(i)};
			}	}
		}
		// otherwise just return everything:
		return shuffled_sets;
	}
}}
namespace okiidoku {

	void print_2d_base( // NOLINT(readability-function-cognitive-complexity) :B
		const Order O,
		std::ostream& os,
		const rng_seed_t rng_seed,
		const std::span<const print_2d_grid_view> grid_views
	) noexcept {
		const unsigned O2 {O*O};
		using o2is_t = visitor::int_ts::o2is_t;
		using o4xs_t = visitor::int_ts::o4xs_t;

		const auto print_box_row_sep_string_ {[&os, O](const unsigned border_i) -> void {
			#define M_NOOK(NOOK_T, NOOK_C, NOOK_B) \
			if      (border_i == 0) [[unlikely]] { os << (NOOK_T); } \
			else if (border_i == O) [[unlikely]] { os << (NOOK_B); } \
			else                    { os << (NOOK_C); }
			M_NOOK(" ┌", " ├", " └")
			for (unsigned box_col {0}; box_col < O; ++box_col) {
				for (unsigned i {0}; i < 1U + (2U * O); ++i) {
					os << "─";
				}
				if (box_col < O - 1U) [[likely]] { M_NOOK("┬", "┼", "┴") }
			}
			M_NOOK("┐", "┤", "┘")
			#undef M_NOOK
		}};

		auto print_box_row_sep_strings {[&](const unsigned border_i) mutable {
			os << '\n';
			print_box_row_sep_string_(border_i);
			for (unsigned i {1}; i < grid_views.size(); ++i) {
				os << "   ";
				print_box_row_sep_string_(border_i);
			}
		}};
		const auto emoji_sets {make_random_emoji_set(O, rng_seed)};

		for (o2is_t row {0}; row < O2; ++row) {
			if (row % O == 0) [[unlikely]] {
				print_box_row_sep_strings(row / O);
			}
			os << '\n';
			for (unsigned grid_i {0}; grid_i < grid_views.size(); ++grid_i) {
				for (o2is_t col {0}; col < O2; ++col) {
					if ((col % O) == 0) [[unlikely]] { os << " │"; }

					auto val {size_t{grid_views[grid_i].operator()(static_cast<o4xs_t>((row * O2) + col))}};
					if (val == O2) {
						os << "  ";
						continue;
					}
					for (const auto emoji_set_index : emoji_sets) {
						const auto& set {emoji::sets.at(emoji_set_index).entries};
						if (val < set.size()) {
							os << set.at(val);
							break;
						}
						val -= set.size();
					}
				}
				os << " │";
				if (grid_i != grid_views.size() - 1) {
					os << "   ";
				}
			}
		}
		print_box_row_sep_strings(O);
	}
}