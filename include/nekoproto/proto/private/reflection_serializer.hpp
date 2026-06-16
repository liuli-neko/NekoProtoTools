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

NEKO_BEGIN_NAMESPACE

namespace detail {
class ReflectionFieldBase {
public:
    ReflectionFieldBase() NEKO_NOEXCEPT                          = default;
    virtual ~ReflectionFieldBase() NEKO_NOEXCEPT                 = default;
    virtual const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT   = 0;
    virtual const std::type_info& typeInfo() const NEKO_NOEXCEPT = 0;
};

template <typename T>
class ReflectionField : public ReflectionFieldBase {
public:
    explicit ReflectionField(const NEKO_STRING_VIEW& name, T* value) : mValue(value), mName(name) {
        NEKO_ASSERT(value != nullptr, "ReflectionSerializer", "can not make reflection object {} for nullptr", name);
    }
    const T& getField() const NEKO_NOEXCEPT { return *mValue; }
    void setField(const T& value) NEKO_NOEXCEPT { (*mValue) = value; }
    const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT override { return mName; }
    const std::type_info& typeInfo() const NEKO_NOEXCEPT override { return typeid(T); }

private:
    T* const mValue;
    const NEKO_STRING_VIEW mName;
};

class ReflectionObject {
public:
    ReflectionObject()                        = default;
    ReflectionObject(const ReflectionObject&) = delete;
    ReflectionObject(ReflectionObject&& other) : mFields(std::move(other.mFields)) {}
    ReflectionObject& operator=(const ReflectionObject&) = delete;
    ReflectionObject& operator=(ReflectionObject&& other) {
        if (this != &other) {
            mFields = std::move(other.mFields);
        }
        return *this;
    }

    inline ~ReflectionObject() { clear(); }
    inline void clear() NEKO_NOEXCEPT { mFields.clear(); }

    template <typename T>
    T getField(const NEKO_STRING_VIEW& name, const T& defaultValue) const NEKO_NOEXCEPT {
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
    bool getField(const NEKO_STRING_VIEW& name, T* result) const NEKO_NOEXCEPT {
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
    bool setField(const NEKO_STRING_VIEW& name, const T& value) NEKO_NOEXCEPT {
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
    ReflectionField<T>* bindField(const NEKO_STRING_VIEW& name, T* value) {
        if (value == nullptr) {
            return nullptr;
        }
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            mFields.insert(std::make_pair<const NEKO_STRING_VIEW&, std::unique_ptr<ReflectionFieldBase>>(
                name, std::make_unique<ReflectionField<T>>(name, value)));
            return static_cast<ReflectionField<T>*>(mFields.find(name)->second.get());
        }
        NEKO_LOG_WARN("ReflectionSerializer", "field {} already exists, duplicate field will be overwritten.", name);
        it->second = std::make_unique<ReflectionField<T>>(name, value);
        return static_cast<ReflectionField<T>*>(it->second.get());
    }

private:
    std::map<NEKO_STRING_VIEW, std::unique_ptr<ReflectionFieldBase>> mFields;
};
} // namespace detail

class ReflectionSerializer {
public:
    ReflectionSerializer()                                       = default;
    ReflectionSerializer(const ReflectionSerializer&)            = delete;
    ReflectionSerializer& operator=(const ReflectionSerializer&) = delete;
    ReflectionSerializer(ReflectionSerializer&& other) : mObject(std::move(other.mObject)) {}
    ReflectionSerializer& operator=(ReflectionSerializer&& other) {
        mObject = std::move(other.mObject);
        return *this;
    }
    ~ReflectionSerializer() = default;

    template <typename T>
        requires detail::has_values_meta<std::remove_cvref_t<T>> && detail::has_names_meta<std::remove_cvref_t<T>>
    static ReflectionSerializer reflection(T& obj) {
        ReflectionSerializer rs;
        Reflect<std::remove_cvref_t<T>>::forEach(obj, [&rs](auto& field, std::string_view name, const auto& /*tags*/) {
            const auto* bound = rs.mObject.bindField(name, &field);
            NEKO_ASSERT(bound != nullptr, "ReflectionSerializer", "failed to bind field {}", name);
        });
        return rs;
    }

    inline detail::ReflectionObject* getObject() { return &mObject; }

private:
    detail::ReflectionObject mObject;
};

NEKO_END_NAMESPACE
