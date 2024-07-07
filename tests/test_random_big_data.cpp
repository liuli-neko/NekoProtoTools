#include <gtest/gtest.h>
#include <string>

#include "../core/json_serializer.hpp"
#include "../core/proto_base.hpp"
#include "../core/serializer_base.hpp"
#include "../core/types/map.hpp"
#include "../core/types/vector.hpp"

NEKO_USE_NAMESPACE
struct TestStruct1 {
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
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                    f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                    f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61,
                    f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77)
    NEKO_DECLARE_PROTOCOL(TestStruct1, JsonSerializer)
};

struct TestStruct2 {
    double f0                      = {};
    std::vector<int> f1            = {};
    TestStruct1 f2                 = {};
    double f3                      = {};
    bool f4                        = {};
    int f5                         = {};
    double f6                      = {};
    std::vector<int> f7            = {};
    bool f8                        = {};
    std::string f9                 = {};
    TestStruct1 f10                = {};
    TestStruct1 f11                = {};
    bool f12                       = {};
    double f13                     = {};
    std::map<std::string, int> f14 = {};
    bool f15                       = {};
    bool f16                       = {};
    bool f17                       = {};
    TestStruct1 f18                = {};
    std::string f19                = {};
    bool f20                       = {};
    TestStruct1 f21                = {};
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
    TestStruct1 f34                = {};
    std::vector<int> f35           = {};
    double f36                     = {};
    double f37                     = {};
    std::string f38                = {};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                    f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38)
    NEKO_DECLARE_PROTOCOL(TestStruct2, JsonSerializer)
};

struct TestStruct3 {
    bool f0                        = {};
    std::vector<int> f1            = {};
    int f2                         = {};
    TestStruct2 f3                 = {};
    TestStruct2 f4                 = {};
    TestStruct1 f5                 = {};
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
    TestStruct1 f19                = {};
    std::string f20                = {};
    TestStruct1 f21                = {};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21)
    NEKO_DECLARE_PROTOCOL(TestStruct3, JsonSerializer)
};

struct TestStruct4 {
    std::vector<int> f0            = {};
    double f1                      = {};
    int f2                         = {};
    TestStruct1 f3                 = {};
    std::string f4                 = {};
    double f5                      = {};
    std::string f6                 = {};
    std::map<std::string, int> f7  = {};
    TestStruct3 f8                 = {};
    double f9                      = {};
    int f10                        = {};
    double f11                     = {};
    TestStruct3 f12                = {};
    double f13                     = {};
    TestStruct3 f14                = {};
    TestStruct1 f15                = {};
    TestStruct2 f16                = {};
    TestStruct2 f17                = {};
    TestStruct2 f18                = {};
    TestStruct3 f19                = {};
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
    TestStruct1 f32                = {};
    TestStruct3 f33                = {};
    int f34                        = {};
    bool f35                       = {};
    std::vector<int> f36           = {};
    std::vector<int> f37           = {};
    double f38                     = {};
    bool f39                       = {};
    bool f40                       = {};
    TestStruct3 f41                = {};
    std::vector<int> f42           = {};
    TestStruct3 f43                = {};
    std::map<std::string, int> f44 = {};
    bool f45                       = {};
    TestStruct3 f46                = {};
    std::map<std::string, int> f47 = {};
    std::map<std::string, int> f48 = {};
    std::vector<int> f49           = {};
    std::vector<int> f50           = {};
    std::map<std::string, int> f51 = {};
    std::string f52                = {};
    TestStruct2 f53                = {};
    TestStruct3 f54                = {};
    int f55                        = {};
    double f56                     = {};
    bool f57                       = {};
    TestStruct3 f58                = {};
    double f59                     = {};
    std::map<std::string, int> f60 = {};
    TestStruct3 f61                = {};
    std::vector<int> f62           = {};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21,
                    f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
                    f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61,
                    f62)
    NEKO_DECLARE_PROTOCOL(TestStruct4, JsonSerializer)
};

#include "big_data_test_data_1.cpp"
#include "big_data_test_data_2.cpp"
#include "big_data_test_data_3.cpp"
#include "big_data_test_data_4.cpp"
#include "big_data_test_data_5.cpp"
#include "big_data_test_data_6.cpp"
#include "big_data_test_data_7.cpp"
#include "big_data_test_data_8.cpp"
#include "big_data_test_data_9.cpp"

TEST(BigProtoTest, Serializer) {
    NEKO_LOG_INFO("Proto1 size {}", sizeof(TestStruct1));
    NEKO_LOG_INFO("Proto1 type size {}", sizeof(TestStruct1::ProtoType));

    NEKO_LOG_INFO("Proto2 size {}", sizeof(TestStruct2));
    NEKO_LOG_INFO("Proto2 type size {}", sizeof(TestStruct2::ProtoType));

    NEKO_LOG_INFO("Proto3 size {}", sizeof(TestStruct3));
    NEKO_LOG_INFO("Proto3 type size {}", sizeof(TestStruct3::ProtoType));

    NEKO_LOG_INFO("Proto4 size {}", sizeof(TestStruct4));
    NEKO_LOG_INFO("Proto4 type size {}", sizeof(TestStruct4::ProtoType));

    // 统计解析时长
    auto start = std::chrono::high_resolution_clock::now();
    auto end   = start;
    TestStruct4 proto1;
    proto1.makeProto().formData({data_1, data_1 + sizeof(data_1)});
    NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
                                              std::chrono::high_resolution_clock::now() - end)
                                              .count());
    NEKO_LOG_INFO("Serializer data: {}", proto1.f0.size());
    end = std::chrono::high_resolution_clock::now();
    TestStruct4 proto2;
    proto2.makeProto().formData({data_2, data_2 + sizeof(data_2)});
    NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
                                              std::chrono::high_resolution_clock::now() - end)
                                              .count());
    NEKO_LOG_INFO("Serializer data: {}", proto2.f0.size());
    end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto3;
    // proto3.makeProto().formData({data_3, data_3 + sizeof(data_3)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto3.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto4;
    // proto4.makeProto().formData({data_4, data_4 + sizeof(data_4)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto4.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto5;
    // proto5.makeProto().formData({data_5, data_5 + sizeof(data_5)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto5.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto6;
    // proto6.makeProto().formData({data_6, data_6 + sizeof(data_6)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto6.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto7;
    // proto7.makeProto().formData({data_7, data_7 + sizeof(data_7)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto7.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto8;
    // proto8.makeProto().formData({data_8, data_8 + sizeof(data_8)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto8.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    // TestStruct4 proto9;
    // proto9.makeProto().formData({data_9, data_9 + sizeof(data_9)});
    // NEKO_LOG_INFO("Serializer time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(
    //                                           std::chrono::high_resolution_clock::now() - end)
    //                                           .count());
    // NEKO_LOG_INFO("Serializer data: {}", proto9.f0.size());
    // end = std::chrono::high_resolution_clock::now();
    NEKO_LOG_INFO("total time: {}s", std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::debug);
    return RUN_ALL_TESTS();
}
