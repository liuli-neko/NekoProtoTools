/**
 * @file xml_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "private/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDXML)
#include "private/log.hpp"

#include <limits>
#include <type_traits>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

#include "private/helpers.hpp"
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <rapidxml/rapidxml_utils.hpp>

NEKO_BEGIN_NAMESPACE

namespace detail {

template <typename ChT>
using XMLNode = rapidxml::xml_node<ChT>;
template <typename ChT>
using XMLAttribute = rapidxml::xml_attribute<ChT>;
template <typename ChT>
using XMLDocument = rapidxml::xml_document<ChT>;
template <typename ChT>
using XMLBuffer = rapidxml::memory_pool<ChT>;

template <typename BufferT>
struct xml_output_buffer_type { // NOLINT(readability-identifier-naming)
    using writer_type = void;
};

} // namespace detail

template <typename BufferT = void>
class RapidXmlOutputSerializer : public detail::OutputSerializer<RapidXmlOutputSerializer<BufferT>> {
public:
public:
    explicit RapidXmlOutputSerializer() NEKO_NOEXCEPT : detail::OutputSerializer<RapidXmlOutputSerializer>(this) {}

    RapidXmlOutputSerializer(const RapidXmlOutputSerializer& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidXmlOutputSerializer>(this) {}
    RapidXmlOutputSerializer(RapidXmlOutputSerializer&& other) NEKO_NOEXCEPT
        : detail::OutputSerializer<RapidXmlOutputSerializer>(this) {}
    ~RapidXmlOutputSerializer() { end(); }
    template <typename T>
    bool saveValue(SizeTag<T> const& /*unused*/) {
        return true;
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        return true;
    }
    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool saveValue(const T value) NEKO_NOEXCEPT {
        return true;
    }
    bool saveValue(const int64_t value) NEKO_NOEXCEPT { return true; }
    bool saveValue(const uint64_t value) NEKO_NOEXCEPT { return true; }
    bool saveValue(const float value) NEKO_NOEXCEPT { return true; }
    bool saveValue(const double value) NEKO_NOEXCEPT { return true; }
    bool saveValue(const bool value) NEKO_NOEXCEPT { return true; }
    template <typename CharT, typename Traits, typename Alloc>
    bool saveValue(const std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        return true;
    }
    bool saveValue(const char* value) NEKO_NOEXCEPT { return true; }
    bool saveValue(const std::nullptr_t) NEKO_NOEXCEPT { return true; }
#if NEKO_CPP_PLUS >= 17
    template <typename CharT, typename Traits>
    bool saveValue(const std::basic_string_view<CharT, Traits> value) NEKO_NOEXCEPT {
        return true;
    }

    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        if constexpr (traits::is_optional<T>::value) {
            if (value.value.has_value()) {
                return /*write key &&*/ (*this)(value.value.value());
            }
        } else {
            return /*write key &&*/ (*this)(value.value);
        }
        return true;
    }
#else
    template <typename T>
    bool saveValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        return /* write key &&*/ (*this)(value.value);
    }
#endif
    bool startArray(const std::size_t /*unused*/) NEKO_NOEXCEPT {
        // TODO: start array
        return true;
    }
    bool endArray() NEKO_NOEXCEPT {
        // TODO: end array
        return true;
    }
    bool startObject(const std::size_t /*unused*/ = -1) NEKO_NOEXCEPT {
        // TODO: start object
        return true;
    }
    bool endObject() NEKO_NOEXCEPT {
        // TODO: end object
        return true;
    }
    bool end() NEKO_NOEXCEPT {
        // TODO: end
        return true;
    }

private:
    RapidXmlOutputSerializer& operator=(const RapidXmlOutputSerializer&) = delete;
    RapidXmlOutputSerializer& operator=(RapidXmlOutputSerializer&&)      = delete;
};

/**
 * @brief json input serializer
 * This class provides a convenient template interface to help you parse JSON to CPP objects, you only need to give the
 * variables that need to be assigned to this class through parentheses, and the class will automatically expand the
 * object and array to make it easier for you to iterate through all the values.
 *
 * @note
 * A layer is automatically unfolded when constructing.
 * Like a vector<int> vec, and you have a array json data {1, 2, 3, 4} "json_data"
 * you construct RapidXmlInputSerializer is(json_data)
 * you should not is(vec), because it will try unfold the first node as array.
 * you should call a load function like load(sa, vec). or do it like this:
 * size_t size;
 * is(make_size_tag(size));
 * vec.resize(size);
 * for (auto &v : vec) {
 *     is(v);
 * }
 *
 */
template <typename BufferT = void>
class RapidXmlInputSerializer : public detail::InputSerializer<RapidXmlInputSerializer<BufferT>> {

public:
    // buf must be null terminated
    explicit RapidXmlInputSerializer(const char* buf, std::size_t /*unused*/) NEKO_NOEXCEPT
        : detail::InputSerializer<RapidXmlInputSerializer>(this) {
        mDocument.parse<rapidxml::parse_full>(const_cast<char*>(buf));
        mNode = &mDocument;
        mStack.push_back(mNode);
    }

    ~RapidXmlInputSerializer() NEKO_NOEXCEPT { mDocument.clear(); }
    operator bool() const NEKO_NOEXCEPT { return mDocument.type() == rapidxml::node_document; }
    NEKO_STRING_VIEW name() const NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load name value: {}", std::string{mNode->name(), mNode->name_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi) {
            return {mNode->name(), mNode->name_size()};
        }
        return {};
    }

    template <typename T, traits::enable_if_t<std::is_signed<T>::value, sizeof(T) <= sizeof(int64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool loadValue(T& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load signed value: {}", std::string{mNode->value(), mNode->value_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            char* end      = nullptr;
            int64_t cvalue = std::strtoll(mNode->value(), &end, 10);
            if (end != mNode->value() + mNode->value_size()) {
                return false;
            }
            if (cvalue < std::numeric_limits<T>::min() || cvalue > std::numeric_limits<T>::max()) {
                return false;
            }
            value = static_cast<T>(cvalue);
            mNode = mNode->next_sibling();
            return true;
        }
        return false;
    }

    template <typename T, traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) <= sizeof(uint64_t),
                                              !std::is_enum<T>::value> = traits::default_value_for_enable>
    bool loadValue(T& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load unsigned value: {}", std::string{mNode->value(), mNode->value_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            char* end       = nullptr;
            uint64_t cvalue = std::strtoull(mNode->value(), &end, 10);
            if (end != mNode->value() + mNode->value_size()) {
                return false;
            }
            if (cvalue > std::numeric_limits<T>::max()) {
                return false;
            }
            value = static_cast<T>(cvalue);
            mNode = mNode->next_sibling();
            return true;
        }
        return false;
    }

    template <typename CharT, typename Traits, typename Alloc>
    bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load string value: {}", std::string{mNode->value(), mNode->value_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            value = {mNode->value(), mNode->value_size()};
            mNode = mNode->next_sibling();
            return true;
        }
        return false;
    }

    bool loadValue(float& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load float value: {}", std::string{mNode->value(), mNode->value_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            char* end;
            value    = std::strtof(mNode->value(), &end);
            auto ret = end == (mNode->value() + mNode->value_size());
            mNode    = mNode->next_sibling();
            return ret;
        }
        return false;
    }

    bool loadValue(double& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "load double value: {}", std::string{mNode->value(), mNode->value_size()});
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            char* end;
            value = std::strtod(mNode->value(), &end);
            mNode = mNode->next_sibling();
            return end == (mNode->value() + mNode->value_size());
        }
        return false;
    }

    bool loadValue(bool& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            value = NEKO_STRING_VIEW{mNode->value(), mNode->value_size()} == "true" ||
                    NEKO_STRING_VIEW{mNode->value(), mNode->value_size()} == "True";
            mNode = mNode->next_sibling();
            return true;
        }
        return false;
    }

    bool loadValue(std::nullptr_t) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        if (mNode->type() == rapidxml::node_element || mNode->type() == rapidxml::node_pi ||
            mNode->type() == rapidxml::node_data || mNode->type() == rapidxml::node_cdata) {
            NEKO_STRING_VIEW value = NEKO_STRING_VIEW{mNode->value(), mNode->value_size()};
            mNode                  = mNode->next_sibling();
            return value == "null";
        }
        return false;
    }

    template <typename T>
    bool loadValue(const SizeTag<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        if (mNode->type() == rapidxml::node_element && mStack.size() > 0) {
            value.size = 0;
            for (auto* child = mNode->first_node(); child != nullptr; child = child->next_sibling()) {
                NEKO_LOG_INFO("XMLSerializer", "size count child name: {}",
                              std::string{child->name(), child->name_size()});
                value.size++;
            }
            NEKO_LOG_INFO("XMLSerializer", "Loading SizeTag: {}", value.size);
            return true;
        }
        return false;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        NEKO_LOG_INFO("XMLSerializer", "Load NameValuePair: {}", std::string{value.name, value.nameLen});
        auto node = mNode->first_node(value.name, value.nameLen);
        bool ret  = true;
        if constexpr (traits::is_optional<T>::value) {
            if (nullptr == node) {
                NEKO_LOG_WARN("XMLSerializer", "Node {} not found", std::string{value.name, value.nameLen});
                value.value.reset();
            } else {
                typename traits::is_optional<T>::value_type result;
                mStack.push_back(mNode);
                mNode = node;
                ret   = (*this)(result);
                mNode = mStack.back();
                mStack.pop_back();
                value.value.emplace(std::move(result));
            }
        } else {
            if (nullptr == node) {
                NEKO_LOG_WARN("XMLSerializer", "Node {} not found", std::string{value.name, value.nameLen});
                return false;
            }
            mStack.push_back(mNode);
            mNode = node;
            ret   = (*this)(value.value);
            mNode = mStack.back();
            mStack.pop_back();
        }
        return ret;
    }
#else
    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        auto node = mNode->first_node(value.name, value.nameLen);
        if (nullptr == node) {
            NEKO_LOG_WARN("XMLSerializer", "Node {} not found", std::string{value.name, value.nameLen});
            return false;
        }
        mNode = node;
        mStack.push_back(node);
        ret   = (*this)(value.value);
        mNode = mStack.back();
        mStack.pop_back();
        return ret;
    }
#endif
    bool startNode() NEKO_NOEXCEPT {
        NEKO_ASSERT(mNode != nullptr, "XMLSerializer", "Current Item is nullptr");
        if (mNode->type() == rapidxml::node_document) {
            return true;
        }
        if (mNode->type() == rapidxml::node_element) {
            auto* node = mIsEnter ? mNode : mNode->first_node();
            if (node != nullptr) {
                mNode = node;
                mStack.push_back(mNode);
                NEKO_LOG_INFO("XMLSerializer", "enter Node {}", std::string{mNode->name(), mNode->name_size()});
                return true;
            }
        }
        NEKO_LOG_INFO("XMLSerializer", "Node is not a valid element");
        return false;
    }

    bool finishNode() NEKO_NOEXCEPT {
        if (mStack.size() > 0U) {
            NEKO_LOG_INFO("XMLSerializer", "finish Node {}",
                          std::string{mStack.back()->name(), mStack.back()->name_size()});
            mNode    = mStack.size() > 1 ? mStack.back()->next_sibling() : mStack.back();
            mIsEnter = true;
            mStack.pop_back();
            return true;
        }
        NEKO_LOG_INFO("XMLSerializer", "Node stack is empty");
        return false;
    }

private:
    RapidXmlInputSerializer& operator=(const RapidXmlInputSerializer&) = delete;
    RapidXmlInputSerializer& operator=(RapidXmlInputSerializer&&)      = delete;

private:
    detail::XMLDocument<char> mDocument;
    std::vector<detail::XMLNode<char>*> mStack;
    detail::XMLNode<char>* mNode;
    bool mIsEnter = false;
};

// #######################################################
// JsonSerializer prologue and epilogue
// #######################################################

// #######################################################
// name value pair
template <typename T, typename BufferT>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const NameValuePair<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
//  size tag
template <typename T, typename BufferT>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const SizeTag<T>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #########################################################
// class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
template <class T, typename BufferT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value,
                              !traits::has_method_serialize<T, RapidXmlInputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <class T, typename BufferT,
          traits::enable_if_t<traits::has_method_serialize<T, RapidXmlInputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidXmlInputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startNode();
}
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value,
                              !traits::has_method_serialize<T, RapidXmlInputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<traits::has_method_serialize<T, RapidXmlInputSerializer<BufferT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.finishNode();
}

template <class T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                              !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidXmlOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                              !traits::has_method_const_serialize<T, RapidXmlOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<traits::has_method_const_serialize<T, RapidXmlOutputSerializer<WriterT>>::value ||
                              traits::has_method_serialize<T, RapidXmlOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.startObject(-1);
}
template <typename T, typename WriterT,
          traits::enable_if_t<traits::has_method_const_serialize<T, RapidXmlOutputSerializer<WriterT>>::value ||
                              traits::has_method_serialize<T, RapidXmlOutputSerializer<WriterT>>::value> =
              traits::default_value_for_enable>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& sa, const T& /*unused*/) NEKO_NOEXCEPT {
    return sa.endObject();
}

// #########################################################
// # arithmetic types
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// # std::string
template <typename BufferT, typename CharT, typename Traits, typename Alloc>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT, typename CharT, typename Traits, typename Alloc>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename Alloc, typename WriterT>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string<CharT, Traits, Alloc>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

#if NEKO_CPP_PLUS >= 17
template <typename CharT, typename Traits, typename WriterT>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename CharT, typename Traits, typename WriterT>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/,
                     const std::basic_string_view<CharT, Traits>& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
#endif

// #####################################################
// # std::nullptr_t
template <typename BufferT>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
template <typename BufferT>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}

template <typename WriterT>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
template <typename WriterT>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT {
    return true;
}
// #####################################################
// # minimal serializable
template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename BufferT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidXmlInputSerializer<BufferT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool prologue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}
template <typename T, typename WriterT,
          traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
inline bool epilogue(RapidXmlOutputSerializer<WriterT>& /*unused*/, const T& /*unused*/) NEKO_NOEXCEPT {
    return true;
}

// #####################################################
// default JsonSerializer type definition
struct RapidXMLSerializer {
    using OutputSerializer = RapidXmlOutputSerializer<void>;
    using InputSerializer  = RapidXmlInputSerializer<void>;
};

NEKO_END_NAMESPACE

#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif

#endif