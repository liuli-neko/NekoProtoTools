# Reflection Provider Refactor Plan

## Goal

`Reflect<T>` is the public reflection API. It should stay stable while the implementation gains multiple metadata backends. The immediate refactor separates:

- public API and iteration algorithms,
- a normalized compile-time field model,
- metadata providers that adapt concrete backends.

This split prepares the current implementation for a C++26 native reflection backend without changing serializer, argparser, RPC, or schema call sites.

## Backend Priority

Explicit user metadata has priority over automatic discovery:

1. `T::Neko` metadata, including `Object`, `Array`, single value, names, values, and `field_tags`.
2. `Meta<T>` specializations.
3. Future C++26 native reflection provider.
4. Current aggregate `AutoUnwrap` fallback.

The future native backend is meant to replace the current `AutoUnwrap` path, not override user-declared metadata. Users who need private fields, custom accessors, ABI-stable field order, or manual tags should keep using `T::Neko` or `Meta<T>`.

## Layering

### Public API

`Reflect<T>` keeps the existing surface:

- `names()`, `name(index)`, `className()`, `size()`,
- `value<N>(obj)`, `value(obj, index)`, `value(obj, name)`,
- `forEach(obj, callback)`,
- `forEachMeta(callback)`,
- `value_types`, `field_tags`, `value_count`.

No downstream module should depend on provider-specific details.

### Field Model

`ReflectModel<T>` is the normalized compile-time view consumed by `Reflect<T>`:

- resolved field value types,
- normalized field tags,
- field count.

It also performs tag/type checks after provider metadata is normalized.

### Provider SPI

Providers adapt concrete metadata sources into a common shape. The transitional provider still exposes tuple-like accessors because the current codebase is built around `Object`, `Array`, `NEKO_SERIALIZER`, and aggregate unwrap. The desired long-term provider shape is index-based:

```cpp
Provider<T>::value_count
Provider<T>::has_names

Provider<T>::template field_type<I>
Provider<T>::template name<I>()
Provider<T>::template tag<I>()
Provider<T>::template get<I>(obj)
```

The index-based shape maps cleanly to C++26 native reflection while current tuple-based providers can implement it on top of `std::get<I>`.

## Names And Tags

`Reflect<T>::names()` returns reflected source names. Wire names and command-line option names remain tag semantics:

- serializer field rename uses `tag_prop::name`,
- argparser long names use `tag_prop::long_name`,
- comments, skip behavior, flat/unframed, fixed length, and similar metadata stay in tags.

Future C++26 annotations should be normalized by the native provider into the existing tag vocabulary:

```cpp
NoTags
JsonTags
BinaryTags
TagList<...>
```

Downstream code should continue to use `tag_query` and should not know whether tags came from `make_tags` or native annotations.

## First Refactor Step

The first step is intentionally conservative:

- add `ReflectProvider<T>` as the current metadata adapter,
- add `ReflectModel<T>` for `value_types`, `field_tags`, and `value_count`,
- keep `ReflectHelper<T>` as a compatibility alias/facade,
- update `Reflect<T>` to consume provider/model instead of directly inspecting `T::Neko` or `Meta<T>`.

After this step, behavior should be identical. Future work can replace the `AutoUnwrap` branch with `NativeReflectionProvider<T>` behind the same provider selection point.
