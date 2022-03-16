#include "solvent/gen/batch.hpp"
#include "solvent/gen/stochastic.hpp"
#include "solvent/morph/canon.hpp"
#include "solvent/morph/scramble.hpp"
#include "solvent/print.hpp"
#include "solvent/grid.hpp"
#include "solvent/rng.hpp"

#include "solvent_util/console_setup.hpp"

#include <iostream>  // cout,
#include <string>
#include <random>    // random_device,
#include <array>


// TODO experiment with effect of batching gen and then doing canon on that batch for perf
// TODO it should probably just return right away if it encounters any failure.
// returns the number of failures
template<solvent::Order O>
unsigned test_morph_O(const unsigned num_rounds) {
	using namespace solvent;
	constexpr unsigned O4 {O*O*O*O};
	std::cout << "\n\ntesting for order " << O << std::endl;
	// TODO: assert that paths are valid.

	unsigned int count_bad {0};
	for (unsigned round {0}; round < num_rounds; ) {
		gen::ss::GeneratorO<O> g {};
		g();

		std::array<typename solvent::size<O>::ord2x_t, O4> gen_grid;
		g.write_to_(std::span(gen_grid));
		morph::canonicalize<O>(gen_grid);

		std::array<typename solvent::size<O>::ord2x_t, O4> canon_grid = gen_grid;
		morph::scramble<O>(canon_grid);
		morph::canonicalize<O>(canon_grid);

		if (gen_grid != canon_grid) {
			++count_bad;
			// TODO
			std::clog << "\n!bad\n";
			print::text(std::clog, O, gen_grid);
			std::clog << "\n";
			print::text(std::clog, O, canon_grid);
			std::clog << "\n==========\n";
		} else {
			std::clog << ".";
		}
		++round;
	}
	std::clog.flush();
	std::cout << "\ncount bad: " << count_bad << " / " << num_rounds;
	if (count_bad > 0) {
		std::cerr << "\nerrors when testing order " << O;
	}
	return count_bad;
}


/**
*/
int main(const int argc, char const *const argv[]) {
	solvent::util::setup_console();

	std::uint_fast64_t srand_key;  // 1
	unsigned int num_rounds; // 2

	if (argc > 1 && !std::string(argv[1]).empty()) {
		srand_key = std::stoi(argv[1]);
	} else {
		srand_key = std::random_device()();
	}
	num_rounds = (argc > 2) ? std::stoi(argv[2]) : 1000;

	std::cout << "\nPARSED ARGUMENTS:"
	<< "\n- ARG 1 [[ srand key  ]] : " << srand_key
	<< "\n- ARG 2 [[ num rounds ]] : " << num_rounds
	<< std::endl;

	// Scramble the random number generators:
	solvent::seed_rng(srand_key);

	if (test_morph_O<3>(num_rounds)) {
		return 1;
	}
	if (test_morph_O<4>(num_rounds)) {
		return 1;
	}
	if (test_morph_O<5>(num_rounds)) {
		return 1;
	}
	if (test_morph_O<10>(num_rounds)) {
		return 1;
	}

	// std::cout << "\ntotal: " << solvent::gen::ss::total;
	// std::cout << "\ntrue_: " << solvent::gen::ss::true_ << std::endl;
	return 0;
}