#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary/endian.hpp"
#include "nekoproto/serialization/error.hpp"
#include "nekoproto/serialization/private/integer.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

NEKO_BEGIN_NAMESPACE
namespace binary {

class Reader {
private:
    struct State {
        const char* data = nullptr;
        std::size_t size = 0;
        std::size_t offset = 0;
    };

    struct Frame {
        enum class Kind { Unknown, Array, Object };

        Kind kind = Kind::Unknown;
        std::size_t size = 0;
        std::size_t index = 0;
        bool initialized = false;
        bool consumed = false;
        std::optional<sa::Error> error;
    };

public:
    struct InputValue {
        State* state = nullptr;
        std::shared_ptr<Frame> frame = std::make_shared<Frame>();
    };

    using InputValueType  = InputValue;
    using InputArrayType  = InputValue;
    using InputObjectType = InputValue;

    Reader(const char* data, std::size_t size) : mState{data, size, 0} {}

    InputValueType root() { return {&mState, std::make_shared<Frame>()}; }
    static InputValueType next(const InputValueType& input) {
        return {input.state, std::make_shared<Frame>()};
    }
    std::size_t offset() const noexcept { return mState.offset; }
    std::size_t size() const noexcept { return mState.size; }

    static std::size_t arraySize(const InputArrayType& array) noexcept { return array.frame->size; }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) noexcept {
        auto child = InputValueType{array.state, std::make_shared<Frame>()};
        if (array.frame->kind != Frame::Kind::Array || index != array.frame->index ||
            index >= array.frame->size) {
            child.frame->error =
                sa::error(sa::ErrorCode::InvalidIndex, "Binary array index is out of sequence");
            return child;
        }
        ++array.frame->index;
        return child;
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept { return object.frame->size; }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) {
        if (object.frame->kind != Frame::Kind::Object || object.frame->index >= object.frame->size) {
            return sa::error(sa::ErrorCode::InvalidField,
                             "Binary object does not contain field '" + std::string(name) + "'");
        }
        auto encodedName = readString(*object.state);
        if (!encodedName) {
            return encodedName.error();
        }
        ++object.frame->index;
        if (encodedName.value() != name) {
            return sa::error(sa::ErrorCode::InvalidField,
                             "Expected binary field '" + std::string(name) + "', got '" +
                                 encodedName.value() + "'");
        }
        return InputValueType{object.state, std::make_shared<Frame>()};
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        while (object.frame->index < object.frame->size) {
            auto name = readString(*object.state);
            if (!name) {
                object.frame->error = name.error();
                return false;
            }
            ++object.frame->index;
            if (!fn(name.value(), InputValueType{object.state, std::make_shared<Frame>()})) {
                return false;
            }
        }
        return true;
    }

    static bool isEmpty(const InputValueType& input) {
        if (input.frame->consumed || input.frame->error) {
            return false;
        }
        const auto offset = input.state->offset;
        auto value = readString(*input.state);
        if (value && value.value() == "null") {
            input.frame->consumed = true;
            return true;
        }
        input.state->offset = offset;
        return false;
    }

    template <typename T>
    static sa::Result<T> toBasicType(const InputValueType& input) {
        if (auto error = validate(input); error.has_value()) {
            return *error;
        }
        input.frame->consumed = true;
        using U = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            return readString(*input.state);
        } else if constexpr (std::is_same_v<U, bool>) {
            if (input.state->offset >= input.state->size) {
                return sa::error(sa::ErrorCode::ParseError, "Unexpected end of binary bool");
            }
            return input.state->data[input.state->offset++] != 0;
        } else if constexpr (std::is_integral_v<U>) {
            return readVariable<U>(*input.state);
        } else if constexpr (std::is_floating_point_v<U>) {
            if (input.state->offset + sizeof(U) > input.state->size) {
                return sa::error(sa::ErrorCode::ParseError, "Unexpected end of binary floating-point value");
            }
            U value{};
            std::memcpy(&value, input.state->data + input.state->offset, sizeof(U));
            input.state->offset += sizeof(U);
            return value;
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported binary basic type");
        }
    }

    template <typename T>
    static sa::Result<T> toFixedBasicType(const InputValueType& input, std::size_t size) {
        return readFixed<T>(input, size);
    }

    template <typename T>
    static sa::Result<T> readFixed(const InputValueType& input, std::size_t size) {
        if (auto error = validate(input); error.has_value()) {
            return *error;
        }
        if (input.state->offset + size > input.state->size) {
            return sa::error(sa::ErrorCode::ParseError, "Unexpected end of fixed-length binary value");
        }
        const auto begin = input.state->offset;
        input.frame->consumed = true;
        if (size == sizeof(T)) {
            T value{};
            std::memcpy(&value, input.state->data + input.state->offset, size);
            input.state->offset += size;
            if constexpr (std::is_integral_v<T> && sizeof(T) > 1) {
                value = betoh(value);
            }
            return value;
        } else {
            auto value = toBasicType<T>(InputValueType{input.state, std::make_shared<Frame>()});
            input.state->offset = begin + size;
            return value;
        }
    }

    static sa::Result<InputArrayType> toArray(const InputValueType& input) {
        return initializeContainer(input, Frame::Kind::Array);
    }

    static sa::Result<InputObjectType> toObject(const InputValueType& input) {
        return initializeContainer(input, Frame::Kind::Object);
    }

private:
    static std::optional<sa::Error> validate(const InputValueType& input) {
        if (input.frame->error) {
            return input.frame->error;
        }
        if (input.frame->consumed) {
            return sa::error(sa::ErrorCode::ParseError, "Binary value was already consumed");
        }
        if (input.state == nullptr || input.state->data == nullptr) {
            return sa::error(sa::ErrorCode::ParseError, "Binary input handle is null");
        }
        return std::nullopt;
    }

    static sa::Result<InputValueType> initializeContainer(const InputValueType& input, Frame::Kind kind) {
        if (input.frame->error) {
            return *input.frame->error;
        }
        if (input.frame->initialized) {
            if (input.frame->kind != kind) {
                return sa::error(sa::ErrorCode::InvalidType, "Binary container kind mismatch");
            }
            return input;
        }
        auto size = readVariable<std::size_t>(*input.state);
        if (!size) {
            return size.error();
        }
        input.frame->kind = kind;
        input.frame->size = size.value();
        input.frame->initialized = true;
        return input;
    }

    template <typename T>
    static sa::Result<T> readVariable(State& state) {
        if (state.offset >= state.size) {
            return sa::error(sa::ErrorCode::ParseError, "Unexpected end of binary integer");
        }
        const auto flags = static_cast<unsigned char>(state.data[state.offset]) & 0x80U;
        using Decode = std::conditional_t<std::is_signed_v<T>, std::make_unsigned_t<T>, T>;
        Decode magnitude{};
        const auto consumed =
            detail::IntegerDecoder::decode(reinterpret_cast<const std::uint8_t*>(state.data + state.offset),
                                           static_cast<int>(state.size - state.offset), magnitude, 1);
        if (consumed <= 0) {
            return sa::error(sa::ErrorCode::ParseError, "Invalid binary integer");
        }
        state.offset += static_cast<std::size_t>(consumed);
        if constexpr (std::is_signed_v<T>) {
            if (flags != 0U) {
                if (magnitude > static_cast<Decode>(std::numeric_limits<T>::max()) + 1U) {
                    return sa::error(sa::ErrorCode::InvalidType, "Signed binary integer out of range");
                }
                if (magnitude == static_cast<Decode>(std::numeric_limits<T>::max()) + 1U) {
                    return std::numeric_limits<T>::min();
                }
                return static_cast<T>(-static_cast<T>(magnitude));
            }
            if (magnitude > static_cast<Decode>(std::numeric_limits<T>::max())) {
                return sa::error(sa::ErrorCode::InvalidType, "Signed binary integer out of range");
            }
        } else if (flags != 0U) {
            return sa::error(sa::ErrorCode::InvalidType, "Negative binary integer cannot be read as unsigned");
        }
        return static_cast<T>(magnitude);
    }

    static sa::Result<std::string> readString(State& state) {
        auto size = readVariable<std::size_t>(state);
        if (!size) {
            return size.error();
        }
        if (size.value() > state.size - state.offset) {
            return sa::error(sa::ErrorCode::ParseError, "Binary string exceeds remaining input");
        }
        std::string value{state.data + state.offset, size.value()};
        state.offset += size.value();
        return value;
    }

private:
    State mState;
};

} // namespace binary
NEKO_END_NAMESPACE
