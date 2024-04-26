#include <iostream>
#include <sstream>

#include "../rpc/cc_rpc_base.hpp"

CS_RPC_USE_NAMESPACE

std::string to_hex(const std::vector<char> &data) {
    std::stringstream ss;
    for (auto c : data) {
        ss << "\\" << std::hex << (uint32_t(c) & 0xFF);
    }
    return ss.str();
}

int main(int argc, char **argv) {
    CCMessageHeader header;
    header.mutable_protoType() = 234;
    header.mutable_factoryVersion() = 2134;
    header.mutable_length() = 31245;
    header.mutable_transType() = 23411;
    std::cout << to_hex(header.serialize()) << std::endl;
    std::cout << "Hello, world!" << std::endl;
    CCMessageHeader header2;
    header2.deserialize(header.serialize());
    std::cout << header2.protoType() << std::endl;
    std::cout << header2.factoryVersion() << std::endl;
    std::cout << header2.length() << std::endl;
    std::cout << header2.transType() << std::endl;
    // cc_rpc_base::rpc_server server;
    // server.start();

    return 0;
}