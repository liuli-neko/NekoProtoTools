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

namespace nekoproto {
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
    [[nodiscard]] auto name() const noexcept -> const char* override { return "nekoproto.argparser.config_io"; }

    [[nodiscard]] auto message(int condition) const -> std::string override {
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

inline auto configIoErrorCategory() -> const std::error_category& {
    static ConfigIoErrorCategory category;
    return category;
}

inline auto makeErrorCode(ConfigIoErrorCode error) noexcept -> std::error_code {
    return {static_cast<int>(error), configIoErrorCategory()};
}

// Required by std::error_code's ADL customization protocol.
inline auto make_error_code(ConfigIoErrorCode error) noexcept -> std::error_code { // NOLINT(readability-identifier-naming)
    return makeErrorCode(error);
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

inline auto makeError(ConfigIoErrorCode code, std::string message, std::string_view format = {},
                                std::filesystem::path path = {}, std::error_code cause = {}) -> ConfigIoError {
    return {.code    = makeErrorCode(code),
            .cause   = cause,
            .message = std::move(message),
            .format  = std::string(format),
            .path    = std::move(path)};
}

inline auto unexpected(ConfigIoError error) -> expected::unexpected<ConfigIoError> {
    return expected::unexpected<ConfigIoError>(std::move(error));
}

inline auto quoteText(std::string_view value) -> std::string { return "'" + std::string(value) + "'"; }

inline auto pathText(const std::filesystem::path& path) -> std::string { return quoteText(path.string()); }

inline void addPathContext(ConfigIoError& error, const std::filesystem::path& path) {
    error.path = path;
    error.message.append(" in config file ");
    error.message.append(pathText(path));
}

inline auto normalizedExtension(const std::filesystem::path& path) -> std::string {
    auto extension = path.extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    for (auto& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return extension;
}

inline auto readStream(std::istream& input, std::string_view format, const std::filesystem::path& path = {}) -> Result<Bytes> {
    Bytes bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (!input.eof() && input.fail()) {
        auto message = std::string("failed to read ") + std::string(format) + " config data";
        if (!path.empty()) {
            message.append(" from ");
            message.append(pathText(path));
        }
        return unexpected(makeError(ConfigIoErrorCode::ReadFailed, std::move(message), format, path));
    }
    return bytes;
}

inline auto writeStream(std::ostream& output, std::span<const char> bytes, std::string_view format,
                                 const std::filesystem::path& path = {}) -> Result<void> {
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        auto message = std::string("failed to write ") + std::string(format) + " config data";
        if (!path.empty()) {
            message.append(" to ");
            message.append(pathText(path));
        }
        return unexpected(makeError(ConfigIoErrorCode::WriteFailed, std::move(message), format, path));
    }
    return {};
}

template <typename Backend>
auto unavailableError() -> ConfigIoError {
    auto message = std::string(Backend::label) + " config backend is not available";
    return makeError(ConfigIoErrorCode::BackendUnavailable, std::move(message), Backend::format);
}

inline auto unknownFormatError(std::string_view format, const std::filesystem::path& path = {}) -> ConfigIoError {
    auto message = std::string("unknown config format ") + quoteText(format);
    if (!path.empty()) {
        message.append(" for ");
        message.append(pathText(path));
    }
    return makeError(ConfigIoErrorCode::UnknownFormat, std::move(message), format, path);
}

} // namespace detail

template <typename T, typename Backend>
auto decode(std::span<const char> data, Format<Backend> /*format*/) -> Result<T> {
    static_assert(std::is_default_constructible_v<T>, "config_io::decode requires a default constructible type");
    if constexpr (!Backend::available) {
        return detail::unexpected(detail::unavailableError<Backend>());
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
                detail::makeError(ConfigIoErrorCode::DecodeFailed, std::move(message), Backend::format, {}, cause));
        }
        return value;
    }
}

template <typename T, typename Backend, typename... SerializerArgs>
auto encode(const T& value, Format<Backend> /*format*/, SerializerArgs&&... serializer_args) -> Result<Bytes> {
    if constexpr (!Backend::available) {
        return detail::unexpected(detail::unavailableError<Backend>());
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
                detail::makeError(ConfigIoErrorCode::EncodeFailed, std::move(message), Backend::format, {}, cause));
        }
        return bytes;
    }
}

template <typename T>
auto decode(std::span<const char> data, std::string_view format) -> Result<T> {
    std::optional<Result<T>> result;
    argparser::detail::forEachConfigIoBackend([&]<typename Backend>(std::type_identity<Backend>) {
        if (result.has_value() || !argparser::detail::configIoFormatMatches<Backend>(format)) {
            return;
        }
        result.emplace(decode<T>(data, Format<Backend>{}));
    });
    if (!result.has_value()) {
        return detail::unexpected(detail::unknownFormatError(format));
    }
    return std::move(*result);
}

template <typename T>
auto decode(std::string_view data, std::string_view format) -> Result<T> {
    return decode<T>(std::span<const char>{data.data(), data.size()}, format);
}

template <typename T>
auto encode(const T& value, std::string_view format) -> Result<Bytes> {
    std::optional<Result<Bytes>> result;
    argparser::detail::forEachConfigIoBackend([&]<typename Backend>(std::type_identity<Backend>) {
        if (result.has_value() || !argparser::detail::configIoFormatMatches<Backend>(format)) {
            return;
        }
        result.emplace(encode(value, Format<Backend>{}));
    });
    if (!result.has_value()) {
        return detail::unexpected(detail::unknownFormatError(format));
    }
    return std::move(*result);
}

template <typename T, typename Backend>
auto load(std::istream& input, Format<Backend> format) -> Result<T> {
    auto bytes = detail::readStream(input, Backend::format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return decode<T>(std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T>
auto load(std::istream& input, std::string_view format) -> Result<T> {
    auto bytes = detail::readStream(input, format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return decode<T>(std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T, typename Backend>
auto load(const std::filesystem::path& path, Format<Backend> format) -> Result<T> {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        auto message = std::string("could not open config file ") + detail::pathText(path);
        return detail::unexpected(detail::makeError(ConfigIoErrorCode::OpenFailed, std::move(message), Backend::format,
                                                     path, {errno, std::generic_category()}));
    }
    auto result = load<T>(input, format);
    if (!result) {
        detail::addPathContext(result.error(), path);
    }
    return result;
}

template <typename T>
auto load(const std::filesystem::path& path, std::string_view format) -> Result<T> {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        auto message = std::string("could not open config file ") + detail::pathText(path);
        return detail::unexpected(detail::makeError(ConfigIoErrorCode::OpenFailed, std::move(message), format, path,
                                                     {errno, std::generic_category()}));
    }
    auto result = load<T>(input, format);
    if (!result) {
        detail::addPathContext(result.error(), path);
    }
    return result;
}

template <typename T>
auto load(const std::filesystem::path& path) -> Result<T> {
    const auto format = detail::normalizedExtension(path);
    if (format.empty()) {
        return detail::unexpected(detail::unknownFormatError(format, path));
    }
    return load<T>(path, format);
}

template <typename T, typename Backend, typename... SerializerArgs>
auto save(const T& value, std::ostream& output, Format<Backend> format, SerializerArgs&&... serializer_args) -> Result<void> {
    auto bytes = encode(value, format, std::forward<SerializerArgs>(serializer_args)...);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return detail::writeStream(output, std::span<const char>{bytes->data(), bytes->size()}, Backend::format);
}

template <typename T>
auto save(const T& value, std::ostream& output, std::string_view format) -> Result<void> {
    auto bytes = encode(value, format);
    if (!bytes) {
        return detail::unexpected(std::move(bytes.error()));
    }
    return detail::writeStream(output, std::span<const char>{bytes->data(), bytes->size()}, format);
}

template <typename T, typename Backend, typename... SerializerArgs>
auto save(const T& value, const std::filesystem::path& path, Format<Backend> format,
                  SerializerArgs&&... serializer_args) -> Result<void> {
    auto bytes = encode(value, format, std::forward<SerializerArgs>(serializer_args)...);
    if (!bytes) {
        detail::addPathContext(bytes.error(), path);
        return detail::unexpected(std::move(bytes.error()));
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        auto message = std::string("could not open config file ") + detail::pathText(path);
        return detail::unexpected(detail::makeError(ConfigIoErrorCode::OpenFailed, std::move(message), Backend::format,
                                                     path, {errno, std::generic_category()}));
    }
    return detail::writeStream(output, std::span<const char>{bytes->data(), bytes->size()}, Backend::format, path);
}

template <typename T>
auto save(const T& value, const std::filesystem::path& path, std::string_view format) -> Result<void> {
    auto bytes = encode(value, format);
    if (!bytes) {
        detail::addPathContext(bytes.error(), path);
        return detail::unexpected(std::move(bytes.error()));
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        auto message = std::string("could not open config file ") + detail::pathText(path);
        return detail::unexpected(detail::makeError(ConfigIoErrorCode::OpenFailed, std::move(message), format, path,
                                                     {errno, std::generic_category()}));
    }
    return detail::writeStream(output, std::span<const char>{bytes->data(), bytes->size()}, format, path);
}

template <typename T>
auto save(const T& value, const std::filesystem::path& path) -> Result<void> {
    const auto format = detail::normalizedExtension(path);
    if (format.empty()) {
        return detail::unexpected(detail::unknownFormatError(format, path));
    }
    return save(value, path, format);
}

} // namespace argparser::config_io
} // namespace nekoproto

namespace std {
template <>
struct is_error_code_enum<nekoproto::argparser::config_io::ConfigIoErrorCode> : true_type {};
} // namespace std
