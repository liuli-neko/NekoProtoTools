#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/types/array.hpp"
#include "nekoproto/proto/types/bitset.hpp"
#include "nekoproto/proto/types/deque.hpp"
#include "nekoproto/proto/types/list.hpp"
#include "nekoproto/proto/types/map.hpp"
#include "nekoproto/proto/types/multimap.hpp"
#include "nekoproto/proto/types/multiset.hpp"
#include "nekoproto/proto/types/pair.hpp"
#include "nekoproto/proto/types/set.hpp"
#include "nekoproto/proto/types/shared_ptr.hpp"
#include "nekoproto/proto/types/tuple.hpp"
#include "nekoproto/proto/types/vector.hpp"
#include <gtest/gtest.h>
#include <string>

NEKO_USE_NAMESPACE
struct ZTypeTest {
    int a                   = 1;
    std::string b           = "field set test";
    bool c                  = false;
    double d                = 3.141592654;
    std::vector<int> e      = {1, 2, 3};
    std::map<int, int> f    = {{1, 1}, {2, 2}};
    std::list<double> g     = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::set<std::string> h = {"a", "b", "c"};
    std::deque<float> i     = {1.1F, 2.2F, 3.3F, 4.4F, 5.5F, 6.6F, 7.7F};
    std::array<int, 7> j    = {1, 2, 3, 4, 5, 6, 7};
    std::tuple<int, std::string, bool, double, std::vector<int>, std::map<int, int>> k = {
        1, "hello", true, 3.141592654, {1, 2, 3}, {{1, 1}, {2, 2}}};
    std::shared_ptr<std::string> l        = std::make_shared<std::string>("hello shared ptr");
    std::multimap<int, std::string> p     = {{1, "hello"}, {2, "world"}, {1, "world"}};
    std::multiset<std::string> q          = {"a", "b", "c", "a", "b"};
    std::vector<std::vector<int>> t       = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    std::bitset<16> v                     = {0x3f3f3f};
    std::pair<std::string, std::string> w = {"hello", "world"};
    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, p, q, t, v, w)
    NEKO_DECLARE_PROTOCOL(ZTypeTest, JsonSerializer)
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    ZTypeTest proto;
    return static_cast<int>(proto.makeProto().fromData(reinterpret_cast<const char *>(data), size));
}