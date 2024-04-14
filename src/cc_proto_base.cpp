#include "../include/cc_proto_base.hpp"

#include <functional>
#include <vector>

CS_PROTO_BEGIN_NAMESPACE

void ProtoFactory::setVersion(int major, int minor, int patch) {
    kVersion = ((major & 0xFF) << 16 |
                  (minor & 0xFF) << 8 |
                  (patch & 0xFF));
}

uint32_t ProtoFactory::version() {
    return kVersion;
}

std::vector<std::function<void(ProtoFactory*)>> 
static_init_funcs(std::function<void(ProtoFactory*)> func = nullptr) {
    static std::vector<std::function<void(ProtoFactory*)>> funcs = {};
    if (func) {
        funcs.push_back(func);
    }
    return funcs;
}

void ProtoFactory::init() {
    for (auto& func : static_init_funcs()) {
        func(this);
    }
}

ProtoFactory::ProtoFactory(int major, int minor, int patch) {
    init();
    setVersion(major, minor, patch);
}

ProtoFactory::~ProtoFactory() {
    // Nothing to do
}

CS_PROTO_END_NAMESPACE