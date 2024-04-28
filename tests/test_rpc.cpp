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
    
    // cc_rpc_base::rpc_server server;
    // server.start();

    return 0;
}