#include "../rpc/cc_rpc_base.hpp"

#include <regex>

#include "ilias_async.hpp"

using namespace ILIAS_NAMESPACE;

CS_RPC_BEGIN_NAMESPACE

std::tuple<std::string, std::string, uint16_t> parseUrl(const std::string &url) {
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

    std::transform(protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    return std::make_tuple(protocol, ip, port);
}

ChannelFactory::ChannelFactory(IoContext &ioContext, std::shared_ptr<CS_PROTO_NAMESPACE::ProtoFactory> factory) : 
    mIoContext(ioContext), 
    mFactory(factory) {}

void ChannelFactory::listen(std::string_view hostname) {

    auto [protocol, ip, port] = parseUrl(std::string(hostname));

    if (protocol == "tcp") {
        mListener = TcpListener(mIoContext, AF_INET);
        auto ret = mListener.bind(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            CS_LOG_ERROR("tcp listener Failed. can't bind {}:{}", ip, port);
        }
    }
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::connect(std::string_view hostname,const uint16_t channelId) {
    auto [protocol, ip, port] = parseUrl(std::string(hostname));

    if (protocol == "tcp") {
        IStreamClient client = TcpClient(mIoContext, AF_INET);
        auto ret = co_await client.connect(IPEndpoint(IPAddress4::fromString(ip.c_str()), port));
        if (!ret) {
            co_return ret.error();
        }
        auto ret1 = co_await makeChannel(std::move(client), channelId);
        if (!ret1) {
            co_return ret.error();
        }
        co_return ret1.value();
    }
}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::accept() {
    auto ret = co_await mListener.accept();
    if (!ret) {
        co_return ret.error();
    }

    IStreamClient client = std::move(ret.value().first);
    auto ret1 = co_await makeChannel(std::move(client), 0);
    if (!ret1) {
        co_return ret.error();
    }
    co_return ret1.value();
}

void ChannelFactory::close() {

}

Task<std::weak_ptr<ChannelBase>> ChannelFactory::makeChannel(IStreamClient &&client,const uint16_t channelId) {
    ChannelHeader cmsg;

    cmsg.set_messageType(static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage));
    cmsg.set_channelId(channelId);
    auto buf = cmsg.serialize();

    CCMessageHeader hmsg(buf.size(), cmsg.type(), mFactory->version(), static_cast<uint16_t>(TransType::Channel));

    auto msgbuf = hmsg.serialize();
    msgbuf.insert(msgbuf.end(), buf.begin(), buf.end());

    ByteStream<IStreamClient, char> client1(std::move(client));
    auto ret = co_await client1.sendAll(msgbuf.data(), msgbuf.size());
    buf.clear();
    msgbuf.clear();

    if (!ret) {
        co_return ret.error();
    }

    msgbuf.resize(CCMessageHeader::size());
    ret = co_await client1.recvAll(msgbuf.data(), msgbuf.size());
    if (!ret) {
        co_return ret.error();
    }

    hmsg.deserialize(std::move(msgbuf));
    msgbuf.clear();
    buf.resize(hmsg.length());

    ret = co_await client1.recvAll(buf.data(), buf.size());
    if (!ret) {
        co_return ret.error();
    }

    cmsg.deserialize(std::move(buf));
    if (hmsg.type() != cmsg.type()) {
        co_return Error(Error::Unknown);
    }

    if (hmsg.factoryVersion() != mFactory->version()) {
        co_return Error(Error::Unknown);
    }

    if (hmsg.transType() != static_cast<uint16_t>(TransType::Channel)) {
        co_return Error(Error::Unknown);
    }

    if (cmsg.messageType() != static_cast<uint8_t>(ChannelHeader::MessageType::ConnectMessage)) {
        co_return Error(Error::Unknown);
    }

    if (cmsg.channelId() != channelId) {
        co_return Error(Error::Unknown);
    }

    std::shared_ptr<ChannelBase>channel(new Channel<ByteStream<IStreamClient, char>, ByteStream<IStreamClient, char>>(this, std::move(client1), channelId));
    mChannels.insert(std::make_pair(channelId, channel));
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
    for (const auto &[key, _] : mChannels) {
        ret.push_back(key);
    }
    return ret;
}

const CS_PROTO_NAMESPACE::ProtoFactory &ChannelFactory::getProtoFactory() {
    return *mFactory;
}

CS_RPC_END_NAMESPACE