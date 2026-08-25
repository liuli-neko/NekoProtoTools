/**
 * @file reflection_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "nekoproto/serialization/reflection.hpp"

namespace nekoproto {

namespace detail {
class ReflectionFieldBase {
public:
    ReflectionFieldBase() noexcept                          = default;
    virtual ~ReflectionFieldBase() noexcept                 = default;
    virtual auto name() const noexcept -> const std::string_view&   = 0;
    virtual auto typeInfo() const noexcept -> const std::type_info& = 0;
};

template <typename T>
class ReflectionField : public ReflectionFieldBase {
public:
    explicit ReflectionField(const std::string_view& name, T* value) : mValue(value), mName(name) {
        NEKO_ASSERT(value != nullptr, "ReflectionSerializer", "can not make reflection object {} for nullptr", name);
    }
    auto getField() const noexcept -> const T& { return *mValue; }
    void setField(const T& value) noexcept { (*mValue) = value; }
    auto name() const noexcept -> const std::string_view& override { return mName; }
    auto typeInfo() const noexcept -> const std::type_info& override { return typeid(T); }

private:
    T* const mValue;
    const std::string_view mName;
};

class ReflectionObject {
public:
    ReflectionObject()                        = default;
    ReflectionObject(const ReflectionObject&) = delete;
    ReflectionObject(ReflectionObject&& other) : mFields(std::move(other.mFields)) {}
    auto operator=(const ReflectionObject&) -> ReflectionObject& = delete;
    auto operator=(ReflectionObject&& other) -> ReflectionObject& {
        if (this != &other) {
            mFields = std::move(other.mFields);
        }
        return *this;
    }

    inline ~ReflectionObject() { clear(); }
    inline void clear() noexcept { mFields.clear(); }

    template <typename T>
    auto getField(const std::string_view& name, const T& defaultValue) const noexcept -> T {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return defaultValue;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return defaultValue;
        }
        auto reflectionField  = dynamic_cast<ReflectionField<T>*>(it->second.get());
        auto reflectionField1 = dynamic_cast<ReflectionField<const T>*>(it->second.get());
        if (reflectionField == nullptr && reflectionField1 == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return defaultValue;
        }
        NEKO_ASSERT(((reflectionField != nullptr ? reflectionField->name() : reflectionField1->name()) == name),
                    "ReflectionSerializer", "field name mismatch");
        return reflectionField != nullptr ? reflectionField->getField() : reflectionField1->getField();
    }

    template <typename T>
    auto getField(const std::string_view& name, T* result) const noexcept -> bool {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second.get());
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        NEKO_ASSERT(reflectionField->name() == name, "ReflectionSerializer", "field name mismatch");
        if (result != nullptr) {
            *result = reflectionField->getField();
        }
        return true;
    }
    template <typename T>
    auto setField(const std::string_view& name, const T& value) noexcept -> bool {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second.get());
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        reflectionField->setField(value);
        return true;
    }

    template <typename T>
    auto bindField(const std::string_view& name, T* value) -> ReflectionField<T>* {
        if (value == nullptr) {
            return nullptr;
        }
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            mFields.insert(std::make_pair<const std::string_view&, std::unique_ptr<ReflectionFieldBase>>(
                name, std::make_unique<ReflectionField<T>>(name, value)));
            return static_cast<ReflectionField<T>*>(mFields.find(name)->second.get());
        }
        NEKO_LOG_WARN("ReflectionSerializer", "field {} already exists, duplicate field will be overwritten.", name);
        it->second = std::make_unique<ReflectionField<T>>(name, value);
        return static_cast<ReflectionField<T>*>(it->second.get());
    }

private:
    std::map<std::string_view, std::unique_ptr<ReflectionFieldBase>> mFields;
};
} // namespace detail

class ReflectionSerializer {
public:
    ReflectionSerializer()                                       = default;
    ReflectionSerializer(const ReflectionSerializer&)            = delete;
    auto operator=(const ReflectionSerializer&) -> ReflectionSerializer& = delete;
    ReflectionSerializer(ReflectionSerializer&& other) : mObject(std::move(other.mObject)) {}
    auto operator=(ReflectionSerializer&& other) -> ReflectionSerializer& {
        mObject = std::move(other.mObject);
        return *this;
    }
    ~ReflectionSerializer() = default;

    template <typename T>
        requires detail::has_values_meta<std::remove_cvref_t<T>> && detail::has_names_meta<std::remove_cvref_t<T>>
    static auto reflection(T& obj) -> ReflectionSerializer {
        ReflectionSerializer rs;
        Reflect<std::remove_cvref_t<T>>::visitNamed(obj, [&rs](auto& field, std::string_view name) {
            const auto* bound = rs.mObject.bindField(name, &field);
            NEKO_ASSERT(bound != nullptr, "ReflectionSerializer", "failed to bind field {}", name);
        });
        return rs;
    }

    inline auto getObject() -> detail::ReflectionObject* { return &mObject; }

private:
    detail::ReflectionObject mObject;
};

} // namespace nekoproto
