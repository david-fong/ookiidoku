#include "solvent_cli/repl.hpp"

#include "solvent/morph/canon.hpp"
#include "solvent/print.hpp"
#include "solvent_util/timer.hpp"
#include "solvent_util/str.hpp"

#include <iostream> // cout, endl,
#include <iomanip>  // setw,
#include <string>
#include <charconv>

namespace solvent::cli {

	namespace str = solvent::util::str;

	constexpr std::string_view welcome_message {
		"\nNote: You can run `tput rmam` to disable terminal text wrapping."
		"\nThis CLI is not built for getting proper benchmark stats. Timer"
		"\nvalues reported should not be taken as such."
	};


	Repl::Repl(const Order order_input): gen_(gen::ss::Generator::create(order_input)) {
		const Order O = gen_->get_order();
		config_.order(O);
		config_.verbosity(verbosity::E::quiet);
	}


	void Repl::start() {
		std::cout
		<< '\n' << str::dim.on << welcome_message << str::dim.off
		<< '\n' << Command::helpMessage
		<< std::endl;

		std::string command;
		do {
			std::cout << "\n[" << config_.order() << "]: ";

			std::getline(std::cin, command);
		} while (run_command(command));
	}


	bool Repl::run_command(const std::string_view cmd_line) {
		size_t token_pos;
		// Very simple parsing: Assumes no leading spaces, and does not
		// trim leading or trailing spaces from the arguments substring.
		const std::string_view cmd_name = cmd_line.substr(0, token_pos = cmd_line.find(" "));
		const std::string_view cmd_args = (token_pos == std::string_view::npos)
			? "" :  cmd_line.substr(token_pos + 1, std::string_view::npos);
		const auto it = Command::enum_str_to_enum.find(cmd_name);
		if (it == Command::enum_str_to_enum.end()) {
			// No command name was matched.
			std::cout << str::red.on << "command \"" << cmd_line << "\" not found."
				" enter \"help\" for the help menu." << str::red.off << std::endl;
			return true;
		}
		switch (it->second) {
			using Command::E;
			case E::help:
				std::cout
				<< Command::helpMessage << str::dim.on
				<< '\n' << verbosity::options_menu_str
				<< str::dim.off << std::endl;
				break;
			case E::quit:
				return false;
			case E::config_order:       config_.order(cmd_args); gen_ = gen::ss::Generator::create(config_.order()); break; // TODO only call if order has changed
			case E::config_print_level: config_.verbosity(cmd_args); break;
			case E::config_auto_canonicalize: config_.canonicalize(cmd_args); break;
			case E::gen_single:   gen_single(); break;
			case E::gen_multiple: gen_multiple(cmd_args); break;
		}
		return true;
	}


	void Repl::gen_single() {
		const clock_t clock_start = std::clock();
		gen_->operator()();
		const double processor_time = (static_cast<double>(std::clock() - clock_start)) / CLOCKS_PER_SEC;
		{
			using val_t = gen::ss::Generator::val_t;
			std::array<val_t, O4_MAX> grid;
			gen_->write_to(std::span<val_t>(grid));
			if (config_.canonicalize()) {
				morph::canonicalize<val_t>(gen_->get_order(), std::span(grid)); // should we make a copy and print as a second grid image?
			}
			
			const std::array<print::print_grid_t, 1> grid_accessors {
				print::print_grid_t([&](std::ostream& _os, uint16_t coord) {
					_os << ' '; print::val_to_str(_os, config_.order(), grid[coord]);
				}),
			};
			print::pretty(std::cout, config_.order(), grid_accessors);
		}
		std::cout << std::setprecision(4)
			<< "\nprocessor time: " << processor_time << " seconds"
			// << "\nnum operations: " << gen_->get_op_count()
			;
		std::cout << std::endl;
	}


	void Repl::gen_multiple(const gen::ss::batch::trials_t stop_after) {
		gen::ss::batch::Params params {
			.stop_after = stop_after
		};
		const gen::ss::batch::BatchReport batch_report = gen::ss::batch::batch(config_.order(), params,
			[this](const gen::ss::Generator& result) {
				if ((config_.verbosity() == verbosity::E::full)) {
					using val_t = gen::ss::Generator::val_t;
					std::array<val_t, O4_MAX> grid;
					result.write_to(std::span<val_t>(grid));
					if (config_.canonicalize()) {
						morph::canonicalize<val_t>(result.get_order(), std::span(grid));
					}
					print::text(std::cout, result.get_order(), grid);
					std::cout << '\n';
					if (result.get_order() > 4) {
						std::cout.flush();
					}
				} else if (config_.verbosity() == verbosity::E::quiet) {
					// TODO.impl print a progress bar
				}
			}
		);

		static const std::string seconds_units = std::string{} + str::dim.on + " seconds (with I/O)" + str::dim.off;
		std::cout << std::setprecision(4)
			<< "\nstop after:      " << params.stop_after
			<< "\nnum threads:     " << params.num_threads
			<< "\nprocess time:    " << batch_report.time_elapsed.proc_seconds << seconds_units
			<< "\nwall-clock time: " << batch_report.time_elapsed.wall_seconds << seconds_units
			// Note: the timer will not include canonicalization time if verbosity is quiet
			;

		if (batch_report.time_elapsed.wall_seconds > 10.0) {
			// Emit a beep sound if the trials took longer than ten processor seconds:
			std::cout << '\a' << std::flush;
		}
		std::cout << std::endl;
	}


	void Repl::gen_multiple(const std::string_view stop_after_str) {
		gen::ss::batch::trials_t stop_by_value;
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
		this->gen_multiple(stop_by_value);
	}
}