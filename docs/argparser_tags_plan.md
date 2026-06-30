# Argparser Tags Plan

This document tracks the reflection-based argparser tag surface. The goal is to keep the core parser small while making
common command-line interfaces pleasant to describe from metadata.

## Current Core Tags

- [x] `arg_name<Long, Short>`: overrides the reflected field name and optional short name.
- [x] `arg_help<Help>`: help text shown under the option.
- [x] `arg_default<Default>`: value applied when CLI input is missing.
- [x] `arg_choices<Choices...>`: allowed values for strings and enums.
- [x] `ArgTags{.required = true}`: missing value is an error unless another source supplies it.
- [x] `ArgTags{.positional = true}`: consumes positional argv entries.
- [x] `ArgTags{.flag = true}`: option can be present without an explicit value.
- [x] `ArgTags{.repeatable = true}`: option can occur multiple times.
- [x] `ArgTags{.hidden = true}`: omit from help.
- [x] `ArgTags{.command = true}`: field participates in command dispatch.
- [x] `ArgTags{.rangeMin = min, .rangeMax = max}`: numeric range, currently `[min, max)`.

## Next Batch

- [x] `arg_value_name<Name>`:
  - Help-only placeholder for non-flag option values, e.g. `--output <PATH>`.
  - Default placeholder remains `<value>`.
- [x] `arg_env<Name>`:
  - Reads an environment variable when CLI input is missing.
  - Precedence: CLI > environment > default tag > struct initial value.
  - Environment values use the same parsing, range, and choices validation as CLI values.
- [x] `arg_separator<Separator>`:
  - Splits vector option values, e.g. `--include a,b,c`.
  - Repeated options still work and append.
  - Empty segments are parsed normally, so they are valid only when the vector element type accepts them.

## Later Candidates

- [x] `arg_aliases<...>`:
  - Additional option spellings. One-character aliases are treated as short aliases; longer aliases are treated as
    long aliases.
  - Help prints the canonical spelling first and then the aliases.
- [x] `arg_implicit<Value>`:
  - Non-flag options may omit an explicit value and fall back to an implicit one.
  - The implicit value is used when the next token looks like another option or when the option appears at the end of
    argv.
- [x] `arg_group<Name>`:
  - Help-only section grouping.
  - Ungrouped options stay under `Options:`; named groups are emitted as additional help sections in declaration order.
- [x] `arg_conflicts<Names...>` and `arg_requires<Names...>`:
  - Post-parse cross-field validation after CLI, environment, and default values have all been applied.
  - Names may reference canonical long names, short names, aliases, or the same names with `-`/`--` prefixes.
  - Missing required peers report `MissingRequired`; conflicting supplied peers report `InvalidValue`.
- [x] `arg_deprecated<Message>`:
  - Keeps old options accepted while marking them in help.
  - `ArgParserConfig::deprecatedOptionHandler` can be used to emit warnings when deprecated CLI input is used.
  - Compose with `ArgTags{.hidden = true}` when a deprecated option should stay accepted but disappear from help.
- [x] `arg_case_insensitive_choices`:
  - Opt-in relaxed choice matching for string and enum choices.
  - Enum names can be parsed with relaxed case when choices are present.

## Semantics To Keep Explicit

- Argparser tags do not accept nested `BaseTags` template parameters. Compose them as parallel `make_tags<a, b, c>`.
- If an input to `make_tags` is already a `TagList`, it is flattened into the surrounding tag list automatically.
- Repeated properties use last-wins lookup after flattening. This supports explicit overrides without merging same-name
  tags.
- Positional tag access is tuple-like (`std::get<I>(tags)` / `tags.get<I>()`), but semantic reads should go through
  `tag_query::get<tag_prop::...>` or `tag_query::get_tag<TagType>`.
- Multi-value tags should stay variadic inside one tag, e.g. `arg_choices<a, b, c>`, `arg_aliases<a, b>`,
  `arg_requires<a, b>`.
- `required + default/env`: a supplied default or environment value satisfies `required`.
- Range syntax is half-open: `[min, max)`.
- Negative values after an option are consumed as values; positional negative values may still look like options unless
  the user uses `--`.
- For `arg_implicit`, values that literally start with `-` should be passed inline, e.g. `--name=-value`.
- `arg_requires`/`arg_conflicts` use the same "supplied" meaning as `required`, so environment and default tags count as
  present for cross-field validation.
