#include "../rpc/cc_rpc_base.hpp"

#include <regex>

#include "ilias_async.hpp"

using namespace ILIAS_NAMESPACE;

CS_RPC_BEGIN_NAMESPACE

std::tuple<std::string, std::string, uint16_t> parseUrl(const std::string& url) {
    std::string protocol;
    std::string ip = "";
    uint16_t port = 0;

    // define the regular expression pattern
    std::regex regex("(\\w+)://([\\d.]+):(\\d+)");
    std::smatch match;

    // search for the pattern in the URL
    if (std::regex_search(url, match, regex)) {
        protocol = match[1];
        ip = match[2];
        port = std::stoi(match[3]);
    }

    std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) { return std::tolower(c); });

    return std::make_tuple(protocol, ip, port);
}

ChannelFactory::ChannelFactory(IoContext& ioContext, std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory)
    : mIoContext(ioContext), mFactory(factory) {}

ChannelFactory::~ChannelFactory() { close(); }

void ChannelFactory::listen(std::string_view hostname) {
    auto [protocol, ip, port] = parseUrl(std::string(hostname));

    if (protocol == "tcp") {
        mListener = TcpListener(mIoContext, AF_INET);
        auto ret = mListener.bind(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            CS_LOG_ERROR("tcp listener Failed. can't bind {}:{}", ip, port);
        }
        return;
    }
    CS_LOG_ERROR("tcp listener Failed. can't bind {}:{}", ip, port);
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::connect(std::string_view hostname, const uint16_t channelId) {
    auto [protocol, ip, port] = parseUrl(std::string(hostname));

    if (protocol == "tcp") {
        IStreamClient client = TcpClient(mIoContext, AF_INET);
        auto ret = co_await client.connect(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        auto ret1 = co_await makeChannel(std::move(client), channelId);
        if (!ret1) {
            co_return Unexpected(ret.error());
        }
        co_return ret1.value();
    }
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::accept() {
    auto ret = co_await mListener.accept();
    if (!ret) {
        CS_LOG_ERROR("tcp listener Failed. can't accept");
        co_return Unexpected(ret.error());
    }

    ByteStream<IStreamClient, char> client1(std::move(ret.value().first));
    std::vector<char> buf(CCMessageHeader::size(), 0);
    auto ret1 = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret1) {
        CS_LOG_ERROR("recv msg header error");
        co_return Unexpected(ret.error());
    }
    CCMessageHeader hmsg;
    hmsg.deserialize(buf);
    buf.clear();
    ChannelHeader cmsg;
    if (hmsg.transType() != static_cast<uint16_t>(TransType::Channel)) {
        CS_LOG_ERROR("trans type {} is not channel", hmsg.transType());
        co_return Unexpected(Error(Error::Unknown));
    }
    if (hmsg.protoType() != cmsg.type()) {
        CS_LOG_ERROR("type {} is not channel header", hmsg.type());
        co_return Unexpected(Error(Error::Unknown));
    }
    buf.resize(hmsg.length());
    ret1 = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret1) {
        CS_LOG_ERROR("recv data error");
        co_return Unexpected(ret.error());
    }
    cmsg.deserialize(buf);
    if (cmsg.messageType() != static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage)) {
        CS_LOG_ERROR("message type {} is not connect message", cmsg.messageType());
        co_return Unexpected(Error(Error::Unknown));
    }
    if (cmsg.factoryVersion() != getProtoFactory().version()) {
        CS_LOG_ERROR("factory version {} is not {}", cmsg.factoryVersion(), getProtoFactory().version());
        co_return Unexpected(Error(Error::Unknown));
    }
    uint32_t channelId = 0;
    if (cmsg.channelId() == 0) {
        channelId = mChannels.size() + 1;
        while (mChannels.find(channelId) != mChannels.end()) {
            channelId++;
        }
        cmsg.set_channelId(channelId);
    } else {
        channelId = cmsg.channelId();
    }

    buf = hmsg.serialize();
    auto msgBuf = cmsg.serialize();
    buf.insert(buf.end(), msgBuf.begin(), msgBuf.end());
    msgBuf.clear();
    ret1 = co_await client1.sendAll(buf.data(), buf.size());
    if (!ret1) {
        CS_LOG_ERROR("send data error");
        co_return Unexpected(ret.error());
    }

    std::shared_ptr<ChannelBase> channel(new ByteStreamChannel(this, std::move(client1), cmsg.channelId()));
    mChannels.insert(std::make_pair(cmsg.channelId(), channel));
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
    cmsg.set_messageType(static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage));
    cmsg.set_channelId(channelId);
    cmsg.set_factoryVersion(mFactory->version());
    auto buf = cmsg.serialize();
    CCMessageHeader hmsg(buf.size(), cmsg.type(), static_cast<uint16_t>(TransType::Channel), 0);
    auto msgbuf = hmsg.serialize();
    msgbuf.resize(msgbuf.size() + buf.size());
    std::copy(buf.begin(), buf.end(), msgbuf.begin() + msgbuf.size() - buf.size());
    ByteStream<IStreamClient, char> client1(std::move(client));
    auto ret = co_await client1.sendAll(msgbuf.data(), msgbuf.size());
    buf.clear();
    msgbuf.clear();

    if (!ret) {
        CS_LOG_ERROR("send data error");
        co_return Unexpected(ret.error());
    }

    msgbuf.resize(CCMessageHeader::size());
    ret = co_await client1.recvAll(msgbuf.data(), msgbuf.size());
    if (!ret) {
        CS_LOG_ERROR("recv msg header error");
        co_return Unexpected(ret.error());
    }
    hmsg.deserialize(std::move(msgbuf));
    msgbuf.clear();
    buf.resize(hmsg.length());

    ret = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret) {
        CS_LOG_ERROR("recv data error");
        co_return Unexpected(ret.error());
    }

    cmsg.deserialize(buf);
    if (hmsg.protoType() != cmsg.type()) {
        CS_LOG_ERROR("type {} is not channel header", hmsg.type());
        co_return Unexpected(Error(Error::Unknown));
    }

    if (hmsg.transType() != static_cast<uint16_t>(TransType::Channel)) {
        CS_LOG_ERROR("trans type {} is not channel", hmsg.transType());
        co_return Unexpected(Error(Error::Unknown));
    }

    if (cmsg.messageType() != static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage)) {
        CS_LOG_ERROR("message type {} is not connect message", cmsg.messageType());
        co_return Unexpected(Error(Error::Unknown));
    }

    if (cmsg.factoryVersion() != mFactory->version()) {
        CS_LOG_ERROR("factory version {} is not {}", cmsg.factoryVersion(), mFactory->version());
        co_return Unexpected(Error(Error::Unknown));
    }

    if (channelId != 0 && cmsg.channelId() != channelId) {
        CS_LOG_ERROR("channel id {} is not {}", cmsg.channelId(), channelId);
        co_return Unexpected(Error(Error::Unknown));
    }

    std::shared_ptr<ChannelBase> channel(new ByteStreamChannel(this, std::move(client1), cmsg.channelId()));
    mChannels.insert(std::make_pair(cmsg.channelId(), channel));
    co_return std::weak_ptr<ChannelBase>(channel);
}

std::weak_ptr<ChannelBase> ChannelFactory::getChannel(uint16_t channelId) {
    auto item = mChannels.find(channelId);
    if (item != mChannels.end()) {
        return item->second;
    }
    return std::weak_ptr<ChannelBase>();
}

std::vector<uint16_t> ChannelFactory::getChannels() {
    std::vector<uint16_t> ret;
    for (const auto& [key, _] : mChannels) {
        ret.push_back(key);
    }
    return ret;
}

const CS_PROTO_NAMESPACE::ProtoFactory& ChannelFactory::getProtoFactory() { return *(mFactory.get()); }

ByteStreamChannel::ByteStreamChannel(ChannelFactory* ctxt, ILIAS_NAMESPACE::ByteStream<>&& client, uint16_t channelId)
    : ChannelBase(ctxt, channelId), mClient(std::move(client)) {
    mState = ChannelBase::ChannelState::Connected;
}

ILIAS_NAMESPACE::Task<void> ByteStreamChannel::send(std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message) {
    if (mState != ChannelBase::ChannelState::Connected) {
        co_return ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::Unknown);
    }
    auto msg = message->serialize();
    CCMessageHeader msgHeader(msg.size(), message->type(), mChannelFactory->getProtoFactory().version(),
                              static_cast<uint16_t>(ChannelFactory::TransType::Channel));
    std::vector<char> sendMsg = msgHeader.serialize();
    sendMsg.resize(sendMsg.size() + msg.size());
    memcpy(sendMsg.data() + CCMessageHeader::size(), msg.data(), msg.size());
    msg.clear();
    auto ret = co_await mClient.send(sendMsg.data(), sendMsg.size());
    sendMsg.clear();
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    co_return ILIAS_NAMESPACE::Result<void>();
}

ILIAS_NAMESPACE::Task<std::unique_ptr<CS_PROTO_NAMESPACE::IProto>> ByteStreamChannel::recv() {
    if (mState != ChannelBase::ChannelState::Connected) {
        co_return ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::Unknown);
    }
    std::vector<char> headerData;
    headerData.resize(CCMessageHeader::size(), 0);
    auto ret = co_await mClient.recvAll(headerData.data(), headerData.size());
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    CCMessageHeader msgHeader;
    msgHeader.deserialize(std::move(headerData));
    if (msgHeader.length() == 0 && msgHeader.protoType() == 0 && msgHeader.transType() == 0) {
        close();
        co_return ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::ChannelBroken);
    }
    std::vector<char> data(msgHeader.length(), 0);
    ret = co_await mClient.recvAll(data.data(), data.size());
    if (!ret) {
        close();
        co_return Unexpected(ret.error());
    }
    std::unique_ptr<CS_PROTO_NAMESPACE::IProto> message(
        mChannelFactory->getProtoFactory().create(msgHeader.protoType()));
    if (message == nullptr) {
        CS_LOG_ERROR("unknown proto type: {}", msgHeader.protoType());
        co_return ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::Unknown);
    }
    auto desRet = message->deserialize(std::move(data));
    if (!desRet) {
        CS_LOG_ERROR("deserialize message deserialize failed.");
    }
    if (message->type() == CS_PROTO_NAMESPACE::ProtoFactory::proto_type<ChannelHeader>()) {
        std::unique_ptr<ChannelHeader> cmsg(static_cast<ChannelHeader*>(message.release()));
        if (cmsg->messageType() == ChannelHeader::MessageType::CloseMessage) {
            close();
            co_return ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code::ChannelBroken);
        }
    }
    co_return std::move(message);
}

void ByteStreamChannel::close() {
    if (mState == ChannelBase::ChannelState::Closed) {
        return;
    }
    ChannelHeader cmsg;
    cmsg.set_channelId(mChannelId);
    cmsg.set_factoryVersion(mChannelFactory->getProtoFactory().version());
    cmsg.set_messageType(ChannelHeader::MessageType::CloseMessage);
    std::vector<char> msg = cmsg.serialize();
    CCMessageHeader msgHeader(msg.size(), cmsg.type(), static_cast<uint16_t>(ChannelFactory::TransType::Channel), 0);
    auto buf = msgHeader.serialize();
    buf.resize(buf.size() + msg.size());
    memcpy(buf.data() + CCMessageHeader::size(), msg.data(), msg.size());
    ilias_wait mClient.sendAll(buf.data(), buf.size());
    mState = ChannelBase::ChannelState::Closed;
}

void ByteStreamChannel::destroy() { mChannelFactory->destroyChannel(mChannelId); }

CS_RPC_END_NAMESPACE

CS_DECLARE_PROTO(CS_RPC_NAMESPACE::CCMessageHeader, RPCHeader);
CS_DECLARE_PROTO(CS_RPC_NAMESPACE::ChannelHeader, ChannelHeader);
