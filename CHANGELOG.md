# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

## [0.4.0] - 2015-12-30
### Fixed
- Correct usage of `isatty` to enable color output
- Fix bug where an input byte would be skipped when restarting pattern search

### Changed
- Return 0 if at least one match is found, or 1 otherwise
- Improve pattern parsing error messages

### Added
- Added this change log

## [0.3.0] - 2015-12-20
### Added
- Support for recursive directory searching, via `-r` option

## [0.2.0] - 2015-11-25
### Added
- Support for string search patterns, via `-s` option

## [0.1.1] - 2015-11-10
### Fixed
- Fix building under Cygwin

## 0.1.0 - 2015-10-09
First versioned release


[Unreleased]: https://github.com/JonathonReinhart/bgrep/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/JonathonReinhart/bgrep/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/JonathonReinhart/bgrep/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/JonathonReinhart/bgrep/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/JonathonReinhart/bgrep/compare/v0.1.0...v0.1.1
