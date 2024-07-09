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
#include "../communication/communication_base.hpp"

#include <regex>

#include "ilias_async.hpp"

using namespace ILIAS_NAMESPACE;

NEKO_BEGIN_NAMESPACE

Expected<std::tuple<std::string, std::string, uint16_t>, Error> parseUrl(const std::string& url) {
    std::string protocol;
    std::string ip = "";
    uint16_t port  = 0;

    // define the regular expression pattern
    std::regex regex("(\\w+)://([\\d.]+):(\\d+)");
    std::smatch match;

    // search for the pattern in the URL
    if (std::regex_search(url, match, regex)) {
        protocol = match[1];
        ip       = match[2];
        port     = std::stoi(match[3]);
    } else {
        NEKO_LOG_ERROR("Invalid url {}", url);
        return Unexpected<Error>(ErrorCode::InvalidUrl);
    }

    std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) { return std::tolower(c); });

    return std::make_tuple(protocol, ip, port);
}

ChannelFactory::ChannelFactory(IoContext& ioContext, std::shared_ptr<NEKO_NAMESPACE::ProtoFactory> factory)
    : mIoContext(ioContext), mFactory(factory) {}

ChannelFactory::~ChannelFactory() { close(); }

Expected<void, Error> ChannelFactory::listen(std::string_view hostname) {
    auto ret = parseUrl(std::string(hostname));
    if (!ret) {
        return Unexpected(ret.error());
    }
    auto [protocol, ip, port] = ret.value();

    if (protocol == "tcp") {
        mListener = TcpListener(mIoContext, AF_INET);
        auto ret  = mListener.bind(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            NEKO_LOG_ERROR("tcp listener Failed. can't bind {}:{}, {}", ip, port, ret.error().message());
            return Unexpected<Error>(ret.error());
        }
        return Expected<void, Error>();
    }

    NEKO_LOG_ERROR("unsupported protocol {}", protocol);
    return Unexpected<Error>(Error(ErrorCode::InvalidUrl));
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::connect(std::string_view hostname, const uint16_t channelId) {
    auto ret = parseUrl(std::string(hostname));
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto [protocol, ip, port] = ret.value();

    if (protocol == "tcp") {
        IStreamClient client = TcpClient(mIoContext, AF_INET);
        auto ret             = co_await client.connect(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        auto ret1 = co_await makeChannel(std::move(client), channelId);
        if (!ret1) {
            co_return Unexpected(ret.error());
        }
        co_return ret1.value();
    }

    co_return Unexpected<Error>(ErrorCode::InvalidUrl);
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::accept() {
    auto ret = co_await mListener.accept();
    if (!ret) {
        NEKO_LOG_ERROR("tcp accept Failed. {}", ret.error().message());
        co_return Unexpected(ret.error());
    }

    ByteStream<IStreamClient, char> client1(std::move(ret.value().first));
    std::vector<char> buf(MessageHeader::size(), 0);
    auto ret1 = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret1) {
        NEKO_LOG_ERROR("recv msg header error");
        co_return Unexpected<Error>(ret.error());
    }
    // NEKO_LOG_INFO("recv: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    MessageHeader hmsg;
    {
        BinarySerializer::InputSerializer serializer(buf);
        serializer(hmsg);
    }
    buf.clear();
    ChannelHeader cmsg;
    if (hmsg.transType != static_cast<uint16_t>(TransType::Channel) || hmsg.protoType != 0) {
        NEKO_LOG_ERROR("trans type {} or protoType {} is not channel", hmsg.transType, hmsg.protoType);
        co_return Unexpected<Error>(Error(ErrorCode::ConnectionMessageTypeError));
    }
    buf.resize(hmsg.length);
    ret1 = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret1) {
        NEKO_LOG_ERROR("recv data error");
        co_return Unexpected(ret.error());
    }
    // NEKO_LOG_INFO("recv: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    {
        BinarySerializer::InputSerializer serializer(buf);
        serializer(cmsg);
    }
    buf.clear();
    if (cmsg.messageType != static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage)) {
        NEKO_LOG_ERROR("message type {} is not connect message", cmsg.messageType);
        co_return Unexpected(Error(ErrorCode::ConnectionMessageTypeError));
    }
    if (cmsg.factoryVersion != getProtoFactory().version()) {
        NEKO_LOG_ERROR("factory version {} is not {}", cmsg.factoryVersion, getProtoFactory().version());
        co_return Unexpected(Error(ErrorCode::ProtoVersionUnsupported));
    }
    uint32_t channelId = 0;
    if (cmsg.channelId == 0) {
        channelId = mChannels.size() + 1;
        while (mChannels.find(channelId) != mChannels.end()) {
            channelId++;
        }
        cmsg.channelId = channelId;
    } else {
        channelId = cmsg.channelId;
    }
    {
        BinarySerializer::OutputSerializer serializer(buf);
        serializer(hmsg);
        serializer(cmsg);
    }
    NEKO_ASSERT(buf.size() == ChannelHeader::size() + MessageHeader::size(), "buf size error");
    // NEKO_LOG_INFO("send: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    ret1 = co_await client1.sendAll(buf.data(), buf.size());
    if (!ret1) {
        NEKO_LOG_ERROR("send data error");
        co_return Unexpected(ret.error());
    }

    std::shared_ptr<ChannelBase> channel(new ByteStreamChannel(this, std::move(client1), cmsg.channelId));
    mChannels.insert(std::make_pair(cmsg.channelId, channel));
    co_return std::weak_ptr<ChannelBase>(channel);
}

void ChannelFactory::destroyChannel(uint16_t channelId) {
    auto item = mChannels.find(channelId);
    if (item == mChannels.end()) {
        return;
    }
    item->second->close();
    mChannels.erase(channelId);
}

void ChannelFactory::close() {
    for (const auto& [id, channel] : mChannels) {
        channel->close();
    }
    mChannels.clear();
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::makeChannel(IStreamClient&& client, const uint16_t channelId) {
    ChannelHeader cmsg;
    cmsg.messageType    = static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage);
    cmsg.channelId      = channelId;
    cmsg.factoryVersion = mFactory->version();
    std::vector<char> buf;
    MessageHeader hmsg(cmsg.size(), 0, static_cast<uint16_t>(TransType::Channel), 0);
    {
        BinarySerializer::OutputSerializer serializer(buf);
        serializer(hmsg);
        serializer(cmsg);
    }
    ByteStream<IStreamClient, char> client1(std::move(client));
    auto ret = co_await client1.sendAll(buf.data(), buf.size());
    buf.clear();

    if (!ret) {
        NEKO_LOG_ERROR("send data error");
        co_return Unexpected(ret.error());
    }

    buf.resize(MessageHeader::size());
    ret = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret) {
        NEKO_LOG_ERROR("recv msg header error");
        co_return Unexpected(ret.error());
    }
    // NEKO_LOG_INFO("recv: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    {
        BinarySerializer::InputSerializer serializer(buf);
        serializer(hmsg);
    }
    buf.clear();
    buf.resize(hmsg.length);
    ret = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret) {
        NEKO_LOG_ERROR("recv data error");
        co_return Unexpected(ret.error());
    }
    // NEKO_LOG_INFO("recv: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    {
        BinarySerializer::InputSerializer serializer(buf);
        serializer(cmsg);
    }
    if (hmsg.transType != static_cast<uint16_t>(TransType::Channel) || hmsg.protoType != 0) {
        NEKO_LOG_ERROR("transType {} or type {} is not channel header", hmsg.transType, hmsg.protoType);
        co_return Unexpected(Error(ErrorCode::ConnectionMessageTypeError));
    }

    if (cmsg.messageType != static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage)) {
        NEKO_LOG_ERROR("message type {} is not connect message", cmsg.messageType);
        co_return Unexpected(Error(ErrorCode::ConnectionMessageTypeError));
    }

    if (cmsg.factoryVersion != mFactory->version()) {
        NEKO_LOG_ERROR("factory version {} is not {}", cmsg.factoryVersion, mFactory->version());
        co_return Unexpected(Error(ErrorCode::ProtoVersionUnsupported));
    }

    if (channelId != 0 && cmsg.channelId != channelId) {
        NEKO_LOG_ERROR("channel id {} is not {}", cmsg.channelId, channelId);
        co_return Unexpected(Error(ErrorCode::ChannelIdInconsistent));
    }

    std::shared_ptr<ChannelBase> channel(new ByteStreamChannel(this, std::move(client1), cmsg.channelId));
    mChannels.insert(std::make_pair(cmsg.channelId, channel));
    co_return std::weak_ptr<ChannelBase>(channel);
}

std::weak_ptr<ChannelBase> ChannelFactory::getChannel(uint16_t channelId) {
    auto item = mChannels.find(channelId);
    if (item != mChannels.end()) {
        return item->second;
    }
    return std::weak_ptr<ChannelBase>();
}

std::vector<uint16_t> ChannelFactory::getChannelIds() {
    std::vector<uint16_t> ret;
    for (const auto& [key, _] : mChannels) {
        ret.push_back(key);
    }
    return ret;
}

const NEKO_NAMESPACE::ProtoFactory& ChannelFactory::getProtoFactory() { return *(mFactory.get()); }

ByteStreamChannel::ByteStreamChannel(ChannelFactory* ctxt, ILIAS_NAMESPACE::ByteStream<>&& client, uint16_t channelId)
    : ChannelBase(ctxt, channelId), mClient(std::move(client)) {
    mState = ChannelBase::ChannelState::Connected;
}

ILIAS_NAMESPACE::Task<void> ByteStreamChannel::send(std::unique_ptr<NEKO_NAMESPACE::IProto> message) {
    if (mState != ChannelBase::ChannelState::Connected) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::Unknown));
    }
    auto msg = message->toData();
    MessageHeader msgHeader(msg.size(), message->type(), mChannelFactory->getProtoFactory().version(),
                            static_cast<uint16_t>(ChannelFactory::TransType::Channel));
    BinarySerializer serializer;
    std::vector<char> sendMsg;
    {
        BinarySerializer::OutputSerializer serializer(sendMsg);
        serializer(msgHeader);
    }
    sendMsg.resize(sendMsg.size() + msg.size());
    memcpy(sendMsg.data() + MessageHeader::size(), msg.data(), msg.size());
    msg.clear();
    auto ret = co_await mClient.send(sendMsg.data(), sendMsg.size());
    sendMsg.clear();
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    co_return ILIAS_NAMESPACE::Result<void>();
}

ILIAS_NAMESPACE::Task<std::unique_ptr<NEKO_NAMESPACE::IProto>> ByteStreamChannel::recv() {
    if (mState != ChannelBase::ChannelState::Connected) {
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::ChannelClosed));
    }
    std::vector<char> headerData;
    headerData.resize(MessageHeader::size(), 0);
    auto ret = co_await mClient.recvAll(headerData.data(), headerData.size());
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    MessageHeader msgHeader;
    BinarySerializer serializer;
    {
        BinarySerializer::InputSerializer serializer(headerData);
        serializer(msgHeader);
    }
    if (msgHeader.length == 0 && msgHeader.protoType == 0 && msgHeader.transType == 0) {
        close();
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::ChannelBroken));
    }
    std::vector<char> data(msgHeader.length, 0);
    ret = co_await mClient.recvAll(data.data(), data.size());
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    if (msgHeader.protoType == 0) {
        ChannelHeader cmsg;
        {
            BinarySerializer::InputSerializer serializer(data);
            serializer(cmsg);
        }
        if (cmsg.messageType == ChannelHeader::MessageType::CloseMessage) {
            close();
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::ChannelClosedByPeer));
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidChannelHeader));
        }
    }

    std::unique_ptr<NEKO_NAMESPACE::IProto> message(mChannelFactory->getProtoFactory().create(msgHeader.protoType));
    if (message == nullptr) {
        NEKO_LOG_ERROR("unknown proto type: {}", msgHeader.protoType);
        co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error(ErrorCode::InvalidProtoType));
    }
    auto desRet = message->formData(std::move(data));
    if (!desRet) {
        NEKO_LOG_ERROR("deserialize message deserialize failed.");
    }
    co_return std::move(message);
}

Task<void> _closeLater(ILIAS_NAMESPACE::ByteStream<> client, std::vector<char> buf, uint16_t id,
                       NEKO_NAMESPACE::ChannelFactory* const mf) {
    // NEKO_LOG_INFO("send: {}", spdlog::to_hex(buf.data(), buf.data() + buf.size()));
    mf->destroyChannel(id);
    auto ret = co_await client.send(buf.data(), buf.size());
    co_return !ret ? Unexpected(ret.error()) : ILIAS_NAMESPACE::Result<>();
}

void ByteStreamChannel::close() {
    if (mState == ChannelBase::ChannelState::Closed) {
        return;
    }
    mState = ChannelBase::ChannelState::Closed;
    ChannelHeader cmsg;
    cmsg.channelId      = mChannelId;
    cmsg.factoryVersion = mChannelFactory->getProtoFactory().version();
    cmsg.messageType    = ChannelHeader::MessageType::CloseMessage;
    MessageHeader msgHeader(ChannelHeader::size(), 0, static_cast<uint16_t>(ChannelFactory::TransType::Channel), 0);
    std::vector<char> buf;
    BinarySerializer serializer;
    {
        BinarySerializer::InputSerializer serializer(buf);
        serializer(msgHeader);
        serializer(cmsg);
    }
    ilias_go _closeLater(std::move(mClient), std::move(buf), mChannelId, mChannelFactory);
}

void ByteStreamChannel::destroy() { mChannelFactory->destroyChannel(mChannelId); }

auto ErrorCategory::instance() -> const ErrorCategory& {
    static ErrorCategory instance;
    return instance;
}

auto ErrorCategory::message(uint32_t value) const -> std::string {
    switch (value) {
#define NEKO_CHANNEL_ERROR(name, code, message, _)                                                                     \
    case code:                                                                                                         \
        return message;
        NEKO_CHANNEL_ERROR_CODE_TABLE
#undef NEKO_CHANNEL_ERROR
    default:
        return "unknown error";
    }
}

auto ErrorCategory::name() const -> std::string_view { return "NekoCommunicationError"; }

auto ErrorCategory::equivalent(uint32_t self, const ILIAS_NAMESPACE::Error& other) const -> bool {
    if (self == static_cast<uint32_t>(ErrorCode::ChannelClosedByPeer) &&
        other == ILIAS_NAMESPACE::Error::ConnectionReset) {
        return true;
    }
    return other.category().name() == name() && self == static_cast<uint32_t>(other.value());
}

NEKO_END_NAMESPACE
