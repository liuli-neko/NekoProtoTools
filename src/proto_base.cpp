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

#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/global/log.hpp"

#include <functional>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace detail {
NEKO_PROTO_API
std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>>&
static_init_funcs(const NEKO_STRING_VIEW& name = "", std::function<void(ProtoFactory*)> func = nullptr) {
    static std::map<NEKO_STRING_VIEW, std::function<void(ProtoFactory*)>> kFuncs = {};
    auto item                                                                    = kFuncs.find(name);
    if (!name.empty() && item == kFuncs.end() && func) {
        kFuncs.insert(std::make_pair(name, func));
    }
    if (item != kFuncs.end()) {
        NEKO_LOG_WARN("proto", "Duplicate init function: {}", name);
    }
    return kFuncs;
}
} // namespace detail

void ProtoFactory::_setVersion(int major, int minor, int patch) NEKO_NOEXCEPT {
    mVersion = ((major & 0xFF) << 16 | (minor & 0xFF) << 8 | (patch & 0xFF));
}

uint32_t ProtoFactory::version() const NEKO_NOEXCEPT { return mVersion; }

void ProtoFactory::_init() NEKO_NOEXCEPT {
    const auto& funcs = detail::static_init_funcs();
    mCreaterList.resize(funcs.size() + NEKO_RESERVED_PROTO_TYPE_SIZE + 1);
    for (const auto& item : funcs) {
        item.second(this);
    }
}

ProtoFactory::ProtoFactory(int major, int minor, int patch) {
    _init();
    _setVersion(major, minor, patch);
}

void ProtoFactory::regist(const NEKO_STRING_VIEW& name, std::function<IProto()> creator) NEKO_NOEXCEPT {
    auto type = _protoType(name, true);
    if (type < (int)mCreaterList.size()) {
        mCreaterList[type] = creator;
    } else {
        if (mDynamicCreaterMap.find(type) != mDynamicCreaterMap.end()) {
            NEKO_LOG_ERROR("proto", "Duplicate regist proto type: {}, will cover origin creator.", name);
        }
        mDynamicCreaterMap.insert(std::make_pair(type, creator));
    }
}

int ProtoFactory::_protoType(const NEKO_STRING_VIEW& name, const bool isDeclared, const int specifyType) NEKO_NOEXCEPT {
    auto& protoNameMap  = _staticProtoTypeMap();
    static int kCounter = NEKO_RESERVED_PROTO_TYPE_SIZE;
    if (name.empty()) {
        NEKO_LOG_ERROR("proto", "Empty proto name");
        return -1;
    }
    auto item = protoNameMap.find(name);
    if (protoNameMap.end() == item) {
        if (specifyType != -1 && isDeclared) {
            for (const auto& item1 : protoNameMap) {
                if (item1.second == specifyType) {
                    NEKO_LOG_ERROR("proto", "proto type {} has been declared as {}, please use another type",
                                   item1.first, item1.second);
                    return -1;
                }
            }
            if (specifyType > NEKO_RESERVED_PROTO_TYPE_SIZE) {
                NEKO_LOG_ERROR(
                    "proto",
                    "specify proto type {} must be less than 100, because more than 100 is used for auto assignment",
                    name);
                return -1;
            }
            NEKO_LOG_INFO("proto", "proto {} type is declared as {}", name, specifyType);
            protoNameMap.insert(std::make_pair(name, specifyType));
            return specifyType;
        }
        if (isDeclared) {
            protoNameMap.insert(std::make_pair(name, ++kCounter));
            NEKO_LOG_INFO("proto", "proto {} type is declared as {}", name, kCounter);
            return kCounter;
        }
        NEKO_LOG_ERROR("proto", "Proto type not declared: {}, are you created a ProtoFactory and declare this type?",
                       name);
        return -1;
    }
    return item->second;
}

IProto ProtoFactory::create(int type) const NEKO_NOEXCEPT {
    if (type > 0 && type < (int)mCreaterList.size() && nullptr != mCreaterList[type]) {
        return mCreaterList[type]();
    }
    if (type >= (int)mCreaterList.size()) {
        auto it = mDynamicCreaterMap.find(type);
        if (it != mDynamicCreaterMap.end()) {
            return it->second();
        }
    }
    return {};
}

IProto ProtoFactory::create(const char* name) const NEKO_NOEXCEPT { return create(_protoType(name, false)); }

const std::map<NEKO_STRING_VIEW, int>& ProtoFactory::protoTypeMap() NEKO_NOEXCEPT { return _staticProtoTypeMap(); }

std::map<NEKO_STRING_VIEW, int>& ProtoFactory::_staticProtoTypeMap() {
    static std::map<NEKO_STRING_VIEW, int> kProtoNameMap;
    return kProtoNameMap;
}

ProtoFactory::~ProtoFactory() {
    // Nothing to do
}

NEKO_END_NAMESPACE