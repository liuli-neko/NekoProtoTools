/**
 * @file communication_base.cpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "nekoproto/communication/communication_base.hpp"

#include <ilias/task.hpp>
#include <system_error>

using namespace ilias;

namespace nekoproto {

template class NEKO_PROTO_API detail::ProtoBase<ProtocolTable, BinarySerializer>;
template class NEKO_PROTO_API detail::ProtoBase<RawDataMessage, BinarySerializer>;

auto ErrorCategory::instance() -> const ErrorCategory& {
    static ErrorCategory kInstance;
    return kInstance;
}

auto ErrorCategory::name() const noexcept -> const char* { return "NekoCommunicationError"; }

auto ErrorCategory::equivalent(int value, const std::error_condition& other) const noexcept -> bool {
    return other.category().name() == name() && value == other.value();
}

namespace detail {

auto ProtoClientBase::serializeMessageData(const IProto& message, bool runInThread, bool reserveHeader) const
    -> IoTask<std::vector<char>> {
    std::vector<char> data;
    if (reserveHeader) {
        data.resize(MessageHeader::size());
    }
    bool ok = false;
    if (runInThread) {
        ok = co_await blocking([&]() -> bool { return message.toData(data); });
    } else {
        ok = message.toData(data);
    }
    if (!ok) {
        data.clear();
    }
    co_return data;
}

auto ProtoClientBase::serializeHeader(const MessageHeader& header) const -> std::vector<char> {
    std::vector<char> headerData;
    BinarySerializer::OutputSerializer serializer(headerData);
    if (!serializer(makeTags<BinaryTag{.raw_fixed_data = true}>(header)) || !serializer.end()) {
        headerData.clear();
    }
    return headerData;
}

auto ProtoClientBase::serializeVersionPacket() const -> IoTask<std::vector<char>> {
    ProtocolTable protocolTable = {};
    if (mFactory != nullptr) {
        protocolTable.protocol_factory_version = mFactory->version();
        for (const auto& [name, type] : ProtoFactory::protoTypeMap()) {
            if (type > reserved_proto_type_size) {
                protocolTable.proto_table[type] = name;
            }
        }
    }

    IProto proto = protocolTable.makeProto();
    auto dataRet = co_await serializeMessageData(proto, false, true);
    if (!dataRet) {
        co_return Err(dataRet.error());
    }
    auto data = std::move(dataRet.value());
    if (data.empty() || static_cast<int>(data.size()) == MessageHeader::size()) {
        co_return std::vector<char>{};
    }

    auto headerData = serializeHeader(MessageHeader(static_cast<uint32_t>(data.size() - MessageHeader::size()),
                                                    proto.type(), MessageType::VersionVerification));
    if (headerData.empty()) {
        co_return std::vector<char>{};
    }
    memcpy(data.data(), headerData.data(), headerData.size());
    co_return data;
}

auto ProtoClientBase::syncProtocolTable(std::span<std::byte> payload, const MessageHeader& header)
    -> IoTask<void> {
    IProto proto{new ProtocolTable::ProtoType{}};
    if (header.data != proto.type()) {
        co_return Err(Error(ErrorCode::InvalidProtoType));
    }
    if (!proto.fromData(reinterpret_cast<char*>(payload.data()), payload.size())) {
        co_return Err(Error(ErrorCode::InvalidProtoData));
    }

    auto* protoTable = proto.cast<ProtocolTable>();
    if (mFactory != nullptr && protoTable->protocol_factory_version != mFactory->version()) {
        NEKO_LOG_WARN("Communication", "ProtoFactory version mismatch: {} != {}", protoTable->protocol_factory_version,
                      mFactory->version());
    }
    mProtocolTable = *protoTable;
    NEKO_LOG_INFO("Communication", "sync message proto table, version: {}. size: {}",
                  mProtocolTable.protocol_factory_version, mProtocolTable.proto_table.size());
    for (const auto& [type, name] : mProtocolTable.proto_table) {
        NEKO_LOG_INFO("Communication", "Proto table: {} -> {}", name, type);
    }
    co_return {};
}

auto ProtoClientBase::finishMessage(IProto message, const MessageHeader& header,
                                           std::vector<std::byte>&& payload, StreamFlag flag) -> IoTask<IProto> {
    if (message == nullptr) {
        if (static_cast<int>(flag & StreamFlag::RecvUnknownTypeData) != 0) {
            auto rawData  = mFactory->create(RawDataMessage::specifyType());
            auto* proto   = rawData.cast<RawDataMessage>();
            proto->type   = header.data;
            proto->length = header.length;
            proto->name   = mProtocolTable.proto_table[header.data];
            proto->data   = std::move(payload);
            co_return rawData;
        }
        co_return Err(Error(ErrorCode::UnrecognizedMessage));
    }

    const auto parsePayload = [&]() -> bool {
        return message.fromData(reinterpret_cast<char*>(payload.data()), payload.size());
    };
    if (static_cast<int>(flag & StreamFlag::SerializerInThread) != 0) {
        auto ret = co_await blocking(parsePayload);
        if (!ret) {
            co_return Err(Error(ErrorCode::InvalidProtoData));
        }
    } else if (!parsePayload()) {
        co_return Err(Error(ErrorCode::InvalidProtoData));
    }
    co_return std::move(message);
}

auto ProtoClientBase::createProto(const uint32_t type) const -> IProto {
    if (mFactory == nullptr) {
        return {};
    }
    if (type <= reserved_proto_type_size || mProtocolTable.proto_table.empty()) {
        return mFactory->create(type);
    }
    auto it = mProtocolTable.proto_table.find(type);
    if (it != mProtocolTable.proto_table.end()) {
        return mFactory->create(it->second.c_str());
    }
    return {};
}


} // namespace detail

} // namespace nekoproto
