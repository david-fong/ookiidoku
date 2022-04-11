#include <okiidoku/mono/morph/transform.hpp>
#include <okiidoku/mono/morph/canon_ties.hpp>
#include <okiidoku/mono/grid.hpp>
#include <okiidoku/traits.hpp>

#include <ranges>
#include <algorithm> // sort
#include <numeric>   // iota
#include <cassert>

namespace okiidoku::mono::morph {

	template<Order O>
	using line_map_t = Transformation<O>::line_map_t;


	template<Order O>
	requires (is_order_compiled(O))
	class CanonPlace final {
		using val_t = traits<O>::o2i_smol_t;
		using mapping_t = Transformation<O>::mapping_t;
		using o1i_t = traits<O>::o1i_t;
		using o2i_t = traits<O>::o2i_t;
		using o4i_t = traits<O>::o4i_t;

	public:
		static constexpr o1i_t O1 = O;
		static constexpr o2i_t O2 = O*O;
		static constexpr o4i_t O3 = O*O*O;
		static constexpr o4i_t O4 = O*O*O*O;

		// explicit CanonPlace(const grid_span_t<O> grid): src_grid{grid} {}

	private:
		// grid_span_t<O> src_grid;

		struct PolarState final {
			line_map_t<O> to_og {Transformation<O>::identity.row_map};
			TieLinks<O, 2> line_ties {};
			TieLinks<O, 1> chute_ties {};

			explicit constexpr PolarState() noexcept {
				line_ties.update([](auto a, auto b){
					return (a%O1) == (b%O1);
				});
			}
			bool has_ties() const { return line_ties.has_unresolved() || chute_ties.has_unresolved(); }

			void do_a_pass(grid_span_t<O> table);
		};
		static grid_arr_flat_t<O> make_table_for_a_pass(const grid_span_t<O> src_grid, bool is_transpose, const PolarState& row, const PolarState& col);

	public:
		static Transformation<O> do_it(const grid_span_t<O> src_grid);
	};


	template<Order O>
	grid_arr_flat_t<O> CanonPlace<O>::make_table_for_a_pass(
		const grid_span_t<O> src_grid,
		const bool is_transpose,
		const PolarState& row_state,
		const PolarState& col_state
	) {
		grid_arr_flat_t<O> table_arr; {
			const auto t {Transformation<O>{
				Transformation<O>::identity.label_map,
				row_state.to_og,
				col_state.to_og,
				is_transpose,
			}};
			t.inverted().apply_from_to(src_grid, table_arr);
		}

		GridSpan2D<O> table(table_arr);
		for (o2i_t row_i {0}; row_i < O2; ++row_i) {
			auto row = table[row_i];
			const auto& ortho {is_transpose ? row_state : col_state};
			// loop over orthogonal partially-resolved line ranges to normalize:
			for (const auto [t_begin, t_end] : ortho.line_ties) {
				std::sort(std::next(row.begin(), t_begin), std::next(row.begin(), t_end));
			}
			// loop over orthogonal partially-resolved chute ranges to normalize:
			{
				std::array<o1i_t, O1> resolve;
				std::iota(resolve.begin(), resolve.end(), 0);
				for (const auto t : ortho.chute_ties) {
					namespace v = std::views;
					std::ranges::sort(resolve | v::drop(t.begin_) | v::take(t.size()), [&](auto a, auto b){
						return std::ranges::lexicographical_compare(row.subspan(a*O1,O1), row.subspan(b*O1,O1));
					});
				}
				std::array<val_t, O2> copy;
				std::copy(row.begin(), row.end(), copy.begin());
				for (o1i_t i {0}; i < O1; ++i) {
					std::copy(
						std::next(copy.begin(), i*O1), std::next(copy.begin(), (i+1)*O1),
						std::next(row.begin(), i*O1)
					);
				}
			}
		}
		return table_arr;
	}


	template<Order O>
	void CanonPlace<O>::PolarState::do_a_pass(
		const grid_span_t<O> table_arr_
	) {
		GridSpan2D<O> table(table_arr_);

		std::array<mapping_t, O2> to_tied;
		std::iota(to_tied.begin(), to_tied.end(), 0);
		for (const auto tie : line_ties) {
			// note: intentionally do not skip ties here since updated table
			// rows could likely be used by chute tie resolution.

			// try to resolve tied line range:
			std::sort(
				std::next(to_tied.begin(), tie.begin_),
				std::next(to_tied.begin(), tie.end_),
				[&](auto a, auto b){ return std::ranges::lexicographical_compare(table[a], table[b]); }
			);
		}
		const auto chute_tie_data {[&](o2i_t chute) {
			namespace v = std::views;
			return to_tied | v::drop(chute*O1) | v::take(O1) | v::transform([&](auto i){ return v::common(table[i]); }) | v::join;
		}};
		// try to resolve tied chute ranges:
		for (const auto tie : chute_ties) {
			if (tie.size() == 1) [[likely]] { continue; }
			std::sort(
				std::next(to_tied.begin(), tie.begin_), // TODO.high THIS IS WRONG.
				std::next(to_tied.begin(), tie.end_),
				[&](auto a, auto b){ return std::ranges::lexicographical_compare(
					chute_tie_data(a), chute_tie_data(b)
				); }
			);
		}

		line_ties.update([&](auto a, auto b){
			return std::ranges::equal(table[to_tied[a]], table[to_tied[b]]);
		});
		chute_ties.update([&](o1i_t a, o1i_t b){
			return std::ranges::equal(chute_tie_data(a), chute_tie_data(b));
		});
		// TODO.high tie data for lines in chute currently are not updated after updates to chute ordering...

		{
			// update s.to_og:
			std::array<mapping_t, O2> tied_to_og;
			for (o2i_t i {0}; i < O2; ++i) {
				tied_to_og[i] = to_og[i/O1][i%O1];
			}
			for (o2i_t i {0}; i < O2; ++i) {
				to_og[i/O1][i%O1] = tied_to_og[to_tied[i]];
			}
		}
	}


	template<Order O>
	Transformation<O> CanonPlace<O>::do_it(const grid_span_t<O> src_grid) {
		PolarState row_state {};
		PolarState col_state {};

		while (row_state.has_ties() || col_state.has_ties()) {
			const auto old_row_state {row_state};
			const auto old_col_state {col_state};
			[&]{ auto table_ {make_table_for_a_pass(src_grid, false, old_row_state, old_col_state)}; row_state.do_a_pass(table_); }();
			[&]{ auto table_ {make_table_for_a_pass(src_grid, true,  old_row_state, old_col_state)}; col_state.do_a_pass(table_); }();

			if (  old_row_state.line_ties  == row_state.line_ties
				&& old_row_state.chute_ties == row_state.chute_ties
				&& old_col_state.line_ties  == col_state.line_ties
				&& old_col_state.chute_ties == col_state.chute_ties
			) [[unlikely]] {
				// TODO.mid stalemate... current design insufficient?
				break;
			}
			// polar state A has no chance for further resolution in the next round
			// if polar state B was unable to resolve any ties in this round.
		}

		Transformation<O> transformation {
			.label_map {Transformation<O>::identity.label_map},
			.row_map {row_state.to_og},
			.col_map {col_state.to_og},
			.transpose {false},
		};
		transformation = transformation.inverted();
		// TODO.high use two iota views mapped one to src_grid and one to transposed view and lexicographical compare. if transposed less, edit transformation and apply a transpose_only transformation to src_grid in place.
		transformation.apply_in_place(src_grid);
		assert(grid_follows_rule<O>(src_grid));
		return transformation;
	}


	template<Order O>
	requires (is_order_compiled(O))
	Transformation<O> canon_place(const grid_span_t<O> grid) {
		return CanonPlace<O>::do_it(grid);
	}


	#define M_OKIIDOKU_TEMPL_TEMPL(O_) \
		template class CanonPlace<O_>; \
		template Transformation<O_> canon_place<O_>(grid_span_t<O_>);
	M_OKIIDOKU_INSTANTIATE_ORDER_TEMPLATES
	#undef M_OKIIDOKU_TEMPL_TEMPL
}