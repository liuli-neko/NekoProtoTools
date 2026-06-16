/**
 * @file xml_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief pugixml-backed XML serializer.
 * @version 0.2
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_PUGIXML)

#include "nekoproto/global/reflect.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"
#include "nekoproto/serialization/xml/pugi_xml_reader.hpp"
#include "nekoproto/serialization/xml/pugi_xml_writer.hpp"

#include <pugixml.hpp>

#include <cctype>
#include <istream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE

namespace detail {

inline bool isValidXmlName(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    const auto isStart    = [](unsigned char ch) { return std::isalpha(ch) != 0 || ch == '_' || ch == ':'; };
    const auto isContinue = [&](unsigned char ch) {
        return isStart(ch) || std::isdigit(ch) != 0 || ch == '-' || ch == '.';
    };
    if (!isStart(static_cast<unsigned char>(name.front()))) {
        return false;
    }
    for (const char ch : name.substr(1)) {
        if (!isContinue(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

template <typename T>
std::string defaultXmlRootName() {
    constexpr auto name = class_nameof<std::remove_cvref_t<T>>;
    if (isValidXmlName(name)) {
        return std::string{name};
    }
    return "root";
}

template <typename BufferT>
void appendXml(BufferT& buffer, std::string_view xml) {
    if constexpr (requires { buffer.insert(buffer.end(), xml.begin(), xml.end()); }) {
        buffer.insert(buffer.end(), xml.begin(), xml.end());
    } else if constexpr (requires { buffer.write(xml.data(), static_cast<std::streamsize>(xml.size())); }) {
        buffer.write(xml.data(), static_cast<std::streamsize>(xml.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported XML output buffer");
    }
}

} // namespace detail

struct PugiXmlBackend {
    using Reader              = xml::Reader;
    using Writer              = xml::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) noexcept : buffer(outputBuffer) {}

        OutputState(BufferT& outputBuffer, std::string rootName, std::string indent = "    ") noexcept
            : buffer(outputBuffer),
              configuredRootName(std::move(rootName)),
              indentation(std::move(indent)) {}

        BufferT& buffer;
        xml::Writer writer;
        std::string configuredRootName;
        std::string indentation = "    ";
        bool hasRoot            = false;
        bool flushed            = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const char* buffer, std::size_t size) noexcept { parse(buffer, size); }

        explicit InputState(std::istream& stream) noexcept {
            const std::string input{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
            parse(input.data(), input.size());
        }

        void parse(const char* buffer, std::size_t size) noexcept {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            const auto parseResult = document.load_buffer(buffer, size, pugi::parse_default, pugi::encoding_utf8);
            if (!parseResult) {
                result = sa::error(sa::ErrorCode::ParseError,
                                   "pugixml parse error at offset " + std::to_string(parseResult.offset) + ": " +
                                       parseResult.description());
                return;
            }
            root = document.document_element();
            if (!root) {
                result = sa::error(sa::ErrorCode::ParseError, "XML document has no root element");
            }
        }

        pugi::xml_document document;
        pugi::xml_node root;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static sa::Result<void> write(OutputState<BufferT>& state, const T& value) {
        const auto rootName = state.configuredRootName.empty() ? detail::defaultXmlRootName<T>() :
                                                                 state.configuredRootName;
        if (!detail::isValidXmlName(rootName)) {
            state.hasRoot = false;
            return sa::error(sa::ErrorCode::InvalidField, "Invalid XML root name '" + rootName + "'");
        }
        state.writer.reset(rootName);
        auto result =
            detail::parser_write<xml::Reader, xml::Writer>(state.writer, value, parsing::Parent<xml::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result);
        state.flushed = false;
        return result;
    }

    template <typename BufferT>
    static sa::Result<void> finish(OutputState<BufferT>& state, sa::Result<void> result) {
        if (!state.hasRoot || !result) {
            return result;
        }
        if (!state.flushed) {
            const auto output = state.writer.str(state.indentation);
            detail::appendXml(state.buffer, output);
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static bool outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept {
        return state.hasRoot && static_cast<bool>(result);
    }

    template <typename SourceT>
    static sa::Result<void> inputResult(const InputState<SourceT>& state) {
        return state.result;
    }

    template <typename SourceT, typename T>
    static sa::Result<void> read(InputState<SourceT>& state, T& value) {
        return detail::parser_read<xml::Reader, xml::Writer>(xml::Reader::InputValueType{state.root, true}, value);
    }
};

template <typename BufferT = PugiXmlBackend::DefaultOutputBuffer>
class PugiXmlOutputSerializer : public detail::OutputSerializerAdapter<PugiXmlBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<PugiXmlBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
PugiXmlOutputSerializer(BufferT&) -> PugiXmlOutputSerializer<BufferT>;

template <typename BufferT>
PugiXmlOutputSerializer(BufferT&, std::string, std::string) -> PugiXmlOutputSerializer<BufferT>;

template <typename BufferT>
PugiXmlOutputSerializer(BufferT&, std::string) -> PugiXmlOutputSerializer<BufferT>;

template <typename SourceT = PugiXmlBackend::DefaultInputSource>
class PugiXmlInputSerializer : public detail::InputSerializerAdapter<PugiXmlBackend, SourceT> {
public:
    using Base = detail::InputSerializerAdapter<PugiXmlBackend, SourceT>;
    using Base::Base;
};

PugiXmlInputSerializer(const char*, std::size_t) -> PugiXmlInputSerializer<>;
PugiXmlInputSerializer(std::istream&) -> PugiXmlInputSerializer<>;

struct PugiXmlSerializer {
    using OutputSerializer = PugiXmlOutputSerializer<>;
    using InputSerializer  = PugiXmlInputSerializer<>;
    using Reader           = xml::Reader;
    using Writer           = xml::Writer;
};

using XmlSerializer = PugiXmlSerializer;

NEKO_END_NAMESPACE

#endif
