# Changelog

## Unreleased

### Added

- Installable CMake targets for Serializer, ArgParser, Protocol, optional format backends, Communication, and experimental JSON-RPC. Missing optional dependencies now have actionable diagnostics and can be fetched on demand where supported.
- Standalone examples and downstream CMake integration checks.
- C++20 JSON runtime and compile-time benchmark samples with raw measurements, compiler commands, dependency versions, and fixed inputs where applicable.
- Binary V2 and RPC V1 fixed-frame regression vectors and explicit rejection of unknown frame versions.

### Changed

- Split the long Chinese and English READMEs into short entry pages and topic references.
- Document the current `nekoproto` namespace and preliminary API stability boundaries.
- Compare parsed JSON values in serializer tests so object member order and small floating-point formatting differences do not determine success.
- Declare C++20 as the minimum language standard for the public CMake targets.

No release tag is implied by this entry. Wire compatibility and RPC API stability remain under review.
