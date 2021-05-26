
# Things To Do

## Higher Priority

- Move the run-multiple business logic to the lib folder.
- Make max_backtracks a parameter.
- Make more conditional types in the size class.
- Design some smarter backtracking.
- Take out all stream related things from gen.
  - Read about std::format and see if it has any use for printing.
  - Make dedicated repl commands for dumping results to a file.
- Scrap and rewrite the run-multiple info summary reporting.
- Test performance of
  - using `std::bitset` for `has_mask_t`.
  - using `usize_t` for O1 O2 O4 definitions.
- Decide what interfaces to support:
  - Probably best to start with just readline
  - Can look into ncurses in the future?
- C++20
  - std::bit_width possible usage in size.hpp
  - `typename` doesn't need to be specified as much.
  - `using enum`. Might want to wait for CLANG to support?
  - see what stringy things can now use constexpr strings.
- CLI
  - implement `-h` and `--help` CLI argument.
  - Make giveup thresholds settable via CLI, and allow setting to "default".
  - give a red message when trying to continue and nothing is left to be found.