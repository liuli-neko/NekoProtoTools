# Argparser Tags Plan

This document tracks the reflection-based argparser tag surface. The goal is to keep the core parser small while making
common command-line interfaces pleasant to describe from metadata.

## Current Core Tags

- [x] `arg_name<Long, Short, BaseTags>`: overrides the reflected field name and optional short name.
- [x] `arg_help<Help, BaseTags>`: help text shown under the option.
- [x] `arg_default<Default, BaseTags>`: value applied when CLI input is missing.
- [x] `arg_choices<BaseTags, Choices...>`: allowed values for strings and enums.
- [x] `ArgTags{.required = true}`: missing value is an error unless another source supplies it.
- [x] `ArgTags{.positional = true}`: consumes positional argv entries.
- [x] `ArgTags{.flag = true}`: option can be present without an explicit value.
- [x] `ArgTags{.repeatable = true}`: option can occur multiple times.
- [x] `ArgTags{.hidden = true}`: omit from help.
- [x] `ArgTags{.command = true}`: field participates in command dispatch.
- [x] `ArgTags{.rangeMin = min, .rangeMax = max}`: numeric range, currently `[min, max)`.

## Next Batch

- [x] `arg_value_name<Name, BaseTags>`:
  - Help-only placeholder for non-flag option values, e.g. `--output <PATH>`.
  - Default placeholder remains `<value>`.
- [x] `arg_env<Name, BaseTags>`:
  - Reads an environment variable when CLI input is missing.
  - Precedence: CLI > environment > default tag > struct initial value.
  - Environment values use the same parsing, range, and choices validation as CLI values.
- [x] `arg_separator<Separator, BaseTags>`:
  - Splits vector option values, e.g. `--include a,b,c`.
  - Repeated options still work and append.
  - Empty segments are parsed normally, so they are valid only when the vector element type accepts them.

## Later Candidates

- [ ] `arg_aliases<...>`: additional option spellings. One-character aliases are treated as short aliases; longer
  aliases are treated as long aliases.
- [ ] `arg_implicit<Value>`: non-bool options may use an implicit value when present without an explicit value.
- [ ] `arg_group<Name>`: help section grouping.
- [ ] `arg_conflicts<...>` and `arg_requires<...>`: post-parse cross-field validation.
- [ ] `arg_deprecated<Message>`: keep old options while warning or hiding them.
- [ ] `arg_case_insensitive_choices`: opt-in relaxed choice matching.

## Semantics To Keep Explicit

- `required + default/env`: a supplied default or environment value satisfies `required`.
- Range syntax is half-open: `[min, max)`.
- Negative values after an option are consumed as values; positional negative values may still look like options unless
  the user uses `--`.
