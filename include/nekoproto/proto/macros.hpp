#pragma once
#define NEKO_DECLARE_PROTOCOL(className, Serializer)                                                                   \
public:                                                                                                                \
    using ProtoType = typename nekoproto::detail::ProtoBase<className, Serializer>;                                    \
    /** @brief make proto from self pointer */                                                                         \
    inline nekoproto::IProto makeProto() noexcept { return nekoproto::IProto{new ProtoType(this)}; }                   \
    /** @brief make proto with structure */                                                                            \
    template <typename... Args>                                                                                        \
    inline static nekoproto::IProto emplaceProto(Args&&... args) noexcept {                                            \
        static_assert(std::is_move_constructible<className>::value, "class " #className " must be copable");           \
        return nekoproto::IProto{new ProtoType(className{std::forward<Args>(args)...})};                               \
    }                                                                                                                  \
    /** @brief make proto by copying other */                                                                          \
    inline static nekoproto::IProto makeProto(const className& other) noexcept {                                       \
        static_assert(std::is_copy_constructible<className>::value, "class " #className " must be copable");           \
        return nekoproto::IProto{new ProtoType(other)};                                                                \
    }
