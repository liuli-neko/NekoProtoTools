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

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "global.hpp"
#include "log.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
struct NameValuePair;

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
    inline explicit ReflectionField(const NEKO_STRING_VIEW& name, T* value) : mValue(value), mName(name) {
        if (value == nullptr) {
            throw std::invalid_argument("can not make reflection object " + std::string(name) + " for nullptr");
        }
    }
    inline const T& getField() const NEKO_NOEXCEPT { return *mValue; }
    inline void setField(const T& value) NEKO_NOEXCEPT { (*mValue) = value; }
    inline const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT override { return mName; }
    inline const std::type_info& typeInfo() const NEKO_NOEXCEPT override { return typeid(T); }

private:
    T* const mValue;
    const NEKO_STRING_VIEW mName;
};

class ReflectionObject {
public:
    ReflectionObject() = default;
    inline ~ReflectionObject() { clear(); }
    inline void clear() NEKO_NOEXCEPT {
        for (auto& it : mFields) {
            delete it.second;
            it.second = nullptr;
        }
        mFields.clear();
    }

    template <typename T>
    inline T getField(const NEKO_STRING_VIEW& name, const T& defaultValue) const NEKO_NOEXCEPT {
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
        auto reflectionField  = dynamic_cast<ReflectionField<T>*>(it->second);
        auto reflectionField1 = dynamic_cast<ReflectionField<const T>*>(it->second);
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
    inline bool getField(const NEKO_STRING_VIEW& name, T* result) const NEKO_NOEXCEPT {
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
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
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
    inline bool setField(const NEKO_STRING_VIEW& name, const T& value) NEKO_NOEXCEPT {
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
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        reflectionField->setField(value);
        return true;
    }

    template <typename T>
    inline ReflectionField<T>* bindField(const NEKO_STRING_VIEW& name, T* value) {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            mFields.insert(std::make_pair(name, new ReflectionField<T>(name, value)));
            return static_cast<ReflectionField<T>*>(mFields.find(name)->second);
        }
        NEKO_LOG_WARN("ReflectionSerializer", "field {} already exists, duplicate field will be overwritten.", name);
        delete it->second;
        it->second = new ReflectionField<T>(name, value);
        return static_cast<ReflectionField<T>*>(it->second);
    }

private:
    std::map<NEKO_STRING_VIEW, ReflectionFieldBase*> mFields;
};

class ReflectionSerializer {
public:
    inline ReflectionSerializer() { mObject.clear(); }
    inline ~ReflectionSerializer() = default;
    inline operator bool() const { return true; }

    template <typename... Ts>
    inline bool operator()(const Ts&... fields) {
        return process(fields...);
    }

    inline ReflectionObject* getObject() { return &mObject; }

private:
    template <typename T, typename... Ts>
    inline bool process(const T& field, const Ts&... fields) {
        return process(field) && process(fields...);
    }
    template <typename T>
    inline bool process(const NameValuePair<T>& field) {
        return nullptr != mObject.bindField(NEKO_STRING_VIEW(field.name, field.nameLen), std::addressof(field.value));
    }

private:
    ReflectionObject mObject;
};

} // namespace detail

NEKO_END_NAMESPACE