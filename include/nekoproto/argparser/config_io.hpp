#pragma once

#include "nekoproto/argparser/detail/config_io_registry.hpp"
#include "nekoproto/global/expected.hpp"
#include "nekoproto/global/global.hpp"

#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

NEKO_BEGIN_NAMESPACE
namespace argparser::config_io {

using Bytes = std::vector<char>;

enum class ConfigIoErrorCode {
    Success = 0,
    UnknownFormat,
    BackendUnavailable,
    OpenFailed,
    ReadFailed,
    DecodeFailed,
    EncodeFailed,
    WriteFailed,
};

class ConfigIoErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "nekoproto.argparser.config_io"; }

    [[nodiscard]] std::string message(int condition) const override {
        switch (static_cast<ConfigIoErrorCode>(condition)) {
        case ConfigIoErrorCode::Success:
            return "success";
        case ConfigIoErrorCode::UnknownFormat:
            return "unknown config format";
        case ConfigIoErrorCode::BackendUnavailable:
            return "config backend unavailable";
        case ConfigIoErrorCode::OpenFailed:
            return "could not open config file";
        case ConfigIoErrorCode::ReadFailed:
            return "could not read config data";
        case ConfigIoErrorCode::DecodeFailed:
            return "could not decode config data";
        case ConfigIoErrorCode::EncodeFailed:
            return "could not encode config data";
        case ConfigIoErrorCode::WriteFailed:
            return "could not write config data";
        }
        return "unknown config I/O error";
    }
};

inline const std::error_category& config_io_error_category() {
    static ConfigIoErrorCategory category;
    return category;
}

inline std::error_code make_error_code(ConfigIoErrorCode error) noexcept {
    return {static_cast<int>(error), config_io_error_category()};
}

struct ConfigIoError {
    std::error_code code;
    std::error_code cause;
    std::string message;
    std::string format;
    std::filesystem::path path;
};

template <typename T>
using Result = expected::expected<T, ConfigIoError>;

template <typename Backend>
struct Format {
    using backend                          = Backend;
    static constexpr std::string_view name = Backend::format;
};

namespace formats {
inline constexpr Format<argparser::detail::JsonConfigIoBackend> json;
inline constexpr Format<argparser::detail::YamlConfigIoBackend> yaml;
inline constexpr Format<argparser::detail::TomlConfigIoBackend> toml;
inline constexpr Format<argparser::detail::BinaryConfigIoBackend> binary;
} // namespace formats

namespace detail {

inline ConfigIoError make_error(ConfigIoErrorCode code, std::string message, std::string_view format = {},
                                std::filesystem::path path = {}, std::error_code cause = {}) {
    return {.code    = make_error_code(code),
            .cause   = cause,
            .message = std::move(message),
            .format  = std::string(format),
            .path    = std::move(path)};
}

inline expected::unexpected<ConfigIoError> unexpected(ConfigIoError error) {
    return expected::unexpected<ConfigIoError>(std::move(error));
}

inline std::string quote_text(std::string_view value) { return "'" + std::string(value) + "'"; }

inline std::string path_text(const std::filesystem::path& path) { return quote_text(path.string()); }

inline void add_path_context(ConfigIoError& error, const std::filesystem::path& path) {
    error.path = path;
    error.message.append(" in config file ");
    error.message.append(path_text(path));
}

inline std::string normalized_extension(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    for (auto& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return extension;
}

inline Result<Bytes> read_stream(std::istream& input, std::string_view format, const std::filesystem::path& path = {}) {
    Bytes bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (!input.eof() && input.fail()) {
        auto message = std::string("failed to read ") + std::string(format) + " config data";
        if (!path.empty()) {
            message.append(" from ");
            message.append(path_text(path));
        }
        return unexpected(make_error(ConfigIoErrorCode::ReadFailed, std::move(message), format, path));
    }
    return bytes;
}

inline Result<void> write_stream(std::ostream& output, std::span<const char> bytes, std::string_view format,
                                 const std::filesystem::path& path = {}) {
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        auto message = std::string("failed to write ") + std::string(format) + " config data";
        if (!path.empty()) {
            message.append(" to ");
            message.append(path_text(path));
        }
        return unexpected(make_error(ConfigIoErrorCode::WriteFailed, std::move(message), format, path));
    }
    return {};
}

template <typename Backend>
ConfigIoError unavailable_error() {
    auto message = std::string(Backend::label) + " config backend is not available";
    return make_error(ConfigIoErrorCode::BackendUnavailable, std::move(message), Backend::format);
}

inline ConfigIoError unknown_format_error(std::string_view format, const std::filesystem::path& path = {}) {
    auto message = std::string("unknown config format ") + quote_text(format);
    if (!path.empty()) {
        message.append(" for ");
        message.append(path_text(path));
    }
    return make_error(ConfigIoErrorCode::UnknownFormat, std::move(message), format, path);
}

} // namespace detail

template <typename T, typename Backend>
Result<T> decode(std::span<const char> data, Format<Backend> /*format*/) {
    static_assert(std::is_default_constructible_v<T>, "config_io::decode requires a default constructible type");
    if constexpr (!Backend::available) {
        return detail::unexpected(detail::unavailable_error<Backend>());
    } else {
        using Serializer = typename Backend::Serializer;
        T value{};
        typename Serializer::InputSerializer input(data.data(), data.size());
        if (!input(value)) {
            auto message = std::string("failed to decode ") + std::string(Backend::label) + " config";
            std::error_code cause;
            if (input.error() != nullptr) {
                cause = input.error()->ec;
                message.append(": ");
                message.append(input.error()->msg);
            }
            return detail::unexpected(
                detail::make_error(ConfigIoErrorCode::DecodeFailed, std::move(message), Backend::format, {}, cause));
        }
        return value;
    }
}

template <typename T, typename Backend, typename... SerializerArgs>
Result<Bytes> encode(const T& value, Format<Backend> /*format*/, SerializerArgs&&... serializer_args) {
    if constexpr (!Backend::available) {
        return detail::unexpected(detail::unavailable_error<Backend>());
    } else {
        using Serializer = typename Backend::Serializer;
        Bytes bytes;
        typename Serializer::OutputSerializer output(bytes, std::forward<SerializerArgs>(serializer_args)...);
        if (!output(value) || !output.end()) {
            auto message = std::string("failed to encode ") + std::string(Backend::label) + " config";
            std::error_code cause;
            if (output.error() != nullptr) {
                cause = output.error()->ec;
                message.append(": ");
                message.append(output.error()->msg);
            }
            return detail::unexpected(
                detail::make_error(ConfigIoErrorCode::EncodeFailed, std::move(message), Backend::format, {}, cause));
        }
        return bytes;
    }
}

template <typename T>
Result<T> decode(std::span<const char> data, std::string_view format) {
    std::optional<Result<T>> result;
    argparser::detail::for_each_config_io_backend([&]<typename Backend>(std::type_identity<Backend>) {
        if (result.has_value() || !argparser::detail::config_io_format_matches<Backend>(format)) {
            return;
        }
        result.emplace(decode<T>(data, Format<Backend>{}));
    });
    if (!result.has_value()) {
        return detail::unexpected(detail::unknown_format_error(format));
    }
    return std::move(*result);
}

template <typename T>
Result<T> decode(std::string_view data, std::string_view format) {
    return decode<T>(std::span<const char>{data.data(), data.size()}, format);
}

template <typename T>
Result<Bytes> encode(const T& value, std::string_view format) {
    std::optional<Result<Bytes>> result;
    argparser::detail::for_each_config_io_backend([&]<typename Backend>(std::type_identity<Backend>) {
        if (result.has_value() || !argparser::detail::config_io_format_matches<Backend>(format)) {
            return;
        }
        result.emplace(encode(value, Format<Backend>{}));
    });
    if (!result.has_value()) {
        return detail::unexpected(detail::unknown_format_error(format));
    }
    return std::move(*result);
}

template <typename T, typename Backend>
Result<T> load(std::istream& input, Format<Backend> format) {
    auto bytes = detail::read_stream(input, Backend::format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return decode<T>(std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T>
Result<T> load(std::istream& input, std::string_view format) {
    auto bytes = detail::read_stream(input, format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return decode<T>(std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T, typename Backend>
Result<T> load(const std::filesystem::path& path, Format<Backend> format) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        auto message = std::string("could not open config file ") + detail::path_text(path);
        return detail::unexpected(detail::make_error(ConfigIoErrorCode::OpenFailed, std::move(message), Backend::format,
                                                     path, {errno, std::generic_category()}));
    }
    auto result = load<T>(input, format);
    if (!result) {
        detail::add_path_context(result.error(), path);
    }
    return result;
}

template <typename T>
Result<T> load(const std::filesystem::path& path, std::string_view format) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        auto message = std::string("could not open config file ") + detail::path_text(path);
        return detail::unexpected(detail::make_error(ConfigIoErrorCode::OpenFailed, std::move(message), format, path,
                                                     {errno, std::generic_category()}));
    }
    auto result = load<T>(input, format);
    if (!result) {
        detail::add_path_context(result.error(), path);
    }
    return result;
}

template <typename T>
Result<T> load(const std::filesystem::path& path) {
    const auto format = detail::normalized_extension(path);
    if (format.empty()) {
        return detail::unexpected(detail::unknown_format_error(format, path));
    }
    return load<T>(path, format);
}

template <typename T, typename Backend, typename... SerializerArgs>
Result<void> save(const T& value, std::ostream& output, Format<Backend> format, SerializerArgs&&... serializer_args) {
    auto bytes = encode(value, format, std::forward<SerializerArgs>(serializer_args)...);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return detail::write_stream(output, std::span<const char>{bytes->data(), bytes->size()}, Backend::format);
}

template <typename T>
Result<void> save(const T& value, std::ostream& output, std::string_view format) {
    auto bytes = encode(value, format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return detail::write_stream(output, std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T, typename Backend, typename... SerializerArgs>
Result<void> save(const T& value, const std::filesystem::path& path, Format<Backend> format,
                  SerializerArgs&&... serializer_args) {
    auto bytes = encode(value, format, std::forward<SerializerArgs>(serializer_args)...);
    if (!bytes) {
        detail::add_path_context(bytes.error(), path);
        return detail::unexpected(std::move(bytes.error()));
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        auto message = std::string("could not open config file ") + detail::path_text(path);
        return detail::unexpected(detail::make_error(ConfigIoErrorCode::OpenFailed, std::move(message), Backend::format,
                                                     path, {errno, std::generic_category()}));
    }
    return detail::write_stream(output, std::span<const char>{bytes->data(), bytes->size()}, Backend::format, path);
}

template <typename T>
Result<void> save(const T& value, const std::filesystem::path& path, std::string_view format) {
    auto bytes = encode(value, format);
    if (!bytes) {
        detail::add_path_context(bytes.error(), path);
        return detail::unexpected(std::move(bytes.error()));
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        auto message = std::string("could not open config file ") + detail::path_text(path);
        return detail::unexpected(detail::make_error(ConfigIoErrorCode::OpenFailed, std::move(message), format, path,
                                                     {errno, std::generic_category()}));
    }
    return detail::write_stream(output, std::span<const char>{bytes->data(), bytes->size()}, format, path);
}

template <typename T>
Result<void> save(const T& value, const std::filesystem::path& path) {
    const auto format = detail::normalized_extension(path);
    if (format.empty()) {
        return detail::unexpected(detail::unknown_format_error(format, path));
    }
    return save(value, path, format);
}

} // namespace argparser::config_io
NEKO_END_NAMESPACE

namespace std {
template <>
struct is_error_code_enum<NEKO_NAMESPACE::argparser::config_io::ConfigIoErrorCode> : true_type {};
} // namespace std
