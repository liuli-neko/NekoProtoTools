#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/rpc/private/backend_base.hpp"

NEKO_BEGIN_NAMESPACE
namespace rpc {

static constexpr auto neko_rpc_compression_algorithm_name(NekoRpcCompressionAlgorithm algorithm) noexcept
    -> std::string_view {
    switch (algorithm) {
    case NekoRpcCompressionAlgorithm::None:
        return "none";
    case NekoRpcCompressionAlgorithm::RunLength:
        return "run_length";
    }
    return "unknown";
}

template <typename Backend, typename Counter>
auto neko_rpc_add_compression_stat(const typename Backend::Options& options, Counter counter,
                                   std::uint64_t value = 1) -> void {
    if (options.compression_stats) {
        ((*options.compression_stats).*counter).fetch_add(value, std::memory_order_relaxed);
    }
}

template <typename Backend>
auto neko_rpc_encode_outgoing_frame(const typename Backend::Options& options,
                                    typename Backend::PeerSession& session,
                                    typename Backend::Codec::FrameParts frame)
    -> ilias::Result<typename Backend::Message, std::error_code> {
    using Codec            = typename Backend::Codec;
    using CompressionCodec = typename Backend::CompressionCodec;
    using CompressionStats = typename Backend::CompressionStats;
    using Kind             = typename Backend::Kind;
    using Flag             = typename Backend::Flag;

    const auto encode_checked = [&options](typename Backend::Codec::FrameParts value)
        -> ilias::Result<typename Backend::Message, std::error_code> {
        auto encoded = Codec::encodeFrame(value);
        const auto header_size = Codec::headerSize();
        if (encoded.size() < header_size) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        ILIAS_TRY(auto body_size,
                  Codec::headerBodySize(std::span<const std::byte>(encoded).first(header_size),
                                        value.header.codec, options.frame_limits));
        if (body_size != encoded.size() - header_size) {
            return ilias::Err(RpcError::InvalidRequest);
        }
        return encoded;
    };

    if (!session.compression_enabled || session.compression_algorithm == Backend::CompressionAlgorithm::None) {
        return encode_checked(frame);
    }

    if (frame.header.kind == Kind::Hello || (frame.header.flags & Flag::Compressed) != 0U) {
        return encode_checked(frame);
    }

    if (frame.payload.empty() || frame.payload.size() < session.compression_min_payload_size) {
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::skipped_small_payloads);
        return encode_checked(frame);
    }

    // Compression is a backend-owned extension transform. The endpoint only sees
    // the final frame bytes and never needs to know whether payload bytes changed.
    NEKO_LOG_TRACE("rpc", "rpc backend compression begin: algorithm={} payload={}",
                   neko_rpc_compression_algorithm_name(session.compression_algorithm), frame.payload.size());
    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::compression_attempts);
    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::compression_input_bytes, frame.payload.size());

    auto compressed = CompressionCodec::compress(frame.payload, session.compression_algorithm);
    if (!compressed) {
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::compression_errors);
        NEKO_LOG_WARN("rpc", "rpc backend compression failed: algorithm={} payload={}",
                      neko_rpc_compression_algorithm_name(session.compression_algorithm), frame.payload.size());
        return ilias::Err(compressed.error());
    }

    if (compressed.value().size() >= frame.payload.size()) {
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::skipped_ineffective_frames);
        NEKO_LOG_TRACE("rpc", "rpc backend compression skipped: reason=ineffective input={} output={}",
                       frame.payload.size(), compressed.value().size());
        return encode_checked(frame);
    }

    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::compressed_frames);
    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::compression_output_bytes,
                                       compressed.value().size());

    const auto original_size = frame.payload.size();
    frame.header.flags = static_cast<std::uint8_t>(frame.header.flags | Flag::Compressed);
    frame.payload      = NekoRpcExtensionCodec::asBytes(compressed.value());
    NEKO_LOG_TRACE("rpc", "rpc backend compression end: input={} output={} algorithm={}", original_size,
                   compressed.value().size(), neko_rpc_compression_algorithm_name(session.compression_algorithm));
    return encode_checked(frame);
}

template <typename Backend>
auto neko_rpc_decompress_incoming_payload(const typename Backend::Options& options,
                                          typename Backend::PeerSession& session,
                                          const typename Backend::Codec::FrameParts& parts)
    -> ilias::Result<std::optional<typename Backend::Message>, std::error_code> {
    using CompressionCodec = typename Backend::CompressionCodec;
    using CompressionStats = typename Backend::CompressionStats;
    using Kind             = typename Backend::Kind;
    using Flag             = typename Backend::Flag;

    if ((parts.header.flags & Flag::Compressed) == 0U) {
        return std::nullopt;
    }

    // Keep decompression result as payload only, so large frames are not rebuilt
    // just to remove the compressed flag.
    if (!session.compression_enabled || session.compression_algorithm == Backend::CompressionAlgorithm::None ||
        parts.header.kind == Kind::Hello) {
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompression_input_bytes,
                                               parts.payload.size());
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompression_errors);
        NEKO_LOG_WARN("rpc", "rpc backend decompression rejected: kind={} payload={}",
                      static_cast<unsigned>(parts.header.kind), parts.payload.size());
        return ilias::Err(RpcError::InvalidRequest);
    }

    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompression_input_bytes,
                                           parts.payload.size());
    NEKO_LOG_TRACE("rpc", "rpc backend decompression begin: algorithm={} payload={}",
                   neko_rpc_compression_algorithm_name(session.compression_algorithm), parts.payload.size());
    const auto ratio = std::max<std::size_t>(options.max_compression_ratio, 1U);
    const auto ratio_budget = parts.payload.size() > std::numeric_limits<std::size_t>::max() / ratio
                                  ? std::numeric_limits<std::size_t>::max()
                                  : parts.payload.size() * ratio;
    const auto output_budget = std::min(options.max_decompressed_payload_bytes, ratio_budget);
    auto payload = CompressionCodec::decompress(parts.payload, session.compression_algorithm, output_budget);
    if (!payload) {
        neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompression_errors);
        NEKO_LOG_WARN("rpc", "rpc backend decompression failed: algorithm={} payload={}",
                      neko_rpc_compression_algorithm_name(session.compression_algorithm), parts.payload.size());
        return ilias::Err(payload.error());
    }

    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompressed_frames);
    neko_rpc_add_compression_stat<Backend>(options, &CompressionStats::decompression_output_bytes,
                                           payload.value().size());
    NEKO_LOG_TRACE("rpc", "rpc backend decompression end: input={} output={} algorithm={}", parts.payload.size(),
                   payload.value().size(), neko_rpc_compression_algorithm_name(session.compression_algorithm));

    return std::move(payload.value());
}

} // namespace rpc
NEKO_END_NAMESPACE
