#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/binary/binary_writer.hpp"
#include "nekoproto/serialization/binary/endian.hpp"
#include "nekoproto/serialization/error.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace binary {

struct ParseLimits {
    std::size_t max_input_bytes           = 16U * 1024U * 1024U;
    std::size_t max_string_bytes          = 4U * 1024U * 1024U;
    std::size_t max_container_elements    = 1U * 1024U * 1024U;
    std::size_t max_object_fields         = 64U * 1024U;
    std::size_t max_depth                 = 64U;
    std::size_t max_total_allocated_bytes = 32U * 1024U * 1024U;
};

class Reader {
private:
    struct State;
    struct Node;

    struct Member {
        std::string_view name;
        std::uint32_t id = 0;
        std::shared_ptr<Node> value;
    };

    struct Node {
        ValueTag tag = ValueTag::Null;
        bool raw = false;
        bool consumed = false;
        std::size_t begin = 0;
        std::size_t dataBegin = 0;
        std::size_t dataEnd = 0;
        std::size_t end = 0;
        std::size_t depth = 0;
        std::vector<std::shared_ptr<Node>> elements;
        std::vector<Member> members;
        std::unordered_map<std::string_view, std::size_t> namedIndex;
        std::unordered_map<std::uint32_t, std::size_t> idIndex;
        std::optional<sa::Error> error;
    };

    struct State {
        const char* data = nullptr;
        std::size_t size = 0;
        std::size_t offset = 0;
        ParseLimits limits;
        std::size_t remainingAllocation = 0;
        bool framed = false;
        std::optional<sa::Error> error;
    };

public:
    struct InputValue {
        State* state = nullptr;
        std::shared_ptr<Node> node;
    };

    using InputValueType  = InputValue;
    using InputArrayType  = InputValue;
    using InputObjectType = InputValue;

    /**
     * @brief Restorable read state used when probing an untagged union.
     *
     * Binary reads are destructive: successful scalar conversions mark nodes
     * consumed and raw reads advance the shared byte cursor. The generic union
     * parser uses this checkpoint to test every alternative without allowing a
     * failed or ambiguous probe to alter the real input state.
     */
    class Checkpoint {
        friend class Reader;

        struct Entry {
            std::shared_ptr<Node> node;
            bool consumed = false;
            std::size_t end = 0;
        };

        State* state = nullptr;
        std::size_t offset = 0;
        std::size_t remainingAllocation = 0;
        std::vector<Entry> entries;
    };

    static Checkpoint checkpoint(const InputValueType& input) {
        Checkpoint result;
        result.state = input.state;
        if (input.state != nullptr) {
            result.offset              = input.state->offset;
            result.remainingAllocation = input.state->remainingAllocation;
        }
        _captureCheckpoint(input.node, result);
        return result;
    }

    static void restore(const Checkpoint& checkpoint) {
        if (checkpoint.state != nullptr) {
            checkpoint.state->offset              = checkpoint.offset;
            checkpoint.state->remainingAllocation = checkpoint.remainingAllocation;
        }
        for (const auto& entry : checkpoint.entries) {
            entry.node->consumed = entry.consumed;
            entry.node->end      = entry.end;
        }
    }

    Reader(const char* data, std::size_t size, ParseLimits limits = {})
        : mState{.data = data,
                 .size = size,
                 .offset = 0,
                 .limits = limits,
                 .remainingAllocation = limits.max_total_allocated_bytes,
                 .framed = false,
                 .error = std::nullopt} {
        if (data == nullptr) {
            mState.error = sa::error(sa::ErrorCode::ParseError, "Binary input handle is null");
            return;
        }
        if (size > limits.max_input_bytes) {
            mState.error = sa::error(sa::ErrorCode::InvalidLength, "Binary input exceeds configured byte limit");
            return;
        }
        if (size >= sizeof(BinaryMagic) && std::memcmp(data, BinaryMagic, sizeof(BinaryMagic)) == 0) {
            mState.framed = true;
            mState.offset = sizeof(BinaryMagic);
        }
    }

    sa::Result<void> inputResult() const {
        return mState.error ? sa::Result<void>{*mState.error} : sa::success();
    }

    void beginRawFixedDataAsRoot() noexcept {
        mState.framed = false;
        mState.offset = 0;
    }

    sa::Result<void> finish() const {
        if (mState.error) return *mState.error;
        if (mState.offset != mState.size) {
            return sa::error(sa::ErrorCode::ParseError, "Binary input contains trailing or unconsumed bytes");
        }
        return sa::success();
    }

    InputValueType root() {
        if (mState.error) return _errorValue(mState, *mState.error);
        if (!mState.framed) return _rawValue(mState);
        auto cursor = mState.offset;
        auto parsed = _parseNode(mState, cursor, mState.size, 0);
        if (!parsed) return _errorValue(mState, parsed.error());
        mState.offset = cursor;
        return {&mState, std::move(parsed.value())};
    }

    static InputValueType next(const InputValueType& input) {
        if (auto error = _validateHandle(input); error) return _errorValue(input.state, *error);
        if (input.node->raw) return _rawValue(*input.state);
        auto cursor = input.node->end;
        auto parsed = _parseNode(*input.state, cursor, input.state->size, input.node->depth);
        if (!parsed) return _errorValue(input.state, parsed.error());
        input.state->offset = cursor;
        return {input.state, std::move(parsed.value())};
    }

    std::size_t offset() const noexcept { return mState.offset; }
    std::size_t size() const noexcept { return mState.size; }

    static bool isRaw(const InputValueType& input) noexcept {
        return input.state != nullptr && input.node != nullptr && input.node->raw;
    }

    static bool isFramedObject(const InputValueType& input) noexcept {
        return input.state != nullptr && input.node != nullptr && !input.node->raw &&
               (input.node->tag == ValueTag::NamedObject || input.node->tag == ValueTag::IdObject);
    }

    static std::size_t arraySize(const InputArrayType& array) noexcept {
        return array.node == nullptr ? 0U : array.node->elements.size();
    }

    static InputValueType arrayElement(const InputArrayType& array, std::size_t index) {
        if (auto error = _validateHandle(array); error) return _errorValue(array.state, *error);
        if (array.node->tag != ValueTag::Array || index >= array.node->elements.size()) {
            return _errorValue(array.state, sa::error(sa::ErrorCode::InvalidIndex, "Binary array index is out of range"));
        }
        return {array.state, array.node->elements[index]};
    }

    static std::size_t objectSize(const InputObjectType& object) noexcept {
        return object.node == nullptr ? 0U : object.node->members.size();
    }

    static sa::Result<InputValueType> objectField(const InputObjectType& object, std::string_view name) {
        if (auto error = _validateHandle(object); error) return *error;
        std::size_t index = 0;
        if (object.node->tag == ValueTag::NamedObject) {
            const auto item = object.node->namedIndex.find(name);
            if (item == object.node->namedIndex.end()) return _missingField(name);
            index = item->second;
        } else if (object.node->tag == ValueTag::IdObject) {
            if (useHashedFieldId(name)) {
                const auto item = object.node->idIndex.find(fieldId(name));
                if (item == object.node->idIndex.end()) return _missingField(name);
                index = item->second;
            } else {
                const auto item = object.node->namedIndex.find(name);
                if (item == object.node->namedIndex.end()) return _missingField(name);
                index = item->second;
            }
        } else {
            return _typeError<InputValueType>("object");
        }
        return InputValueType{object.state, object.node->members[index].value};
    }

    template <typename Fn>
    static bool forEachObjectMember(const InputObjectType& object, Fn&& fn) {
        if (_validateHandle(object) || object.node->tag != ValueTag::NamedObject) return false;
        for (const auto& member : object.node->members) {
            if (!fn(member.name, InputValueType{object.state, member.value})) return false;
        }
        return true;
    }

    static bool isEmpty(const InputValueType& input) {
        if (_validate(input) || input.node->raw || input.node->tag != ValueTag::Null) return false;
        input.node->consumed = true;
        return true;
    }

    template <typename T>
    static sa::Result<T> toBasicType(const InputValueType& input) {
        if (auto error = _validate(input); error) return *error;
        using U = std::remove_cvref_t<T>;
        if (input.node->raw) return _readRaw<U>(input);
        if constexpr (std::is_same_v<U, std::string>) {
            if (input.node->tag != ValueTag::String) return _typeError<U>("string");
            const auto size = input.node->dataEnd - input.node->dataBegin;
            if (auto error = _charge(*input.state, size); error) return *error;
            input.node->consumed = true;
            return std::string{input.state->data + input.node->dataBegin, size};
        } else if constexpr (std::is_same_v<U, bool>) {
            if (input.node->tag != ValueTag::False && input.node->tag != ValueTag::True) return _typeError<U>("bool");
            input.node->consumed = true;
            return input.node->tag == ValueTag::True;
        } else if constexpr (std::is_integral_v<U>) {
            const auto expected = std::is_signed_v<U> ? ValueTag::SignedInteger : ValueTag::UnsignedInteger;
            if (input.node->tag != expected) {
                return _typeError<U>(std::is_signed_v<U> ? "signed integer" : "unsigned integer");
            }
            auto cursor = input.node->dataBegin;
            using Unsigned = std::make_unsigned_t<U>;
            auto decoded = _readUleb<Unsigned>(*input.state, cursor, input.node->dataEnd);
            if (!decoded) return decoded.error();
            input.node->consumed = true;
            if constexpr (std::is_signed_v<U>) {
                const auto encoded = decoded.value();
                const auto magnitude = static_cast<Unsigned>((encoded >> 1U) + (encoded & Unsigned{1}));
                const bool negative = (encoded & Unsigned{1}) != 0;
                if (negative) {
                    const auto minimumMagnitude = static_cast<Unsigned>(std::numeric_limits<U>::max()) + Unsigned{1};
                    if (magnitude > minimumMagnitude) return _rangeError<U>();
                    if (magnitude == minimumMagnitude) return std::numeric_limits<U>::min();
                    return static_cast<U>(-static_cast<U>(magnitude));
                }
                if (magnitude > static_cast<Unsigned>(std::numeric_limits<U>::max())) return _rangeError<U>();
                return static_cast<U>(magnitude);
            } else {
                return static_cast<U>(decoded.value());
            }
        } else if constexpr (std::is_floating_point_v<U>) {
            return _readFloating<U>(input);
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
        if (auto error = _validate(input); error) return *error;
        using U = std::remove_cvref_t<T>;
        if (input.node->raw) return _readRawFixed<U>(input, size);
        if constexpr (std::is_same_v<U, bool>) {
            return toBasicType<U>(input);
        } else if constexpr (std::is_integral_v<U>) {
            if (size != sizeof(U) || input.node->tag != _fixedTag<U>()) return _typeError<U>("fixed-width integer");
            U value{};
            std::memcpy(&value, input.state->data + input.node->dataBegin, sizeof(U));
            if constexpr (sizeof(U) > 1) value = betoh(value);
            input.node->consumed = true;
            return value;
        } else if constexpr (std::is_floating_point_v<U>) {
            if (size != sizeof(U)) return sa::error(sa::ErrorCode::InvalidLength, "Invalid fixed floating-point width");
            return _readFloating<U>(input);
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported fixed binary type");
        }
    }

    static sa::Result<InputArrayType> toArray(const InputValueType& input) {
        if (auto error = _validate(input); error) return *error;
        if (input.node->raw || input.node->tag != ValueTag::Array) return _typeError<InputArrayType>("array");
        return input;
    }

    static sa::Result<InputObjectType> toObject(const InputValueType& input) {
        if (auto error = _validate(input); error) return *error;
        if (!isFramedObject(input)) return _typeError<InputObjectType>("object");
        return input;
    }

private:
    static void _captureCheckpoint(const std::shared_ptr<Node>& node, Checkpoint& checkpoint) {
        if (node == nullptr) return;
        checkpoint.entries.push_back({node, node->consumed, node->end});
        for (const auto& element : node->elements) {
            _captureCheckpoint(element, checkpoint);
        }
        for (const auto& member : node->members) {
            _captureCheckpoint(member.value, checkpoint);
        }
    }

    static std::optional<sa::Error> _validateHandle(const InputValueType& input) {
        if (input.state == nullptr || input.node == nullptr || input.state->data == nullptr) {
            return sa::error(sa::ErrorCode::ParseError, "Binary input handle is null");
        }
        if (input.state->error) return input.state->error;
        if (input.node->error) return input.node->error;
        return std::nullopt;
    }

    static std::optional<sa::Error> _validate(const InputValueType& input) {
        if (auto error = _validateHandle(input); error) return error;
        if (input.node->consumed) return sa::error(sa::ErrorCode::ParseError, "Binary value was already consumed");
        return std::nullopt;
    }

    static InputValueType _errorValue(State* state, sa::Error error) {
        InputValueType value;
        value.state = state;
        value.node = std::make_shared<Node>();
        value.node->error = std::move(error);
        return value;
    }
    static InputValueType _errorValue(State& state, sa::Error error) { return _errorValue(&state, std::move(error)); }

    static InputValueType _rawValue(State& state) {
        InputValueType value;
        value.state = &state;
        value.node = std::make_shared<Node>();
        value.node->raw = true;
        value.node->begin = state.offset;
        value.node->dataBegin = state.offset;
        value.node->dataEnd = state.size;
        value.node->end = state.offset;
        if (state.offset >= state.size) {
            value.node->error = sa::error(sa::ErrorCode::ParseError, "Unexpected end of raw fixed binary data");
        }
        return value;
    }

    static sa::Result<std::shared_ptr<Node>> _parseNode(State& state, std::size_t& cursor, std::size_t limit,
                                                        std::size_t depth) {
        if (depth > state.limits.max_depth) {
            return sa::error(sa::ErrorCode::InvalidLength, "Binary nesting exceeds configured depth limit");
        }
        if (cursor >= limit || limit > state.size) {
            return sa::error(sa::ErrorCode::ParseError, "Unexpected end of binary value");
        }
        if (auto error = _charge(state, sizeof(Node)); error) return *error;
        auto node = std::make_shared<Node>();
        node->begin = cursor;
        node->depth = depth;
        const auto rawTag = static_cast<std::uint8_t>(state.data[cursor++]);
        if (rawTag > static_cast<std::uint8_t>(ValueTag::FixedUnsigned64)) {
            return sa::error(sa::ErrorCode::InvalidType, "Unknown binary value tag");
        }
        node->tag = static_cast<ValueTag>(rawTag);
        switch (node->tag) {
        case ValueTag::Null:
        case ValueTag::False:
        case ValueTag::True:
            break;
        case ValueTag::SignedInteger:
        case ValueTag::UnsignedInteger: {
            node->dataBegin = cursor;
            auto ignored = _readUleb<std::uint64_t>(state, cursor, limit);
            if (!ignored) return ignored.error();
            node->dataEnd = cursor;
            break;
        }
        case ValueTag::Float32:
            if (!_takeFixed(*node, cursor, limit, sizeof(std::uint32_t))) return _truncatedFixed();
            break;
        case ValueTag::Float64:
            if (!_takeFixed(*node, cursor, limit, sizeof(std::uint64_t))) return _truncatedFixed();
            break;
        case ValueTag::String: {
            auto size = _readUleb<std::uint64_t>(state, cursor, limit);
            if (!size) return size.error();
            if (size.value() > state.limits.max_string_bytes || size.value() > limit - cursor) {
                return sa::error(sa::ErrorCode::InvalidLength, "Binary string exceeds configured byte limit");
            }
            node->dataBegin = cursor;
            cursor += static_cast<std::size_t>(size.value());
            node->dataEnd = cursor;
            break;
        }
        case ValueTag::Array: {
            auto count = _readCount(state, cursor, limit, state.limits.max_container_elements, "array");
            if (!count) return count.error();
            if (auto error = _chargeProduct(state, count.value(), sizeof(std::shared_ptr<Node>)); error) return *error;
            node->elements.reserve(count.value());
            for (std::size_t ix = 0; ix < count.value(); ++ix) {
                auto child = _parseNode(state, cursor, limit, depth + 1U);
                if (!child) return child.error();
                node->elements.push_back(std::move(child.value()));
            }
            break;
        }
        case ValueTag::NamedObject:
        case ValueTag::IdObject: {
            auto count = _readCount(state, cursor, limit, state.limits.max_object_fields, "object");
            if (!count) return count.error();
            if (auto error = _chargeProduct(state, count.value(), sizeof(Member) + sizeof(std::size_t)); error) {
                return *error;
            }
            node->members.reserve(count.value());
            if (node->tag == ValueTag::NamedObject) node->namedIndex.reserve(count.value());
            else {
                node->namedIndex.reserve(count.value());
                node->idIndex.reserve(count.value());
            }
            for (std::size_t ix = 0; ix < count.value(); ++ix) {
                Member member;
                if (node->tag == ValueTag::NamedObject) {
                    auto size = _readUleb<std::uint64_t>(state, cursor, limit);
                    if (!size) return size.error();
                    if (size.value() > state.limits.max_string_bytes || size.value() > limit - cursor) {
                        return sa::error(sa::ErrorCode::InvalidLength, "Binary field name exceeds configured limit");
                    }
                    member.name = {state.data + cursor, static_cast<std::size_t>(size.value())};
                    cursor += static_cast<std::size_t>(size.value());
                    if (node->namedIndex.contains(member.name)) {
                        return sa::error(sa::ErrorCode::InvalidField, "Binary object contains a duplicate field name");
                    }
                } else {
                    auto key = _readUleb<std::uint64_t>(state, cursor, limit);
                    if (!key) return key.error();
                    if ((key.value() & 1U) == 0U) {
                        const auto nameSize64 = key.value() >> 1U;
                        if (nameSize64 > state.limits.max_string_bytes || nameSize64 > limit - cursor) {
                            return sa::error(sa::ErrorCode::InvalidLength,
                                             "Binary reflected field name exceeds configured limit");
                        }
                        member.name = {state.data + cursor, static_cast<std::size_t>(nameSize64)};
                        cursor += static_cast<std::size_t>(nameSize64);
                        if (node->namedIndex.contains(member.name)) {
                            return sa::error(sa::ErrorCode::InvalidField,
                                             "Binary object contains a duplicate reflected field name");
                        }
                    } else {
                        const auto id64 = key.value() >> 1U;
                        if (id64 > std::numeric_limits<std::uint32_t>::max()) {
                            return sa::error(sa::ErrorCode::InvalidField, "Binary reflected field id is out of range");
                        }
                        member.id = static_cast<std::uint32_t>(id64);
                        if (node->idIndex.contains(member.id)) {
                            return sa::error(sa::ErrorCode::InvalidField,
                                             "Binary object contains a duplicate reflected field id");
                        }
                    }
                }
                auto child = _parseNode(state, cursor, limit, depth + 1U);
                if (!child) return child.error();
                member.value = std::move(child.value());
                const auto memberIndex = node->members.size();
                if (node->tag == ValueTag::NamedObject) node->namedIndex.emplace(member.name, memberIndex);
                else if (member.name.empty()) node->idIndex.emplace(member.id, memberIndex);
                else node->namedIndex.emplace(member.name, memberIndex);
                node->members.push_back(std::move(member));
            }
            break;
        }
        default: {
            const auto width = _fixedWidth(node->tag);
            if (width == 0U || !_takeFixed(*node, cursor, limit, width)) return _truncatedFixed();
            break;
        }
        }
        node->end = cursor;
        return node;
    }

    static bool _takeFixed(Node& node, std::size_t& cursor, std::size_t limit, std::size_t width) {
        if (width > limit - cursor) return false;
        node.dataBegin = cursor;
        cursor += width;
        node.dataEnd = cursor;
        return true;
    }

    static std::size_t _fixedWidth(ValueTag tag) noexcept {
        switch (tag) {
        case ValueTag::FixedSigned8:
        case ValueTag::FixedUnsigned8: return 1;
        case ValueTag::FixedSigned16:
        case ValueTag::FixedUnsigned16: return 2;
        case ValueTag::FixedSigned32:
        case ValueTag::FixedUnsigned32: return 4;
        case ValueTag::FixedSigned64:
        case ValueTag::FixedUnsigned64: return 8;
        default: return 0;
        }
    }

    template <typename UInt>
    static sa::Result<UInt> _readUleb(const State& state, std::size_t& cursor, std::size_t limit) {
        static_assert(std::is_unsigned_v<UInt>);
        const auto begin = cursor;
        UInt value = 0;
        unsigned shift = 0;
        while (cursor < limit) {
            const auto byte = static_cast<std::uint8_t>(state.data[cursor++]);
            const auto chunk = static_cast<UInt>(byte & 0x7FU);
            if (shift >= std::numeric_limits<UInt>::digits || chunk > (std::numeric_limits<UInt>::max() >> shift)) {
                return sa::error(sa::ErrorCode::InvalidType, "Binary varint is out of range");
            }
            value |= static_cast<UInt>(chunk << shift);
            if ((byte & 0x80U) == 0) {
                if (cursor - begin > 1U && chunk == 0) {
                    return sa::error(sa::ErrorCode::ParseError, "Binary varint is not minimally encoded");
                }
                return value;
            }
            shift += 7U;
        }
        return sa::error(sa::ErrorCode::ParseError, "Truncated binary varint");
    }

    static sa::Result<std::size_t> _readCount(const State& state, std::size_t& cursor, std::size_t limit,
                                               std::size_t maximum, std::string_view kind) {
        auto count = _readUleb<std::uint64_t>(state, cursor, limit);
        if (!count) return count.error();
        if (count.value() > maximum || count.value() > std::numeric_limits<std::size_t>::max()) {
            return sa::error(sa::ErrorCode::InvalidLength,
                             "Binary " + std::string(kind) + " exceeds configured member limit");
        }
        return static_cast<std::size_t>(count.value());
    }

    template <typename U>
    static sa::Result<U> _readRaw(const InputValueType& input) {
        if constexpr (std::is_same_v<U, std::string>) {
            auto cursor = input.state->offset;
            auto size = _readUleb<std::uint64_t>(*input.state, cursor, input.state->size);
            if (!size) return size.error();
            if (size.value() > input.state->limits.max_string_bytes || size.value() > input.state->size - cursor) {
                return sa::error(sa::ErrorCode::InvalidLength, "Raw binary string exceeds configured limit");
            }
            if (auto error = _charge(*input.state, static_cast<std::size_t>(size.value())); error) return *error;
            std::string value{input.state->data + cursor, static_cast<std::size_t>(size.value())};
            cursor += static_cast<std::size_t>(size.value());
            _consumeRaw(input, cursor);
            return value;
        } else if constexpr (std::is_arithmetic_v<U>) {
            return _readRawFixed<U>(input, sizeof(U));
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported raw binary type");
        }
    }

    template <typename U>
    static sa::Result<U> _readRawFixed(const InputValueType& input, std::size_t size) {
        if (size != sizeof(U) || size > input.state->size - input.state->offset) {
            return sa::error(sa::ErrorCode::ParseError, "Unexpected end of raw fixed binary data");
        }
        if constexpr (std::is_same_v<U, bool>) {
            const auto byte = static_cast<std::uint8_t>(input.state->data[input.state->offset]);
            _consumeRaw(input, input.state->offset + 1U);
            if (byte > 1U) return sa::error(sa::ErrorCode::ParseError, "Raw binary bool must be 0 or 1");
            return byte != 0U;
        } else if constexpr (std::is_floating_point_v<U> && !std::is_same_v<U, float> &&
                             !std::is_same_v<U, double>) {
            return sa::error(sa::ErrorCode::InvalidType,
                             "Raw fixed binary supports only IEEE-754 float and double");
        }
        U value{};
        std::memcpy(&value, input.state->data + input.state->offset, size);
        const auto end = input.state->offset + size;
        _consumeRaw(input, end);
        if constexpr (std::is_integral_v<U> && sizeof(U) > 1) {
            value = betoh(value);
        } else if constexpr (std::is_same_v<U, float>) {
            auto bits = std::bit_cast<std::uint32_t>(value);
            value = std::bit_cast<float>(betoh(bits));
        } else if constexpr (std::is_same_v<U, double>) {
            auto bits = std::bit_cast<std::uint64_t>(value);
            value = std::bit_cast<double>(betoh(bits));
        }
        return value;
    }

    static void _consumeRaw(const InputValueType& input, std::size_t end) {
        input.state->offset = end;
        input.node->end = end;
        input.node->consumed = true;
    }

    template <typename U>
    static sa::Result<U> _readFloating(const InputValueType& input) {
        static_assert(std::numeric_limits<U>::is_iec559, "Binary floating-point requires IEEE-754");
        if constexpr (std::is_same_v<U, float>) {
            if (input.node->tag != ValueTag::Float32) return _typeError<U>("float32");
            std::uint32_t bits{};
            std::memcpy(&bits, input.state->data + input.node->dataBegin, sizeof(bits));
            input.node->consumed = true;
            return std::bit_cast<float>(betoh(bits));
        } else if constexpr (std::is_same_v<U, double>) {
            if (input.node->tag != ValueTag::Float64) return _typeError<U>("float64");
            std::uint64_t bits{};
            std::memcpy(&bits, input.state->data + input.node->dataBegin, sizeof(bits));
            input.node->consumed = true;
            return std::bit_cast<double>(betoh(bits));
        } else {
            static_assert(std::is_same_v<U, void>, "Binary V2 supports only IEEE-754 float and double");
        }
    }

    template <typename U>
    static consteval ValueTag _fixedTag() {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (sizeof(U) == 1) return ValueTag::FixedSigned8;
            else if constexpr (sizeof(U) == 2) return ValueTag::FixedSigned16;
            else if constexpr (sizeof(U) == 4) return ValueTag::FixedSigned32;
            else if constexpr (sizeof(U) == 8) return ValueTag::FixedSigned64;
            else static_assert(std::is_same_v<U, void>, "Unsupported fixed-width integer type");
        } else {
            if constexpr (sizeof(U) == 1) return ValueTag::FixedUnsigned8;
            else if constexpr (sizeof(U) == 2) return ValueTag::FixedUnsigned16;
            else if constexpr (sizeof(U) == 4) return ValueTag::FixedUnsigned32;
            else if constexpr (sizeof(U) == 8) return ValueTag::FixedUnsigned64;
            else static_assert(std::is_same_v<U, void>, "Unsupported fixed-width integer type");
        }
    }

    static std::optional<sa::Error> _charge(State& state, std::size_t bytes) {
        if (bytes > state.remainingAllocation) {
            return sa::error(sa::ErrorCode::InvalidLength, "Binary parse allocation budget exceeded");
        }
        state.remainingAllocation -= bytes;
        return std::nullopt;
    }
    static std::optional<sa::Error> _chargeProduct(State& state, std::size_t count, std::size_t elementSize) {
        if (count != 0U && elementSize > std::numeric_limits<std::size_t>::max() / count) {
            return sa::error(sa::ErrorCode::InvalidLength, "Binary parse allocation size overflow");
        }
        return _charge(state, count * elementSize);
    }

    template <typename T>
    static sa::Result<T> _typeError(std::string_view expected) {
        return sa::error(sa::ErrorCode::InvalidType, "Binary value is not a " + std::string(expected));
    }
    template <typename T>
    static sa::Result<T> _rangeError() {
        return sa::error(sa::ErrorCode::InvalidType, "Binary integer is out of range");
    }
    static sa::Result<InputValueType> _missingField(std::string_view name) {
        return sa::error(sa::ErrorCode::InvalidField,
                         "Binary object does not contain field '" + std::string(name) + "'");
    }
    static sa::Error _truncatedFixed() {
        return sa::error(sa::ErrorCode::ParseError, "Truncated fixed-width binary value");
    }

private:
    State mState;
};

} // namespace binary
NEKO_END_NAMESPACE
