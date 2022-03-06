#include "solvent_cli/repl.hpp"

#include "solvent_lib/morph/canon.hpp"
#include "solvent_lib/print.hpp"
#include "solvent_util/timer.hpp"
#include "solvent_util/str.hpp"

#include <iostream> // cout, endl,
#include <iomanip>  // setw,
#include <string>
#include <charconv>

namespace solvent::cli {

	namespace str = solvent::util::str;
	using namespace solvent::lib;

	constexpr std::string_view TERMINAL_OUTPUT_TIPS {
	"\nNote: You can run `tput rmam` in your shell to disable text wrapping."};


	Repl::Repl(const Order order_input): gen_(gen::Generator::create(order_input)) {
		const Order O = gen_->get_order();
		config_.order(O);
		if (O <= 4) { config_.verbosity(verbosity::Kind::Silent); }
		if (O  > 4) { config_.verbosity(verbosity::Kind::NoGiveups); }
		config_.path_kind(gen::path::Kind::RowMajor);
		config_.max_dead_ends(0);
	}


	void Repl::start(void) {
		const auto my_numpunct = new util::str::MyNumPunct;
		const auto pushed_locale = std::cout.imbue(std::locale(std::cout.getloc(), my_numpunct ));
		std::cout
		<< '\n' << str::dim.on << TERMINAL_OUTPUT_TIPS << str::dim.off
		<< '\n' << Command::HelpMessage
		<< std::endl;

		std::string command;
		do {
			std::cout << "\n[" << config_.order() << "]: ";

			std::getline(std::cin, command);
		} while (run_command(command));
		std::cout.imbue(pushed_locale);
		// delete my_numpunct; // TODO.learn why isn't this needed?
	}


	bool Repl::run_command(const std::string_view cmd_line) {
		size_t token_pos;
		// Very simple parsing: Assumes no leading spaces, and does not
		// trim leading or trailing spaces from the arguments substring.
		const std::string_view cmd_name = cmd_line.substr(0, token_pos = cmd_line.find(" "));
		const std::string_view cmd_args = (token_pos == std::string_view::npos)
			? "" :  cmd_line.substr(token_pos + 1, std::string_view::npos);
		const auto it = Command::Str2Enum.find(cmd_name);
		if (it == Command::Str2Enum.end()) {
			// No command name was matched.
			std::cout << str::red.on;
			std::cout << "command \"" << cmd_line << "\" not found."
				" enter \"help\" for the help menu.";
			std::cout << str::red.off << std::endl;
			return true;
		}
		switch (it->second) {
			using Command::E;
			case E::Help:
				std::cout
				<< Command::HelpMessage << str::dim.on
				<< '\n' << verbosity::options_menu_str
				<< '\n' << gen::path::options_menu_str
				<< str::dim.off << std::endl;
				break;
			case E::Quit:
				return false;
			case E::ConfigOrder:       config_.order(cmd_args); gen_ = gen::Generator::create(config_.order()); break; // TODO only call if order has changed
			case E::ConfigVerbosity:   config_.verbosity(cmd_args); break;
			case E::ConfigGenPath:     config_.path_kind(cmd_args); break;
			case E::ConfigMaxDeadEnds: config_.max_dead_ends(cmd_args); break;
			case E::Canonicalize:      config_.canonicalize(cmd_args); break;
			case E::GenSingle:     gen_single();     break;
			case E::GenContinue:   gen_single(true); break;
			case E::GenMultiple:   gen_multiple(cmd_args, false); break;
			case E::GenMultipleOk: gen_multiple(cmd_args, true); break;
		}
		return true;
	}


	void Repl::gen_single(const bool cont_prev) {
		const clock_t clock_start = std::clock();
		cont_prev
			? gen_->continue_prev()
			: gen_->operator()(gen::Params{
				.path_kind = config_.path_kind(),
				.max_dead_ends = config_.max_dead_ends(),
			});
		const double processor_time = (static_cast<double>(std::clock() - clock_start)) / CLOCKS_PER_SEC;
		{
			using val_t = gen::Generator::val_t;
			using dead_ends_t = gen::Generator::dead_ends_t;
			std::array<val_t, O4_MAX> grid;
			std::array<dead_ends_t, O4_MAX> dead_ends;
			gen_->write_to(std::span<val_t>(grid));
			gen_->write_dead_ends_to<dead_ends_t>(std::span(dead_ends));
			if (gen_->status() == gen::ExitStatus::Ok && config_.canonicalize()) {
				morph::canonicalize<val_t>(gen_->get_order(), std::span(grid)); // should we make a copy and print as a third grid image?
			}
			
			const std::vector<print::print_grid_t> grid_accessors {
				print::print_grid_t([&](std::ostream& _os, uint16_t coord) {
					_os << ' '; print::val_to_str(_os, config_.order(), grid[coord]);
				}),
				print::print_grid_t([&](std::ostream& _os, uint16_t coord) {
					const auto shade = gen::Generator::shaded_dead_end_stat(static_cast<dead_ends_t>(gen_->get_params().max_dead_ends), dead_ends[coord]);
					_os << shade << shade;
				}),
			};
			print::pretty(std::cout, config_.order(), grid_accessors);
		}
		std::cout << std::setprecision(4)
			<< "\nprocessor time: " << processor_time << " seconds"
			<< "\nnum operations: " << gen_->get_op_count()
			<< "\nmax dead ends:  " << gen_->get_most_dead_ends_seen()
			<< "\nstatus:         " << ((gen_->status() == gen::ExitStatus::Ok) ? "OK"
				: (gen_->status() == gen::ExitStatus::Abort) ? "ABORT" : "GENERATOR EXHAUSTED")
			;
		std::cout << std::endl;
	}


	void Repl::gen_multiple(
		const trials_t stop_after,
		const bool only_count_oks
	) {
		gen::batch::Params params{
			.gen_params {
				.path_kind = config_.path_kind(),
				.max_dead_ends = config_.max_dead_ends(),
			},
			.only_count_oks = only_count_oks,
			.stop_after = stop_after
		};
		const gen::batch::BatchReport batch_report = gen::batch::batch(config_.order(), params,
			[this](const gen::Generator& result) {
				using val_t = gen::Generator::val_t;
				std::array<val_t, O4_MAX> grid;
				result.write_to(std::span<val_t>(grid));
				if (result.status() == gen::ExitStatus::Ok && config_.canonicalize()) {
					morph::canonicalize<val_t>(result.get_order(), std::span(grid));
				}
				if ((config_.verbosity() == verbosity::Kind::All)
				|| ((config_.verbosity() == verbosity::Kind::NoGiveups) && (result.status() == gen::ExitStatus::Ok))
				) {
					print::text(std::cout, result.get_order(), grid);
					if (result.get_order() <= 4) {
						std::cout << '\n';
					} else {
						std::cout << std::endl; // always flush for big grids
					}
				} else if (config_.verbosity() == verbosity::Kind::Silent) {
					// TODO.impl print a progress bar
				}
			}
		);
		if (config_.verbosity() == verbosity::Kind::NoGiveups
			&& only_count_oks
			&& batch_report.total_anys == 0
		) {
			std::cout << str::red.on << "* all generations aborted" << str::red.off;
		}

		static const std::string seconds_units = std::string{} + str::dim.on + " seconds (with I/O)" + str::dim.off;
		std::cout << std::setprecision(4)
			<< "\nstop after:      " << params.stop_after
			<< "\nnum threads:     " << params.num_threads
			<< "\ngenerator path:  " << params.gen_params.path_kind
			<< "\npercent aborted: " << (batch_report.fraction_aborted * 100) << " %"
			<< "\nprocess time:    " << batch_report.time_elapsed.proc_seconds << seconds_units
			<< "\nwall-clock time: " << batch_report.time_elapsed.wall_seconds << seconds_units
			;

		// Print bins (work distribution):
		batch_report.print(std::cout, config_.order());

		if (batch_report.time_elapsed.wall_seconds > 10.0) {
			// Emit a beep sound if the trials took longer than ten processor seconds:
			std::cout << '\a' << std::flush;
		}
		std::cout << std::endl;
	}


	void Repl::gen_multiple(
		const std::string_view stop_after_str,
		const bool only_count_oks
	) {
		trials_t stop_by_value;
		const auto parse_result = std::from_chars(stop_after_str.begin(), stop_after_str.end(), stop_by_value);
		if (parse_result.ec == std::errc{}) {
			if (stop_by_value <= 0) {
				std::cout << str::red.on
					<< "please provide a non-zero, positive integer."
					<< str::red.off << std::endl;
				return;
			}
		} else {
			std::cout << str::red.on
				<< "could not convert \"" << stop_after_str << "\" to an integer."
				<< str::red.off << std::endl;
			return;
		}
		this->gen_multiple(stop_by_value, only_count_oks);
	}
}