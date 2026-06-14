# Changelog

All notable changes to Noctalia Greeter are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Virtual terminal switching: `Ctrl+Alt+F1`–`F12` switches VTs from the greeter
  (previously every key was forwarded to the UI with no VT handling).
- Keyboard reference and a troubleshooting section in the README.
- Configurable cursor theme, size and search path. The compositor now reads
  `cursor_theme`, `cursor_size` and `cursor_path` from `greeter.conf`, falling
  back to the `XCURSOR_THEME`, `XCURSOR_SIZE` and `XCURSOR_PATH` environment
  variables, then the wlroots defaults. Previously the cursor theme was
  hardcoded to the wlroots default.
- `programs.noctalia-greeter.settings.cursor.{theme,size,package}` options in the NixOS
  module, which inject the matching `XCURSOR_*` variables into the greetd
  session command (so they survive greetd's empty greeter environment) and add
  the theme package's `share/icons` to `XCURSOR_PATH`.

### Changed

- `greeter.conf` parsing now logs a warning (with the offending line number)
  when it encounters a malformed line, an empty key, an empty value, or a
  duplicate key, instead of skipping them silently.
- Unrecognized keys in `greeter.conf` are reported on load so typos are easier
  to spot.

### Fixed

- Fixed a dangling `string_view` in the `greeter.conf` value parser that could
  read freed memory when unquoting values.

## [1.0.0]

### Added

- Full keyboard navigation: `Tab`/`Shift+Tab` focus ring, arrow-key movement,
  `F3`/`F7` session and color-scheme pickers, and `Enter`/`Space`/`Esc`.
- Separate `default_session` and last-used `session` preferences in
  `greeter.conf`.
- Appearance sync with Noctalia Shell v5 through the
  `noctalia-greeter-apply-appearance` polkit helper.
- greetd-user install setup (`setup_greeter_system.sh`).
