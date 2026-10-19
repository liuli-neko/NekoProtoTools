#pragma once
#define NEKO_SERIALIZER(...)                                                                                           \
public:                                                                                                                \
    constexpr auto nekoMemberTuple() noexcept { return nekoproto::detail::serializerMemberTuple(__VA_ARGS__); }       \
    constexpr auto nekoMemberTuple() const noexcept { return nekoproto::detail::serializerMemberTuple(__VA_ARGS__); } \
    struct NekoSerializerArgsHelper {                                                                                  \
        using tuple = decltype(std::forward_as_tuple(__VA_ARGS__));                                                    \
    };                                                                                                                 \
    struct Neko {                                                                                                      \
        using NekoSerializerArgsTuple = typename NekoSerializerArgsHelper::tuple;                                      \
        constexpr static std::array names =                                                                            \
            nekoproto::detail::MakeNamesImpl<#__VA_ARGS__, std::tuple_size_v<NekoSerializerArgsTuple>>::names;         \
        constexpr static auto values = []<std::size_t... Is>(std::index_sequence<Is...>) {                             \
            return std::tuple{nekoproto::detail::NekoMemberIndexAccessor<Is>{}...};                                    \
        }(std::make_index_sequence<std::tuple_size_v<NekoSerializerArgsTuple>>{});                                      \
        constexpr static auto field_tags = []<std::size_t... Is>(std::index_sequence<Is...>) {                         \
            return std::tuple{                                                                                         \
                nekoproto::detail::serializerMakeTags<std::tuple_element_t<Is, NekoSerializerArgsTuple>>()...};        \
        }(std::make_index_sequence<std::tuple_size_v<NekoSerializerArgsTuple>>{});                                      \
    };
