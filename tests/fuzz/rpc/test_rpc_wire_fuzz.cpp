#include "nekoproto/rpc/rpc.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

NEKO_USE_NAMESPACE

namespace {

struct FuzzBinaryValue {
    std::map<std::string, std::vector<std::int64_t>> values;
    std::string label;

    NEKO_SERIALIZER(values, label)
};

constexpr std::size_t MaxFuzzInputBytes = 1U * 1024U * 1024U;
constexpr std::size_t MaxFuzzOutputBytes = 2U * 1024U * 1024U;

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size > MaxFuzzInputBytes) {
        return 0;
    }
    const auto bytes = std::span<const std::byte>{reinterpret_cast<const std::byte*>(data), size};

    binary::ParseLimits limits;
    limits.max_input_bytes = MaxFuzzInputBytes;
    limits.max_string_bytes = 256U * 1024U;
    limits.max_container_elements = 64U * 1024U;
    limits.max_object_fields = 4096U;
    limits.max_depth = 32U;
    limits.max_total_allocated_bytes = MaxFuzzOutputBytes;
    FuzzBinaryValue decoded;
    BinarySerializer::InputSerializer input(reinterpret_cast<const char*>(data), size, limits);
    if (input(decoded)) {
        std::vector<char> canonical;
        BinarySerializer::OutputSerializer output(canonical);
        if (output(decoded) && canonical.size() <= MaxFuzzOutputBytes) {
            FuzzBinaryValue reparsed;
            BinarySerializer::InputSerializer canonicalInput(canonical.data(), canonical.size(), limits);
            (void)canonicalInput(reparsed);
        }
    }

    rpc::NekoRpcFrameCodec::FrameParts frame;
    (void)rpc::NekoRpcFrameCodec::parseFrame(bytes, 0U, frame);

    rpc::NekoRpcExtensionCodec::ExtensionMapType extensions;
    if (rpc::NekoRpcExtensionCodec::loadTlvs(bytes, extensions)) {
        std::vector<rpc::NekoRpcMethodEntry> entries;
        if (rpc::NekoRpcMethodIdExtension::parseMethodTable(extensions, entries)) {
            rpc::NekoRpcMethodIdTable table(64U * 1024U);
            (void)table.applyRemoteTable(std::move(entries), rpc::NekoRpcMethodIdExtension::InitialTableVersion);
        }
    }

    (void)rpc::NekoRpcCompressionCodec::decompress(
        bytes, rpc::NekoRpcCompressionAlgorithm::RunLength, MaxFuzzOutputBytes);
    return 0;
}
