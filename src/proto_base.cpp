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

namespace nekoproto {
namespace detail {
NEKO_PROTO_API
auto staticInitFuncs(const std::string_view& name = "", std::function<void(ProtoFactory*)> func = nullptr)
    -> std::map<std::string_view, std::function<void(ProtoFactory*)>>& {
    static std::map<std::string_view, std::function<void(ProtoFactory*)>> kFuncs = {};
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

void ProtoFactory::setVersion(int major, int minor, int patch) noexcept {
    mVersion = ((major & 0xFF) << 16 | (minor & 0xFF) << 8 | (patch & 0xFF));
}

auto ProtoFactory::version() const noexcept -> uint32_t { return mVersion; }

void ProtoFactory::init() noexcept {
    const auto& funcs = detail::staticInitFuncs();
    mCreaterList.resize(funcs.size() + reserved_proto_type_size + 1);
    for (const auto& item : funcs) {
        item.second(this);
    }
}

ProtoFactory::ProtoFactory(int major, int minor, int patch) {
    init();
    setVersion(major, minor, patch);
}

void ProtoFactory::regist(const std::string_view& name, std::function<IProto()> creator) noexcept {
    auto type = protoType(name, true);
    if (type < (int)mCreaterList.size()) {
        mCreaterList[type] = creator;
    } else {
        if (mDynamicCreaterMap.find(type) != mDynamicCreaterMap.end()) {
            NEKO_LOG_ERROR("proto", "Duplicate regist proto type: {}, will cover origin creator.", name);
        }
        mDynamicCreaterMap.insert(std::make_pair(type, creator));
    }
}

auto ProtoFactory::protoType(const std::string_view& name, const bool isDeclared, const int specifyType) noexcept
    -> int {
    auto& protoNameMap  = staticProtoTypeMap();
    static int kCounter = reserved_proto_type_size;
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
            if (specifyType > reserved_proto_type_size) {
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

auto ProtoFactory::create(int type) const noexcept -> IProto {
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

auto ProtoFactory::create(const char* name) const noexcept -> IProto { return create(protoType(name, false)); }

auto ProtoFactory::protoTypeMap() noexcept -> const std::map<std::string_view, int>& { return staticProtoTypeMap(); }

auto ProtoFactory::staticProtoTypeMap() -> std::map<std::string_view, int>& {
    static std::map<std::string_view, int> kProtoNameMap;
    return kProtoNameMap;
}

ProtoFactory::~ProtoFactory() {
    // Nothing to do
}

} // namespace nekoproto
