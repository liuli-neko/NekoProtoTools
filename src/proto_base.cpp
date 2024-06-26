/**
 * @file proto_base.cpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../core/proto_base.hpp"

#include <functional>
#include <vector>

NEKO_BEGIN_NAMESPACE

void ProtoFactory::setVersion(int major, int minor, int patch) NEKO_NOEXCEPT {
    mVersion = ((major & 0xFF) << 16 | (minor & 0xFF) << 8 | (patch & 0xFF));
}

uint32_t ProtoFactory::version() const NEKO_NOEXCEPT { return mVersion; }

std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>>&
static_init_funcs(const NEKO_STRING_VIEW& name = "", std::function<void(ProtoFactory*)> func = nullptr) {
    static std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>> funcs = {};
    auto item                                                                   = funcs.find(name);
    if (!name.empty() && item == funcs.end() && func) {
        funcs.insert(std::make_pair(name, func));
    }
    if (item != funcs.end()) {
        NEKO_LOG_WARN("Duplicate init function: {}", name);
    }
    return funcs;
}

void ProtoFactory::init() NEKO_NOEXCEPT {
    const auto& funcs = static_init_funcs();
    mCreaterList.resize(funcs.size() + 1);
    for (const auto& item : funcs) {
        item.second(this);
    }
}

ProtoFactory::ProtoFactory(int major, int minor, int patch) {
    init();
    setVersion(major, minor, patch);
}

void ProtoFactory::regist(const NEKO_STRING_VIEW& name, std::function<IProto*()> creator) NEKO_NOEXCEPT {
    auto type          = proto_type(name, true);
    mCreaterList[type] = creator;
}

int ProtoFactory::proto_type(const NEKO_STRING_VIEW& name, const bool isGeneric) NEKO_NOEXCEPT {
    static std::map<NEKO_STRING_VIEW, int> protoNameMap;
    static int counter = 0;
    auto item          = protoNameMap.find(name);
    if (protoNameMap.end() == item) {
        if (isGeneric) {
            protoNameMap.insert(std::make_pair(name, ++counter));
            return counter;
        } else {
            NEKO_LOG_ERROR("Proto type not generic: {}, are you created a ProtoFactory and declare this type?", name);
            return -1;
        }
    }
    return item->second;
}

std::unique_ptr<IProto> ProtoFactory::create(int type) const NEKO_NOEXCEPT {
    if (type > 0 && type < mCreaterList.size() && nullptr != mCreaterList[type]) {
        return std::unique_ptr<IProto>(mCreaterList[type]());
    }
    return nullptr;
}

std::unique_ptr<IProto> ProtoFactory::create(const char* name) const NEKO_NOEXCEPT {
    auto index = proto_type(name, false);
    if (index > 0 && index < mCreaterList.size() && nullptr != mCreaterList[index]) {
        return std::unique_ptr<IProto>(mCreaterList[index]());
    }
    return nullptr;
}

ProtoFactory::~ProtoFactory() {
    // Nothing to do
}

NEKO_END_NAMESPACE