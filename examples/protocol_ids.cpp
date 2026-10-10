#include <nekoproto/proto/proto_base.hpp>

#include <iostream>
#include <string_view>

namespace {

// Only the registry's public type descriptor is needed for this probe.
struct FixedAlpha {
    struct ProtoType {
        static constexpr auto name() noexcept -> std::string_view { return "fixed-alpha"; }
    };
};

struct FixedBeta {
    struct ProtoType {
        static constexpr auto name() noexcept -> std::string_view { return "fixed-beta"; }
    };
};

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const std::string_view order = argv[1];
    if (order != "--forward" && order != "--reverse") return 1;
    const bool reverse = order == "--reverse";

    if (reverse) {
        if (nekoproto::ProtoFactory::specifyProtoType<FixedBeta>(12) != 12) return 2;
        if (nekoproto::ProtoFactory::specifyProtoType<FixedAlpha>(11) != 11) return 2;
    } else {
        if (nekoproto::ProtoFactory::specifyProtoType<FixedAlpha>(11) != 11) return 2;
        if (nekoproto::ProtoFactory::specifyProtoType<FixedBeta>(12) != 12) return 2;
    }

    nekoproto::ProtoFactory factory;
    const auto register_dynamic = [&factory](std::string_view name) {
        factory.regist(name, [] { return nekoproto::IProto{}; });
    };
    if (reverse) {
        register_dynamic("dynamic-beta");
        register_dynamic("dynamic-alpha");
    } else {
        register_dynamic("dynamic-alpha");
        register_dynamic("dynamic-beta");
    }

    const auto& ids = nekoproto::ProtoFactory::protoTypeMap();
    const auto alpha = ids.at("dynamic-alpha");
    const auto beta = ids.at("dynamic-beta");
    const bool fixed_ids = nekoproto::ProtoFactory::protoType<FixedAlpha>() == 11 &&
                           nekoproto::ProtoFactory::protoType<FixedBeta>() == 12;
    const bool ordered_auto_ids = reverse ? beta + 1 == alpha : alpha + 1 == beta;
    std::cout << "fixed-alpha=11 fixed-beta=12 dynamic-alpha=" << alpha << " dynamic-beta=" << beta << '\n';
    return fixed_ids && ordered_auto_ids ? 0 : 3;
}
