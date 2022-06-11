#include <okiidoku/puzzle/solver/cand_elim_find.hpp>

#include <okiidoku/puzzle/solver/found.hpp>
#include <okiidoku/o2_bit_arr.hpp>

// #include <algorithm>
#include <array>

#include <okiidoku/puzzle/solver/cand_elim_find.macros.hpp>

namespace okiidoku::mono::detail::solver { namespace {

	template<Order O> requires(is_order_compiled(O))
	struct ChuteIsecsSyms final {
		using T = Ints<O>;
		using o1i_t = int_ts::o1i_t<O>;
		using o2i_t = int_ts::o2i_t<O>;
		// inner dimension is for intersections in a line, outer dimension for intersections in a box.
		[[nodiscard, gnu::pure]] const O2BitArr<O>& at_isec(const o2i_t isec_i) const noexcept { return arr_[isec_i]; }
		[[nodiscard, gnu::pure]]       O2BitArr<O>& at_isec(const o2i_t isec_i)       noexcept { return arr_[isec_i]; }
		[[nodiscard, gnu::pure]] const O2BitArr<O>& at_isec(const o1i_t box_isec_i, const o1i_t line_isec_i) const noexcept { return arr_[(T::O1*box_isec_i)+line_isec_i]; }
		[[nodiscard, gnu::pure]]       O2BitArr<O>& at_isec(const o1i_t box_isec_i, const o1i_t line_isec_i)       noexcept { return arr_[(T::O1*box_isec_i)+line_isec_i]; }
	private:
		std::array<O2BitArr<O>, T::O2> arr_ {};
	};

	template<Order O> requires(is_order_compiled(O))
	[[nodiscard]] bool find_locked_cands_and_check_needs_unwind(
		const CandsGrid<O>& cells_cands,
		FoundQueues<O>& found_queues
	) noexcept {
		OKIIDOKU_CAND_ELIM_FINDER_TYPEDEFS
		// for intersection I of block B and line L, and symbol S,
		// if S's only candidate cells in L are in I, the same must hold true for B
		// if S's only candidate cells in B are in I, the same must hold true for L
		const auto line_type {LineType::row}; // TODO.asap add the outer loop to also try LineType::col
		for (o1i_t chute {0}; chute < T::O1; ++chute) {
			// TODO optimize by interleaving entries of syms_non_single and syms
			ChuteIsecsSyms<O> chute_isecs_syms_non_single {}; // syms with multiple cand cells in each chute isec
			ChuteIsecsSyms<O> chute_isecs_syms {}; // all cand syms in each chute isec
			for (o2i_t chute_isec {0}; chute_isec < T::O2; ++chute_isec) {
			for (o1i_t isec_cell {0}; isec_cell < T::O1; ++isec_cell) {
				const auto chute_cell {static_cast<o3i_t>((T::O1*chute_isec)+isec_cell)};
				const auto rmi {chute_cell_to_rmi<O>(line_type, chute, chute_cell)};
				const auto& cell_cands {cells_cands.at_rmi(rmi)};
				auto& seen_once {chute_isecs_syms.at_isec(chute_isec)};
				auto& seen_twice {chute_isecs_syms_non_single.at_isec(chute_isec)};
				seen_twice |= (seen_once & cell_cands);
				seen_once |= cell_cands;
			}}
			std::array<O2BitArr<O>, T::O1> lines_syms;
			std::array<O2BitArr<O>, T::O1> lines_syms_claiming_an_isec; lines_syms_claiming_an_isec.fill(O2BitArr_ones<O>);
			std::array<O2BitArr<O>, T::O1> boxes_syms;
			std::array<O2BitArr<O>, T::O1> boxes_syms_claiming_an_isec; boxes_syms_claiming_an_isec.fill(O2BitArr_ones<O>);
			for (o1i_t isec_i {0}; isec_i < T::O1; ++isec_i) {
				for (o1i_t isec_j {0}; isec_j < T::O1; ++isec_j) {{
					const auto& line_isec_syms {chute_isecs_syms.at_isec(isec_i, isec_j)};
					lines_syms_claiming_an_isec[isec_i].remove(lines_syms[isec_i] & line_isec_syms);
					lines_syms[isec_i] |= line_isec_syms;
					}{
					const auto& box_isec_syms {chute_isecs_syms.at_isec(isec_j, isec_i)};
					boxes_syms_claiming_an_isec[isec_i].remove(boxes_syms[isec_i] & box_isec_syms);
					boxes_syms[isec_i] |= box_isec_syms;
				}}
			}
			for (o1i_t box_isec {0}; box_isec < T::O1; ++box_isec) {
			for (o1i_t line_isec {0}; line_isec < T::O1; ++line_isec) {
				const auto& isec_syms {chute_isecs_syms.at_isec(box_isec, line_isec)};
				auto box_minus_isec {boxes_syms[line_isec]}; box_minus_isec.remove(isec_syms);
				auto line_minus_isec {lines_syms[box_isec]}; line_minus_isec.remove(isec_syms);
				const auto& isec_syms_non_single {chute_isecs_syms_non_single.at_isec(box_isec, line_isec)};
				const auto line_match {lines_syms_claiming_an_isec[box_isec] & isec_syms_non_single & box_minus_isec};
				const auto box_match {boxes_syms_claiming_an_isec[line_isec] & isec_syms_non_single & line_minus_isec};
				const auto isec {static_cast<o3xs_t>((T::O2*chute)+(T::O1*box_isec)+line_isec)};
				if (line_match.count() > 0) [[unlikely]] {
					found_queues.push_back(found::LockedCands<O>{
						.syms{line_match},
						.isec{isec},
						.line_type{line_type},
						.remove_from_rest_of{BoxOrLine::box},
					});
				} else if (box_match.count() > 0) [[unlikely]] {
					found_queues.push_back(found::LockedCands<O>{
						.syms{box_match},
						.isec{isec},
						.line_type{line_type},
						.remove_from_rest_of{BoxOrLine::line},
					});
				}
			}}
		}
		return false;
	}
}}
namespace okiidoku::mono::detail::solver {

	OKIIDOKU_CAND_ELIM_FINDER_DEF(locked_cands)
	#undef OKIIDOKU_CAND_ELIM_FINDER_DEF

	#define OKIIDOKU_FOR_COMPILED_O(O_) \
		template UnwindInfo CandElimFind<O_>::locked_cands(Engine<O_>&) noexcept;
	OKIIDOKU_INSTANTIATE_ORDER_TEMPLATES
	#undef OKIIDOKU_FOR_COMPILED_O
}