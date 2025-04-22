#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <gtest/gtest.h>
#include <iostream>

#include "nekoproto/global/log.hpp"
namespace cereal {
//! Saving for std::map<std::string, std::string>
template <class Archive, class C, class A>
inline void save(Archive& ar, std::map<C, A> const& map) {
    for (const auto& i : map)
        ar(cereal::make_nvp(i.first, i.second));
}

//! Loading for std::map<std::string, std::string>
template <class Archive, class C, class A>
inline void load(Archive& ar, std::map<C, A>& map) {
    map.clear();

    auto hint = map.begin();
    while (true) {
        const auto namePtr = ar.getNodeName();

        if (!namePtr) break;

        C key = namePtr;
        A value;
        ar(value);
        hint = map.emplace_hint(hint, std::move(key), std::move(value));
    }
}
} // namespace cereal

struct CTestStruct1 {
    std::map<std::string, int> f0  = {};
    std::string f1                 = {};
    int f2                         = {};
    double f3                      = {};
    std::map<std::string, int> f4  = {};
    int f5                         = {};
    double f6                      = {};
    std::map<std::string, int> f7  = {};
    double f8                      = {};
    std::string f9                 = {};
    int f10                        = {};
    int f11                        = {};
    std::vector<int> f12           = {};
    std::map<std::string, int> f13 = {};
    double f14                     = {};
    std::map<std::string, int> f15 = {};
    int f16                        = {};
    bool f17                       = {};
    bool f18                       = {};
    double f19                     = {};
    std::string f20                = {};
    int f21                        = {};
    int f22                        = {};
    std::string f23                = {};
    bool f24                       = {};
    std::map<std::string, int> f25 = {};
    std::vector<int> f26           = {};
    int f27                        = {};
    std::string f28                = {};
    bool f29                       = {};
    std::string f30                = {};
    double f31                     = {};
    std::map<std::string, int> f32 = {};
    std::vector<int> f33           = {};
    std::string f34                = {};
    std::string f35                = {};
    double f36                     = {};
    double f37                     = {};
    double f38                     = {};
    std::string f39                = {};
    std::string f40                = {};
    std::string f41                = {};
    std::map<std::string, int> f42 = {};
    double f43                     = {};
    int f44                        = {};
    std::map<std::string, int> f45 = {};
    double f46                     = {};
    bool f47                       = {};
    double f48                     = {};
    std::vector<int> f49           = {};
    int f50                        = {};
    double f51                     = {};
    std::map<std::string, int> f52 = {};
    std::string f53                = {};
    bool f54                       = {};
    std::map<std::string, int> f55 = {};
    bool f56                       = {};
    bool f57                       = {};
    int f58                        = {};
    int f59                        = {};
    std::vector<int> f60           = {};
    bool f61                       = {};
    double f62                     = {};
    std::vector<int> f63           = {};
    std::string f64                = {};
    std::vector<int> f65           = {};
    std::map<std::string, int> f66 = {};
    std::vector<int> f67           = {};
    int f68                        = {};
    double f69                     = {};
    std::map<std::string, int> f70 = {};
    bool f71                       = {};
    int f72                        = {};
    int f73                        = {};
    bool f74                       = {};
    double f75                     = {};
    std::string f76                = {};
    std::string f77                = {};

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(f0), CEREAL_NVP(f1), CEREAL_NVP(f2), CEREAL_NVP(f3), CEREAL_NVP(f4), CEREAL_NVP(f5),
           CEREAL_NVP(f6), CEREAL_NVP(f7), CEREAL_NVP(f8), CEREAL_NVP(f9), CEREAL_NVP(f10), CEREAL_NVP(f11),
           CEREAL_NVP(f12), CEREAL_NVP(f13), CEREAL_NVP(f14), CEREAL_NVP(f15), CEREAL_NVP(f16), CEREAL_NVP(f17),
           CEREAL_NVP(f18), CEREAL_NVP(f19), CEREAL_NVP(f20), CEREAL_NVP(f21), CEREAL_NVP(f22), CEREAL_NVP(f23),
           CEREAL_NVP(f24), CEREAL_NVP(f25), CEREAL_NVP(f26), CEREAL_NVP(f27), CEREAL_NVP(f28), CEREAL_NVP(f29),
           CEREAL_NVP(f30), CEREAL_NVP(f31), CEREAL_NVP(f32), CEREAL_NVP(f33), CEREAL_NVP(f34), CEREAL_NVP(f35),
           CEREAL_NVP(f36), CEREAL_NVP(f37), CEREAL_NVP(f38), CEREAL_NVP(f39), CEREAL_NVP(f40), CEREAL_NVP(f41),
           CEREAL_NVP(f42), CEREAL_NVP(f43), CEREAL_NVP(f44), CEREAL_NVP(f45), CEREAL_NVP(f46), CEREAL_NVP(f47),
           CEREAL_NVP(f48), CEREAL_NVP(f49), CEREAL_NVP(f50), CEREAL_NVP(f51), CEREAL_NVP(f52), CEREAL_NVP(f53),
           CEREAL_NVP(f54), CEREAL_NVP(f55), CEREAL_NVP(f56), CEREAL_NVP(f57), CEREAL_NVP(f58), CEREAL_NVP(f59),
           CEREAL_NVP(f60), CEREAL_NVP(f61), CEREAL_NVP(f62), CEREAL_NVP(f63), CEREAL_NVP(f64), CEREAL_NVP(f65),
           CEREAL_NVP(f66), CEREAL_NVP(f67), CEREAL_NVP(f68), CEREAL_NVP(f69), CEREAL_NVP(f70), CEREAL_NVP(f71),
           CEREAL_NVP(f72), CEREAL_NVP(f73), CEREAL_NVP(f74), CEREAL_NVP(f75), CEREAL_NVP(f76), CEREAL_NVP(f77));
    }
};

struct CTestStruct2 {
    double f0                      = {};
    std::vector<int> f1            = {};
    CTestStruct1 f2                = {};
    double f3                      = {};
    bool f4                        = {};
    int f5                         = {};
    double f6                      = {};
    std::vector<int> f7            = {};
    bool f8                        = {};
    std::string f9                 = {};
    CTestStruct1 f10               = {};
    CTestStruct1 f11               = {};
    bool f12                       = {};
    double f13                     = {};
    std::map<std::string, int> f14 = {};
    bool f15                       = {};
    bool f16                       = {};
    bool f17                       = {};
    CTestStruct1 f18               = {};
    std::string f19                = {};
    bool f20                       = {};
    CTestStruct1 f21               = {};
    std::map<std::string, int> f22 = {};
    bool f23                       = {};
    double f24                     = {};
    bool f25                       = {};
    int f26                        = {};
    int f27                        = {};
    std::map<std::string, int> f28 = {};
    int f29                        = {};
    std::map<std::string, int> f30 = {};
    std::map<std::string, int> f31 = {};
    std::map<std::string, int> f32 = {};
    int f33                        = {};
    CTestStruct1 f34               = {};
    std::vector<int> f35           = {};
    double f36                     = {};
    double f37                     = {};
    std::string f38                = {};

    template <typename Archive>
    void serialize(Archive& ar) {

        ar(CEREAL_NVP(f0), CEREAL_NVP(f1), CEREAL_NVP(f2), CEREAL_NVP(f3), CEREAL_NVP(f4), CEREAL_NVP(f5),
           CEREAL_NVP(f6), CEREAL_NVP(f7), CEREAL_NVP(f8), CEREAL_NVP(f9), CEREAL_NVP(f10), CEREAL_NVP(f11),
           CEREAL_NVP(f12), CEREAL_NVP(f13), CEREAL_NVP(f14), CEREAL_NVP(f15), CEREAL_NVP(f16), CEREAL_NVP(f17),
           CEREAL_NVP(f18), CEREAL_NVP(f19), CEREAL_NVP(f20), CEREAL_NVP(f21), CEREAL_NVP(f22), CEREAL_NVP(f23),
           CEREAL_NVP(f24), CEREAL_NVP(f25), CEREAL_NVP(f26), CEREAL_NVP(f27), CEREAL_NVP(f28), CEREAL_NVP(f29),
           CEREAL_NVP(f30), CEREAL_NVP(f31), CEREAL_NVP(f32), CEREAL_NVP(f33), CEREAL_NVP(f34), CEREAL_NVP(f35),
           CEREAL_NVP(f36), CEREAL_NVP(f37), CEREAL_NVP(f38));
    }
};

struct CTestStruct3 {
    bool f0                        = {};
    std::vector<int> f1            = {};
    int f2                         = {};
    CTestStruct2 f3                = {};
    CTestStruct2 f4                = {};
    CTestStruct1 f5                = {};
    std::map<std::string, int> f6  = {};
    std::map<std::string, int> f7  = {};
    int f8                         = {};
    std::map<std::string, int> f9  = {};
    double f10                     = {};
    std::map<std::string, int> f11 = {};
    std::vector<int> f12           = {};
    int f13                        = {};
    int f14                        = {};
    bool f15                       = {};
    std::map<std::string, int> f16 = {};
    std::map<std::string, int> f17 = {};
    bool f18                       = {};
    CTestStruct1 f19               = {};
    std::string f20                = {};
    CTestStruct1 f21               = {};

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(f0), CEREAL_NVP(f1), CEREAL_NVP(f2), CEREAL_NVP(f3), CEREAL_NVP(f4), CEREAL_NVP(f5),
           CEREAL_NVP(f6), CEREAL_NVP(f7), CEREAL_NVP(f8), CEREAL_NVP(f9), CEREAL_NVP(f10), CEREAL_NVP(f11),
           CEREAL_NVP(f12), CEREAL_NVP(f13), CEREAL_NVP(f14), CEREAL_NVP(f15), CEREAL_NVP(f16), CEREAL_NVP(f17),
           CEREAL_NVP(f18), CEREAL_NVP(f19), CEREAL_NVP(f20), CEREAL_NVP(f21));
    }
};

struct CTestStruct4 {
    std::vector<int> f0            = {};
    double f1                      = {};
    int f2                         = {};
    CTestStruct1 f3                = {};
    std::string f4                 = {};
    double f5                      = {};
    std::string f6                 = {};
    std::map<std::string, int> f7  = {};
    CTestStruct3 f8                = {};
    double f9                      = {};
    int f10                        = {};
    double f11                     = {};
    CTestStruct3 f12               = {};
    double f13                     = {};
    CTestStruct3 f14               = {};
    CTestStruct1 f15               = {};
    CTestStruct2 f16               = {};
    CTestStruct2 f17               = {};
    CTestStruct2 f18               = {};
    CTestStruct3 f19               = {};
    std::vector<int> f20           = {};
    std::map<std::string, int> f21 = {};
    bool f22                       = {};
    std::vector<int> f23           = {};
    double f24                     = {};
    double f25                     = {};
    double f26                     = {};
    std::string f27                = {};
    std::vector<int> f28           = {};
    std::string f29                = {};
    int f30                        = {};
    std::map<std::string, int> f31 = {};
    CTestStruct1 f32               = {};
    CTestStruct3 f33               = {};
    int f34                        = {};
    bool f35                       = {};
    std::vector<int> f36           = {};
    std::vector<int> f37           = {};
    double f38                     = {};
    bool f39                       = {};
    bool f40                       = {};
    CTestStruct3 f41               = {};
    std::vector<int> f42           = {};
    CTestStruct3 f43               = {};
    std::map<std::string, int> f44 = {};
    bool f45                       = {};
    CTestStruct3 f46               = {};
    std::map<std::string, int> f47 = {};
    std::map<std::string, int> f48 = {};
    std::vector<int> f49           = {};
    std::vector<int> f50           = {};
    std::map<std::string, int> f51 = {};
    std::string f52                = {};
    CTestStruct2 f53               = {};
    CTestStruct3 f54               = {};
    int f55                        = {};
    double f56                     = {};
    bool f57                       = {};
    CTestStruct3 f58               = {};
    double f59                     = {};
    std::map<std::string, int> f60 = {};
    CTestStruct3 f61               = {};
    std::vector<int> f62           = {};

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(f0), CEREAL_NVP(f1), CEREAL_NVP(f2), CEREAL_NVP(f3), CEREAL_NVP(f4), CEREAL_NVP(f5),
           CEREAL_NVP(f6), CEREAL_NVP(f7), CEREAL_NVP(f8), CEREAL_NVP(f9), CEREAL_NVP(f10), CEREAL_NVP(f11),
           CEREAL_NVP(f12), CEREAL_NVP(f13), CEREAL_NVP(f14), CEREAL_NVP(f15), CEREAL_NVP(f16), CEREAL_NVP(f17),
           CEREAL_NVP(f18), CEREAL_NVP(f19), CEREAL_NVP(f20), CEREAL_NVP(f21), CEREAL_NVP(f22), CEREAL_NVP(f23),
           CEREAL_NVP(f24), CEREAL_NVP(f25), CEREAL_NVP(f26), CEREAL_NVP(f27), CEREAL_NVP(f28), CEREAL_NVP(f29),
           CEREAL_NVP(f30), CEREAL_NVP(f31), CEREAL_NVP(f32), CEREAL_NVP(f33), CEREAL_NVP(f34), CEREAL_NVP(f35),
           CEREAL_NVP(f36), CEREAL_NVP(f37), CEREAL_NVP(f38), CEREAL_NVP(f39), CEREAL_NVP(f40), CEREAL_NVP(f41),
           CEREAL_NVP(f42), CEREAL_NVP(f43), CEREAL_NVP(f44), CEREAL_NVP(f45), CEREAL_NVP(f46), CEREAL_NVP(f47),
           CEREAL_NVP(f48), CEREAL_NVP(f49), CEREAL_NVP(f50), CEREAL_NVP(f51), CEREAL_NVP(f52), CEREAL_NVP(f53),
           CEREAL_NVP(f54), CEREAL_NVP(f55), CEREAL_NVP(f56), CEREAL_NVP(f57), CEREAL_NVP(f58), CEREAL_NVP(f59),
           CEREAL_NVP(f60), CEREAL_NVP(f61), CEREAL_NVP(f62));
    }
};

#include "big_data_test_data_1.cpp"
#include "big_data_test_data_2.cpp"

std::string makeData(const char* data, std::size_t size) { return std::string(data, size); }

TEST(BigProtoTest, Serializer) {
    NEKO_LOG_INFO("unit test", "Proto1 size {}", sizeof(CTestStruct1));

    NEKO_LOG_INFO("unit test", "Proto2 size {}", sizeof(CTestStruct2));

    NEKO_LOG_INFO("unit test", "Proto3 size {}", sizeof(CTestStruct3));

    NEKO_LOG_INFO("unit test", "Proto4 size {}", sizeof(CTestStruct4));

    // 统计解析时长
    auto data  = makeData(data_1, sizeof(data_1));
    auto start = std::chrono::high_resolution_clock::now();
    auto end   = start;
    CTestStruct4 proto1;
    {
        std::istringstream is(data);
        cereal::JSONInputArchive ia(is);
        proto1.serialize(ia);
    }
    NEKO_LOG_INFO(
        "unit test", "Serializer time: {}s",
        std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - end)
            .count());
    NEKO_LOG_INFO("unit test", "Serializer data: {}", proto1.f0.size());
    data = makeData(data_2, sizeof(data_2));
    end  = std::chrono::high_resolution_clock::now();
    CTestStruct4 proto2;
    {
        std::istringstream is(data);
        cereal::JSONInputArchive ia(is);
        proto2.serialize(ia);
    }
    NEKO_LOG_INFO(
        "unit test", "Serializer time: {}s",
        std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - end)
            .count());
    NEKO_LOG_INFO("unit test", "Serializer data: {}", proto2.f0.size());
    end = std::chrono::high_resolution_clock::now();

    NEKO_LOG_INFO("unit test", "total time: {}s",
                  std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    return RUN_ALL_TESTS();
}
