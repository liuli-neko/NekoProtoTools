#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_LIBFYAML) || defined(NEKO_PROTO_ENABLE_YAMLCPP)

#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/serializer_adapter.hpp"

#include <istream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)
#include "nekoproto/serialization/yaml/libfyaml_reader.hpp"
#include "nekoproto/serialization/yaml/libfyaml_writer.hpp"

#include <libfyaml.h>

#include <cstdlib>
#include <cstring>
#include <utility>
#endif

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)
#include "nekoproto/serialization/yaml/yamlcpp_reader.hpp"
#include "nekoproto/serialization/yaml/yamlcpp_writer.hpp"

#include <yaml-cpp/yaml.h>

#include <sstream>
#endif

namespace nekoproto {

namespace detail {
template <typename BufferT>
void appendYaml(BufferT& buffer, std::string_view yaml) {
    if constexpr (requires { buffer.insert(buffer.end(), yaml.begin(), yaml.end()); }) {
        buffer.insert(buffer.end(), yaml.begin(), yaml.end());
    } else if constexpr (requires { buffer.write(yaml.data(), static_cast<std::streamsize>(yaml.size())); }) {
        buffer.write(yaml.data(), static_cast<std::streamsize>(yaml.size()));
    } else {
        static_assert(always_false_v<BufferT>, "Unsupported YAML output buffer");
    }
}
} // namespace detail

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)
namespace detail {
inline void quietYamlDiagOutput(fy_diag* /*diag*/, void* /*user*/, const char* /*buf*/, size_t /*len*/) {}

inline auto createQuietYamlDiag() -> fy_diag* {
    fy_diag_cfg diagCfg{};
    fy_diag_cfg_default(&diagCfg);
    diagCfg.fp        = nullptr;
    diagCfg.output_fn = quietYamlDiagOutput;
    diagCfg.colorize  = false;
    auto* diag        = fy_diag_create(&diagCfg);
    if (diag != nullptr) {
        fy_diag_set_collect_errors(diag, true);
    }
    return diag;
}

inline auto firstYamlDiagMessage(fy_diag* diag) -> std::string {
    if (diag == nullptr) {
        return "libfyaml parse error";
    }
    void* iter = nullptr;
    if (auto* error = fy_diag_errors_iterate(diag, &iter); error != nullptr && error->msg != nullptr) {
        std::string message = error->msg;
        if (error->line > 0 && error->column > 0) {
            message += " at line " + std::to_string(error->line) + ", column " + std::to_string(error->column);
        }
        return message;
    }
    return "libfyaml parse error";
}

inline auto defaultYamlParseConfig(fy_diag* diag = nullptr) -> fy_parse_cfg {
    fy_parse_cfg cfg{};
    cfg.flags = static_cast<fy_parse_cfg_flags>(FYPCF_QUIET | FYPCF_COLLECT_DIAG | FYPCF_RESOLVE_DOCUMENT |
                                                FYPCF_DEFAULT_VERSION_1_2);
    cfg.diag  = diag;
    return cfg;
}
} // namespace detail

struct LibfyamlBackend {
    using Reader              = yaml::Reader;
    using Writer              = yaml::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) : buffer(outputBuffer) { resetDocument(); }

        OutputState(const OutputState&)            = delete;
        OutputState(OutputState&&)                 = delete;
        OutputState& operator=(const OutputState&) = delete;
        OutputState& operator=(OutputState&&)      = delete;

        ~OutputState() {
            if (document != nullptr) {
                fy_document_destroy(document);
            }
            if (diag != nullptr) {
                fy_diag_destroy(diag);
            }
        }

        void resetDocument() {
            if (document != nullptr) {
                fy_document_destroy(document);
            }
            if (diag != nullptr) {
                fy_diag_destroy(diag);
            }
            diag        = detail::createQuietYamlDiag();
            parseConfig = detail::defaultYamlParseConfig(diag);
            document    = fy_document_create(&parseConfig);
            writer.reset(document);
            result  = document == nullptr ? sa::error(sa::ErrorCode::Unknown, "Could not create YAML document")
                                          : sa::success();
            hasRoot = false;
            flushed = false;
        }

        BufferT& buffer;
        fy_parse_cfg parseConfig{};
        fy_diag* diag         = nullptr;
        fy_document* document = nullptr;
        yaml::Writer writer;
        sa::Result<void> result;
        bool hasRoot = false;
        bool flushed = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const char* buffer, std::size_t size) { parse(buffer, size); }

        explicit InputState(std::istream& stream) {
            ownedInput.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
            parse(ownedInput.data(), ownedInput.size(), false);
        }

        InputState(const InputState&)            = delete;
        InputState(InputState&&)                 = delete;
        InputState& operator=(const InputState&) = delete;
        InputState& operator=(InputState&&)      = delete;

        ~InputState() {
            if (document != nullptr) {
                fy_document_destroy(document);
            }
            if (diag != nullptr) {
                fy_diag_destroy(diag);
            }
        }

        void parse(const char* buffer, std::size_t size, bool copy = true) {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (copy) {
                ownedInput.assign(buffer, size);
                buffer = ownedInput.data();
            }
            if (diag != nullptr) {
                fy_diag_destroy(diag);
            }
            diag        = detail::createQuietYamlDiag();
            parseConfig = detail::defaultYamlParseConfig(diag);
            document    = fy_document_build_from_string(&parseConfig, buffer, size);
            if (document == nullptr) {
                result = sa::error(sa::ErrorCode::ParseError, detail::firstYamlDiagMessage(diag));
                return;
            }
            root = fy_document_root(document);
            if (root == nullptr) {
                result = sa::error(sa::ErrorCode::ParseError, "YAML document has no root node");
                return;
            }
            result = sa::success();
        }

        std::string ownedInput;
        fy_parse_cfg parseConfig{};
        fy_diag* diag         = nullptr;
        fy_document* document = nullptr;
        fy_node* root         = nullptr;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        state.resetDocument();
        if (!state.result) {
            return state.result;
        }
        auto result   = parserWrite<yaml::Writer>(state.writer, value, parsing::Parent<yaml::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result) && static_cast<bool>(state.writer.result());
        if (!state.writer.result()) {
            return state.writer.result();
        }
        return result;
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!result) {
            return result;
        }
        if (!state.writer.result()) {
            return state.writer.result();
        }
        if (!state.hasRoot) {
            return result;
        }
        if (!state.flushed) {
            char* output = fy_emit_document_to_string(
                state.document, static_cast<fy_emitter_cfg_flags>(FYECF_MODE_MANUAL | FYECF_DOC_START_MARK_OFF));
            if (output == nullptr) {
                return sa::error(sa::ErrorCode::Unknown, "Could not emit YAML document");
            }
            detail::appendYaml(state.buffer, std::string_view{output, std::strlen(output)});
            std::free(output);
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static auto outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept -> bool {
        return state.hasRoot && static_cast<bool>(result) && static_cast<bool>(state.writer.result());
    }

    template <typename SourceT>
    static auto inputResult(const InputState<SourceT>& state) -> sa::Result<void> {
        return state.result;
    }

    template <typename SourceT, typename T>
    static auto read(InputState<SourceT>& state, T& value) -> sa::Result<void> {
        return parserRead<yaml::Reader>(state.root, value);
    }
};

template <typename BufferT = LibfyamlBackend::DefaultOutputBuffer>
class LibfyamlOutputSerializer : public detail::OutputSerializerAdapter<LibfyamlBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<LibfyamlBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
LibfyamlOutputSerializer(BufferT&) -> LibfyamlOutputSerializer<BufferT>;

template <typename SourceT = LibfyamlBackend::DefaultInputSource>
class LibfyamlInputSerializer : public detail::InputSerializerAdapter<LibfyamlBackend, SourceT> {
public:
    using Base = detail::InputSerializerAdapter<LibfyamlBackend, SourceT>;
    using Base::Base;
};

LibfyamlInputSerializer(const char*, std::size_t) -> LibfyamlInputSerializer<>;
LibfyamlInputSerializer(std::istream&) -> LibfyamlInputSerializer<>;

struct LibfyamlSerializer {
    using OutputSerializer = LibfyamlOutputSerializer<>;
    using InputSerializer  = LibfyamlInputSerializer<>;
    using Reader           = yaml::Reader;
    using Writer           = yaml::Writer;
};
#endif

#if defined(NEKO_PROTO_ENABLE_YAMLCPP)
struct YamlCppBackend {
    using Reader              = yamlcpp::Reader;
    using Writer              = yamlcpp::Writer;
    using DefaultOutputBuffer = std::vector<char>;
    using DefaultInputSource  = void;

    template <typename BufferT>
    struct OutputState {
        explicit OutputState(BufferT& outputBuffer) : buffer(outputBuffer), writer(&document) {}

        BufferT& buffer;
        YAML::Node document;
        yamlcpp::Writer writer;
        bool hasRoot = false;
        bool flushed = false;
    };

    template <typename SourceT>
    struct InputState {
        explicit InputState(const char* buffer, std::size_t size) { parse(buffer, size); }

        explicit InputState(std::istream& stream) {
            ownedInput.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
            parse(ownedInput.data(), ownedInput.size(), false);
        }

        void parse(const char* buffer, std::size_t size, bool copy = true) {
            while (size > 0 && buffer[size - 1] == '\0') {
                --size;
            }
            if (copy) {
                ownedInput.assign(buffer, size);
                buffer = ownedInput.data();
            }
            try {
                document = YAML::Load(std::string{buffer, size});
            } catch (const YAML::Exception& error) {
                result = sa::error(sa::ErrorCode::ParseError, error.what());
                return;
            }
            if (!document || !document.IsDefined()) {
                result = sa::error(sa::ErrorCode::ParseError, "YAML document has no root node");
                return;
            }
            result = sa::success();
        }

        std::string ownedInput;
        YAML::Node document;
        sa::Result<void> result;
    };

    template <typename BufferT, typename T>
    static auto write(OutputState<BufferT>& state, const T& value) -> sa::Result<void> {
        state.document = YAML::Node{};
        state.writer.reset(&state.document);
        auto result   = parserWrite<yamlcpp::Writer>(state.writer, value, parsing::Parent<yamlcpp::Writer>::Root{});
        state.hasRoot = static_cast<bool>(result) && static_cast<bool>(state.writer.result());
        state.flushed = false;
        if (!state.writer.result()) {
            return state.writer.result();
        }
        return result;
    }

    template <typename BufferT>
    static auto finish(OutputState<BufferT>& state, sa::Result<void> result) -> sa::Result<void> {
        if (!result) {
            return result;
        }
        if (!state.writer.result()) {
            return state.writer.result();
        }
        if (!state.hasRoot) {
            return result;
        }
        if (!state.flushed) {
            YAML::Emitter emitter;
            emitter << state.document;
            if (!emitter.good()) {
                return sa::error(sa::ErrorCode::Unknown, emitter.GetLastError());
            }
            detail::appendYaml(state.buffer, std::string_view{emitter.c_str(), emitter.size()});
            state.flushed = true;
        }
        return result;
    }

    template <typename BufferT>
    static auto outputReady(const OutputState<BufferT>& state, const sa::Result<void>& result) noexcept -> bool {
        return state.hasRoot && static_cast<bool>(result) && static_cast<bool>(state.writer.result());
    }

    template <typename SourceT>
    static auto inputResult(const InputState<SourceT>& state) -> sa::Result<void> {
        return state.result;
    }

    template <typename SourceT, typename T>
    static auto read(InputState<SourceT>& state, T& value) -> sa::Result<void> {
        return parserRead<yamlcpp::Reader>(state.document, value);
    }
};

template <typename BufferT = YamlCppBackend::DefaultOutputBuffer>
class YamlCppOutputSerializer : public detail::OutputSerializerAdapter<YamlCppBackend, BufferT> {
public:
    using Base = detail::OutputSerializerAdapter<YamlCppBackend, BufferT>;
    using Base::Base;
};

template <typename BufferT>
YamlCppOutputSerializer(BufferT&) -> YamlCppOutputSerializer<BufferT>;

template <typename SourceT = YamlCppBackend::DefaultInputSource>
class YamlCppInputSerializer : public detail::InputSerializerAdapter<YamlCppBackend, SourceT> {
public:
    using Base = detail::InputSerializerAdapter<YamlCppBackend, SourceT>;
    using Base::Base;
};

YamlCppInputSerializer(const char*, std::size_t) -> YamlCppInputSerializer<>;
YamlCppInputSerializer(std::istream&) -> YamlCppInputSerializer<>;

struct YamlCppSerializer {
    using OutputSerializer = YamlCppOutputSerializer<>;
    using InputSerializer  = YamlCppInputSerializer<>;
    using Reader           = yamlcpp::Reader;
    using Writer           = yamlcpp::Writer;
};
#endif

#if defined(NEKO_PROTO_ENABLE_LIBFYAML)
using YamlSerializer = LibfyamlSerializer;
#elif defined(NEKO_PROTO_ENABLE_YAMLCPP)
using YamlSerializer = YamlCppSerializer;
#endif

} // namespace nekoproto

#else
#define NEKO_PROTO_NO_YAML_SERIALIZER
#endif
