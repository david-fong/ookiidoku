#ifndef HPP_SOLVENT_LIB_GEN
#define HPP_SOLVENT_LIB_GEN

#include "./path.hpp"
#include "../grid.hpp"
#include "../size.hpp"

#include <random>
#include <ostream>
#include <string>
#include <array>
#include <optional>


// https://en.cppreference.com/w/cpp/language/friend#Template_friend_operators
// https://web.mst.edu/~nmjxv3/articles/templates.html
namespace solvent::lib::gen { template<solvent::Order O> class Generator; }
template<solvent::Order O> std::ostream& operator<<(std::ostream&, solvent::lib::gen::Generator<O> const&);


namespace solvent::lib::gen {

	//
	std::mt19937 Rng;

	struct Params {
		path::Kind path_kind;
		unsigned long max_backtracks;
	};

	// Container for a very large number.
	// number of operations taken to generate a solution by grid-order.
	using opcount_t = unsigned long long;

	enum class ExitStatus {
		Exhausted, Abort, Ok,
	};

	//
	template<Order O>
	class Generator final : public Grid<O> {
	 friend std::ostream& operator<< <O>(std::ostream&, Generator const& s);
	 public:
		using has_mask_t = typename size<O>::has_mask_t;
		using ord1_t = typename size<O>::ord1_t;
		using ord2_t = typename size<O>::ord2_t;
		using ord4_t = typename size<O>::ord4_t;

		// Note that this should always be smaller than opcount_t.
		using backtrack_t =
			// Make sure this can fit `DEFAULT_MAX_BACKTRACKS`.
			//typename std::conditional_t<(O < 4), std::uint_fast8_t,
			std::conditional_t<(O < 5), std::uint_fast16_t,
			std::conditional_t<(O < 6), std::uint_fast32_t,
			std::uint64_t
		>>;

		static constexpr ord1_t O1 = O;
		static constexpr ord2_t O2 = O*O;
		static constexpr ord4_t O4 = O*O*O*O;

		static constexpr backtrack_t DEFAULT_MAX_BACKTRACKS = [](){ const backtrack_t _[] = {
			0, 1, 3, 150, 1'125, 560'000, 1'000'000'000,
		}; return _[O]; }();

		//
		struct GenResult final {
			Params params;
			ExitStatus status = ExitStatus::Abort;
			ord4_t progress = 0;
			opcount_t op_count = 0;
			backtrack_t most_backtracks_seen = 0;
			std::array<ord2_t, O4> grid = {};
		};

		//
		class Tile final {
		 friend class Generator<O>;
		 public:
			void clear(void) noexcept {
				next_try_index = 0;
				value = O2;
			}
			[[gnu::pure]] bool is_clear(void) const noexcept {
				return value == O2;
			}
		 private:
			// Index into val_try_order_. If set to O2, backtrack next.
			ord2_t next_try_index;
			ord2_t value;
		};

	 public:
		Generator(void);

		// Pass std::nullopt to continue the previous run.
		[[gnu::hot]] GenResult generate(std::optional<Params>);
		GenResult get_gen_result() { return gen_result_; }

	 private:
		std::array<std::array<ord2_t, O2>, O2> val_try_order_;
		std::array<Tile, O4> values_; // indexed by progress
		std::array<has_mask_t, O2> rows_has_;
		std::array<has_mask_t, O2> cols_has_;
		std::array<has_mask_t, O2> blks_has_;
		std::array<backtrack_t, O4> backtracks_; // indexed by progress

		// clear fields and scramble val_try_order
		void init(void);
		[[gnu::hot]] PathDirection set_next_valid(ord4_t progress, ord4_t coord) noexcept;
		GenResult gen_result_;
	};
}
#endif