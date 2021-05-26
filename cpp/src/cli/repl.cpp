/* #include "./repl.hpp"

#include "../lib/gen/batch.hpp"
#include "../util/timer.hpp"
#include "../util/ansi.hpp"

#include <iostream> // cout, endl,
#include <iomanip>  // setw,
#include <cmath>    // pow,

namespace solvent::cli {

	const std::string TABLE_SEPARATOR = "\n+-----------+----------+----------------+-----------+-----------+";
	const std::string TABLE_HEADER    = "\n|  bin bot  |   hits   |   operations   |  giveup%  |  speedup  |";


	namespace ansi = solvent::util::ansi;
	using namespace solvent::lib;

	// Mechanism to statically toggle printing alignment:
	// (#undef-ed before the end of this namespace)
	#define STATW_I << std::setw(this->gen.STATS_WIDTH)
	#define STATW_D << std::setw(this->gen.STATS_WIDTH + 4)

	const std::string TERMINAL_OUTPUT_TIPS =
	"\nNote: You can run `tput rmam` in your shell to disable text wrapping."
	"\nIf UTF-8 characters are garbled on Windows, run `chcp.com 65001`.";


	template<Order O>
	Repl<O>::Repl(std::ostream& os):
		gen(os),
		os(os)
	{
		set_output_level(verbosity::Kind::All);

		// Print diagnostics about Generator member size:
		std::cout
		<< "\nsolver obj size: " << sizeof(gen) << " bytes"
		<< "\ndefault genpath: " << gen.get_path_kind();
		if constexpr (O > 3) {
			std::cout << '\n' << ansi::DIM.ON << TERMINAL_OUTPUT_TIPS << ansi::DIM.OFF;
		}
		std::cout << std::endl;

		// Print help menu and then start the REPL (read-execute-print-loop):
		std::cout << Command::HelpMessage << std::endl;
		std::string command;
		do {
			std::cout << PROMPT;
			std::getline(std::cin, command);
		} while (run_command(command));
	}


	template<Order O>
	bool Repl<O>::run_command(std::string const& cmd_line) {
		size_t token_pos;
		// Very simple parsing: Assumes no leading spaces, and does not
		// trim leading or trailing spaces from the arguments substring.
		const std::string cmd_name = cmd_line.substr(0, token_pos = cmd_line.find(" "));
		const std::string cmd_args = (token_pos == std::string::npos)
			? "" :  cmd_line.substr(token_pos + 1, std::string::npos);
		const auto it = Command::Str2Enum.find(cmd_name);
		if (it == Command::Str2Enum.end()) {
			// No command name was matched.
			std::cout << ansi::RED.ON;
			std::cout << "command \"" << cmd_line << "\" not found."
				" enter \"help\" for the help menu.";
			std::cout << ansi::RED.OFF << std::endl;
			return true;
		}
		switch (it->second) {
			using Command::E;
			case E::Help:
				std::cout
				<< Command::HelpMessage << ansi::DIM.ON
				<< '\n' << verbosity::OPTIONS_MENU
				<< '\n' << gen::path::OPTIONS_MENU
				<< ansi::DIM.OFF << std::endl;
				break;
			case E::Quit:
				return false;
			case E::ConfigVerbosity:   set_output_level(cmd_args); break;
			case E::ConfigGenPath:    gen.set_path_kind(cmd_args); break;
			case E::RunSingle:     run_single();     break;
			case E::ContinuePrev:  run_single(true); break;
			case E::RunMultiple:   run_multiple(cmd_args, trials::StopAfterWhat::Any);    break;
			case E::RunMultipleOk: run_multiple(cmd_args, trials::StopAfterWhat::Ok); break;
		}
		return true;
	}


	template<Order O>
	verbosity::Kind Repl<O>::set_output_level(verbosity::Kind new_output_level) {
		const verbosity::Kind old_output_level = this->output_level;
		this->output_level = new_output_level;
		return old_output_level;
	}


	template<Order O>
	verbosity::Kind Repl<O>::set_output_level(std::string const& new_output_level_str) {
		std::cout << "\noutput level is ";
		if (new_output_level_str.empty()) {
			std::cout << "currently set to: " << get_output_level() << std::endl;
			return get_output_level();
		}
		for (unsigned i = 0; i < verbosity::size; i++) {
			if (new_output_level_str.compare(verbosity::NAMES[i]) == 0) {
				if (verbosity::Kind{i} == get_output_level()) {
					std::cout << "already set to: ";
				} else {
					std::cout << "now set to: ";
					set_output_level(verbosity::Kind{i});
				}
				std::cout << get_output_level() << std::endl;
				return get_output_level();
			}
		}
		// unsuccessful return:
		std::cout << get_output_level() << " (unchanged).\n"
			<< ansi::RED.ON << '"' << new_output_level_str
			<< "\" is not a valid output level name.\n"
			<< verbosity::OPTIONS_MENU << ansi::RED.OFF << std::endl;
		return get_output_level();
	}


	template<Order O>
	path::Kind Repl<O>::set_path_kind(const Path::E new_path_kind) noexcept {
		if (new_path_kind == get_path_kind()) {
			// Short circuit:
			return get_path_kind();
		}
		const Path::E prev_path_kind = get_path_kind();
		path_kind = new_path_kind;
		return prev_path_kind;
	}


	template<Order O>
	Path::E Repl<O>::set_path_kind(std::string const& new_path_kind_str) noexcept {
		std::cout << "\ngenerator path is ";
		if (new_path_kind_str.empty()) {
			std::cout << "currently set to: " << get_path_kind() << std::endl;
			return get_path_kind();
		}
		for (unsigned i = 0; i < Path::NUM_KINDS; i++) {
			if (new_path_kind_str.compare(Path::NAMES[i]) == 0) {
				if (Path::E{i} == get_path_kind()) {
					std::cout << "already set to: ";
				} else {
					std::cout << "now set to: ";
					set_path_kind(Path::E{i});
				}
				std::cout << get_path_kind() << std::endl;
				return get_path_kind();
			}
		}
		// unsuccessful return:
		std::cout << get_path_kind() << " (unchanged).\n"
			<< ansi::RED.ON << '"' << new_path_kind_str
			<< "\" is not a valid generator path name.\n"
			<< Path::OPTIONS_MENU << ansi::RED.OFF << std::endl;
		return get_path_kind();
	}

	template<Order O>
	void Repl<O>::print_msg_bar(
		std::string const& msg,
		unsigned bar_length,
		const char fill_char
	) const {
		if (bar_length < msg.length() + 8) {
			bar_length = msg.length() + 8;
		}
		std::string bar(bar_length, fill_char);
		if (!msg.empty()) {
			bar.replace(4, msg.length(), msg);
			bar.at(3) = ' ';
			bar.at(4 + msg.length()) = ' ';
		}
		os << '\n' <<bar;
	}


	template<Order O>
	void Repl<O>::run_single(const bool cont_prev) {
		gen.print_msg_bar("START");

		// Generate a new solution:
		const clock_t clock_start = std::clock();
		gen.generate(cont_prev);
		const double processor_time = ((double)(std::clock() - clock_start)) / CLOCKS_PER_SEC;

		os << "\nprocessor time: " STATW_D << processor_time << " seconds";
		os << "\nnum operations: " STATW_I << gen.gen_result.get_op_count();
		os << "\nmax backtracks: " STATW_I << gen.get_most_backtracks();
		if (!gen.is_pretty) gen.print_msg_bar("", '-');
		gen.print();
		gen.print_msg_bar((gen.gen_result.get_exit_status() == gen::ExitStatus::Ok) ? "DONE" : "ABORT");
		gen.gen_result = generator_t::GenResult { exit_status: 1 };
		os << std::endl;
	}


	template<Order O>
	void Repl<O>::run_multiple(
		const trials_t stop_after,
		const trials::StopAfterWhat trials_stop_method
	) {
		const unsigned COLS = [this](){ // Never zero. Not used when writing to file.
			const unsigned term_cols = GET_TERM_COLS();
			const unsigned cols = (term_cols-7)/(gen.O4+1);
			return term_cols ? (cols ? cols : 1) : ((unsigned[]){0,64,5,2,1,1,1})[O];
		}();
		const unsigned BAR_WIDTH = (gen.O4+1) * COLS + (gen.is_pretty ? 7 : 0);
		// Note at above: the magic number `7` is the length of the progress indicator.

		// NOTE: The last bin is for trials that do not succeed.
		std::array<trials_t, trials::NUM_BINS+1> bin_hit_count = {0,};
		std::array<double,   trials::NUM_BINS+1> bin_ops_total = {0,};

		gen.print_msg_bar("START x" + std::to_string(stop_after), BAR_WIDTH);
		// TODO use solvent::lib::gen::batch

		const std::string seconds_units = DIM_ON + " seconds (with I/O)" + DIM_OFF;
		gen.print_msg_bar("", BAR_WIDTH, '-'); os
		<< "\nhelper threads: " STATW_I << num_extra_threads
		<< "\ngenerator path: " STATW_I << gen.get_path_kind()
		// TODO [stats] For total successes and total trieals.
		<< "\nprocessor time: " STATW_D << proc_seconds << seconds_units
		<< "\nreal-life time: " STATW_D << wall_seconds << seconds_units;
		;
		if (wall_seconds > 10.0) {
			// Emit a beep sound if the trials took longer than ten processor seconds:
			std::cout << '\a' << std::flush;
		}
		// Print bins (work distribution):
		print_trials_work_distribution(total_trials, bin_hit_count, bin_ops_total);
		gen.print_msg_bar("DONE x" + std::to_string(stop_after), BAR_WIDTH);
		os << std::endl;
	}


	template<Order O>
	void Repl<O>::run_multiple(
		std::string const& trials_string,
		const trials::StopAfterWhat stop_by_method
	) {
		long stopByValue;
		try {
			stopByValue = std::stol(trials_string);
			if (stopByValue <= 0) {
				std::cout << ansi::RED.ON;
				std::cout << "please provide a non-zero, positive integer.";
				std::cout << ansi::RED.OFF << std::endl;
				return;
			}
		} catch (std::invalid_argument const& ia) {
			std::cout << ansi::RED.ON;
			std::cout << "could not convert \"" << trials_string << "\" to an integer.";
			std::cout << ansi::RED.OFF << std::endl;
			return;
		}
		run_multiple(static_cast<trials_t>(stopByValue), stop_by_method);
	}


	template<Order O>
	void Repl<O>::print_trials_work_distribution(
		BatchReport batch_report
	) {
		const std::string THROUGHPUT_BAR_STRING = "--------------------------------";


		os << trials::TABLE_SEPARATOR;
		os << trials::TABLE_HEADER;
		os << trials::TABLE_SEPARATOR;
		for (unsigned i = 0; i < bin_hit_count.size(); i++) {
			if (i == trials::NUM_BINS) {
				// Print a special separator for the giveups row:
				os << trials::TABLE_SEPARATOR;
			}
			// Bin Bottom column:
			const double bin_bottom  = (double)(i) * gen.GIVEUP_THRESHOLD / trials::NUM_BINS;
			if constexpr (O <= 4) {
				os << "\n|" << std::setw(9) << (int)(bin_bottom);
			} else {
				os << "\n|" << std::setw(8) << (int)(bin_bottom / 1'000.0) << 'K';
			}
			// Bin Hit Count column:
			os << "  |";
			if (bin_hit_count[i] == 0) os << DIM_ON;
			os << std::setw(8) << bin_hit_count[i];
			if (bin_hit_count[i] == 0) os << DIM_OFF;

			// Operation Count column:
			os << "  |";
			if (bin_hit_count[i] == 0) os << DIM_ON;
			os << std::setw(13) << unsigned(bin_ops_total[i] / ((O<5)?1:1000));
			os << ((O<5)?' ':'K');
			if (bin_ops_total[i] == 0) os << DIM_OFF;

			// Giveup Percentage column:
			os << "  |";
			os << std::setw(9) << (100.0 * (total_trials - successful_trials_accum_arr[i]) / total_trials);

			// Speedup Column
			os << "  |" << std::setw(9);
			if (i == trials::NUM_BINS) {
				os << "unknown";
			} else {
				//os << std::scientific << (throughput[i]) << std::fixed;
				os << 100.0 * (throughput[i] / throughput[trials::NUM_BINS-1]);
			}
			// Closing right-edge:
			os << "  |";

			// Print a bar to visualize throughput relative to tha
			// of the best. Note visual exaggeration via exponents
			// (the exponent value was chosen by taste / visual feel)
			const unsigned bar_length = THROUGHPUT_BAR_STRING.length()
				* std::pow(throughput[i] / throughput[best_throughput_bin], static_cast<int>(20.0/O));
			if (i != best_throughput_bin) os << DIM_ON;
			os << ' ' << THROUGHPUT_BAR_STRING.substr(0, bar_length);
			if (i != best_throughput_bin) os << DIM_OFF;
		}
		os << " <- current giveup threshold";
		os << trials::TABLE_SEPARATOR;
		os << DIM_ON << trials::THROUGHPUT_COMMENTARY << DIM_OFF;
	}

	#undef STATW_I
	#undef STATW_D

}
 */