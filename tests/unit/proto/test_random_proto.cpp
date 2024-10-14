#include <gtest/gtest.h>

#include "nekoproto/proto/binary_serializer.hpp"
#include "nekoproto/proto/json_serializer.hpp"
#include "nekoproto/proto/proto_base.hpp"
#include "nekoproto/proto/serializer_base.hpp"
#include "nekoproto/proto/to_string.hpp"
#include "nekoproto/proto/types/array.hpp"
#include "nekoproto/proto/types/binary_data.hpp"
#include "nekoproto/proto/types/enum.hpp"
#include "nekoproto/proto/types/list.hpp"
#include "nekoproto/proto/types/map.hpp"
#include "nekoproto/proto/types/set.hpp"
#include "nekoproto/proto/types/struct_unwrap.hpp"
#include "nekoproto/proto/types/tuple.hpp"
#include "nekoproto/proto/types/variant.hpp"
#include "nekoproto/proto/types/vector.hpp"

NEKO_USE_NAMESPACE

// struct test
struct TestStruct1 {
    std::map<std::string, int> f0 = {{"Z9sdL0MSyN", -1923945060}, {"lYAQQJEiJt", 229373784},
                                     {"3oaqfJCybR", -804753647},  {"oCAA2buT1v", -1207021257},
                                     {"Scdb2dhQme", 1456449854},  {"XmFyySGHjw", -801486992},
                                     {"pPBdhZDypm", -412129266},  {"IsQw22r6Dz", -1783175530},
                                     {"FlL5YGN8h0", -463335142},  {"0rWYZLUsS9", -2143085657}};
    NEKO_SERIALIZER(f0)
    NEKO_DECLARE_PROTOCOL(TestStruct1, JsonSerializer)
};

struct TestStruct2 {
    int f0                        = -403291840;
    std::map<std::string, int> f1 = {{"rp8Ua0jmJp", 1987895180},  {"f2w4LeQRrW", 664158635},
                                     {"svbfNxmvr9", -1716023598}, {"VP7irxyVbD", -1069926786},
                                     {"D6bFspHh41", 339153760},   {"02DGaeNlcE", -1532124426},
                                     {"Fw670fTD4K", -2001919954}, {"B6qlTNOxMS", 1711337555},
                                     {"WgohiD6Y69", 1548795319},  {"HBy4LJO5NA", -138078076}};
    NEKO_SERIALIZER(f0, f1)
    NEKO_DECLARE_PROTOCOL(TestStruct2, JsonSerializer)
};

struct TestStruct3 {
    std::vector<int> f0 = {1711541063,  -2070819069, 76618561,   493743238,   1189907426,
                           -1143241877, 1146877503,  -405659334, -1126159121, 2074572908};
    double f1           = -8.020863616155006e+299;
    std::string f2      = "NqWUnEusv8";
    NEKO_SERIALIZER(f0, f1, f2)
    NEKO_DECLARE_PROTOCOL(TestStruct3, JsonSerializer)
};

struct TestStruct4 {
    int f0         = -1997678378;
    bool f1        = false;
    double f2      = -3.1746509726272533e+299;
    std::string f3 = "G34HHyzcl5";
    NEKO_SERIALIZER(f0, f1, f2, f3)
    NEKO_DECLARE_PROTOCOL(TestStruct4, JsonSerializer)
};

struct TestStruct5 {
    std::vector<int> f0           = {-841425325,  -1130877369, 2043097867, 59654119,   -601892324,
                                     -1224007400, -950461210,  -694312697, 2106800445, 1680227814};
    std::map<std::string, int> f1 = {
        {"K3XESgJmVK", -1935803076}, {"0U7o3hgCtR", 896095582},  {"CuxZhTfY7j", 199125974}, {"AQ6eiXUjlY", 2082062355},
        {"2otZbZ7cpi", -1213582835}, {"Iu7Y6HkA75", 1209328957}, {"sprOPoh3CW", 57442396},  {"wjCMpCTI4J", -432965320},
        {"QyAtLgjkwR", -1996194884}, {"I6oCAuPZDm", -1986265413}};
    double f2      = -8.014055252028164e+299;
    std::string f3 = "nCAt5yTzBl";
    std::string f4 = "GhseYHwnaY";
    NEKO_SERIALIZER(f0, f1, f2, f3, f4)
    NEKO_DECLARE_PROTOCOL(TestStruct5, JsonSerializer)
};

struct TestStruct6 {
    std::map<std::string, int> f0 = {{"GwSfm0tpOn", -1098423654}, {"P965eFit32", -10956276},
                                     {"3E8AzussMP", -834378561},  {"XjQudcPIe4", 1705536507},
                                     {"ifIuEdte2V", 1205238480},  {"iUM09OVsUk", -1021717616},
                                     {"5dwB0JHVwS", -663168167},  {"CjUk9yu1L6", 1391079},
                                     {"1pSvhlHldc", 151844755},   {"Qaf3EmlP9X", 1554448116}};
    double f1                     = -5.293677734807105e+299;
    bool f2                       = false;
    double f3                     = -8.430619231519632e+299;
    double f4                     = -8.669764205002429e+299;
    double f5                     = 1.6864865661185284e+299;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5)
    NEKO_DECLARE_PROTOCOL(TestStruct6, JsonSerializer)
};

struct TestStruct7 {
    std::string f0 = "09poko43AE";
    int f1         = -428971019;
    bool f2        = false;
    int f3         = -1324089226;
    int f4         = 397916368;
    double f5      = -1.3226163234454003e+299;
    std::string f6 = "EUrm8YmUZP";
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6)
    NEKO_DECLARE_PROTOCOL(TestStruct7, JsonSerializer)
};

struct TestStruct8 {
    int f0                        = 712605329;
    std::string f1                = "kNhygPfWzA";
    int f2                        = 1666921291;
    std::map<std::string, int> f3 = {{"5V9GSjAcYx", -1835427543}, {"IXdctAWl1o", -632992051},
                                     {"gGfaVFsDoC", 757321775},   {"KeTFthdpfJ", 1821966045},
                                     {"3u4OknZfEY", -763215700},  {"5Pm3d5pL6S", -1930683770},
                                     {"5EXOIuvucF", -555997305},  {"94MzRfTtBM", 571737101},
                                     {"PSqbUPzkme", -1687568567}, {"kycIe57yGJ", 1725386047}};
    bool f4                       = false;
    std::vector<int> f5           = {-165027441, -735330697,  922168966,  -1536244760, -281856968,
                                     63246468,   -1384098944, 1519340291, 1044952351,  -2111042490};
    bool f6                       = true;
    std::vector<int> f7           = {464755467, -879940331, -1019281719, -77544801,   1539154156,
                                     168279865, -586950980, 1789708201,  -1758792108, 212034928};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7)
    NEKO_DECLARE_PROTOCOL(TestStruct8, JsonSerializer)
};

struct TestStruct9 {
    std::map<std::string, int> f0 = {{"hUOHvaizWM", 1653690764},  {"4oipYHWbZs", 375044897},
                                     {"w8w3OvTGkL", -1612941357}, {"o687vMQBXc", -1126061092},
                                     {"cUEvjwCcvd", 2023100212},  {"H5KRwca7uK", 1494637057},
                                     {"HdzflALsmr", -36613685},   {"LAjploxx8O", 1178123253},
                                     {"pOl0el4Eq9", 1690992401},  {"cXpsZhEA7b", -1640271012}};
    std::vector<int> f1           = {-1941058169, -1162850707, 574390989,   -1765191017, -721703922,
                                     -1844633443, 752275119,   -1009245902, -1073630691, -1158548134};
    bool f2                       = true;
    int f3                        = -728677935;
    double f4                     = -3.0922775317346423e+298;
    int f5                        = -1318010881;
    int f6                        = 341412847;
    std::vector<int> f7           = {98430907,   -1130035720, -2006899859, -1758901675, -779474323,
                                     -852477140, 2035277653,  405320940,   706859183,   1177366166};
    int f8                        = -1428370322;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8)
    NEKO_DECLARE_PROTOCOL(TestStruct9, JsonSerializer)
};

struct TestStruct10 {
    std::vector<int> f0           = {1260526507,  -1268346044, -1924515098, 93549855,    -198760288,
                                     -1690894174, -909715670,  542895005,   -1928956624, 2046410681};
    int f1                        = -1778270581;
    double f2                     = -1.945749623536153e+299;
    std::string f3                = "tdt2XV6wfS";
    bool f4                       = true;
    std::string f5                = "a5LXAITxco";
    bool f6                       = true;
    double f7                     = -1.070931628477785e+299;
    std::vector<int> f8           = {-1071081592, -886109948, -1581868160, 1489268637,  -900225185,
                                     1097612067,  1693588769, 1595434707,  -1917566955, -459803661};
    std::map<std::string, int> f9 = {
        {"1njlWivsaz", 2070719656}, {"cMzmXNeSSm", -653996030}, {"3o2ogevaWk", 1258048995}, {"hSD2ROCXAi", -1136682342},
        {"rgCNNMhNtI", 1283070840}, {"5msCPGa4aZ", 633468870},  {"4AP4UwmqON", 1191045916}, {"HaGXgmtvhU", -1613634433},
        {"uokvqDOlXb", -538105088}, {"YPJqntpjvL", -1038533331}};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9)
    NEKO_DECLARE_PROTOCOL(TestStruct10, JsonSerializer)
};

struct TestStruct11 {
    std::vector<int> f0           = {-160815678, 1961244977,  -1052692650, 2129260414, -1150817350,
                                     908480604,  -1116126869, 502913935,   2076035654, 1843131733};
    double f1                     = 9.311396816669503e+299;
    bool f2                       = false;
    std::vector<int> f3           = {-1259459748, 1527577674,  176203404,  518439881, 1760597335,
                                     -826774748,  -1005826243, 1238429299, 711683036, -1175820646};
    std::string f4                = "q6ciSGN7yy";
    int f5                        = 1604417695;
    std::map<std::string, int> f6 = {{"a6EA6Dxaoa", 328180395},   {"cKx5OsjsPJ", 1074155433},
                                     {"b6XSOEXBNb", 1767906646},  {"qacyWUvirD", -756098389},
                                     {"XrywWCWrTr", -15517421},   {"VewNGpNCHO", -2047364718},
                                     {"1TrCnX9KVy", -1538505189}, {"OmTujHkoUX", 297863076},
                                     {"ec4bTb3QSc", 1367588635},  {"l35S6eRc6n", 1179336267}};
    std::vector<int> f7           = {330624278,  940488361,  1667639906,  -1079353061, 485396940,
                                     -956628388, -709770812, -1158440861, 747822549,   115140283};
    std::map<std::string, int> f8 = {
        {"TgQgD5HZXC", 65243325},    {"OXXBpLa2wB", 1122665588}, {"OR0MaJS8Hn", 566073110},  {"IZ8FCKi4Ru", 1225056615},
        {"1GXywivNsa", 278079381},   {"f9BQVERLqA", 563361278},  {"Wnr28WY2vT", 1295798595}, {"ISAFZc395y", 232340127},
        {"DuEFdBaPbB", -1807750673}, {"aIBNGMHwL4", -575711666}};
    int f9  = -2099173184;
    int f10 = 1799437830;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10)
    NEKO_DECLARE_PROTOCOL(TestStruct11, JsonSerializer)
};

struct TestStruct12 {
    std::vector<int> f0           = {1334246526, -93295146,  1831157455, -1485097097, 1624813195,
                                     1332373122, -765697351, 1040759936, 222289133,   601226087};
    bool f1                       = false;
    std::map<std::string, int> f2 = {{"wmqPYeIkKP", -1689098612}, {"hHvVQwkjDQ", 80792084},
                                     {"wcYCmCptcy", -1979199845}, {"eMva65at6v", 1441280664},
                                     {"LKQMysQSG8", -1462747171}, {"T05dhM9lCx", -1468913027},
                                     {"RyxHejouI8", -80681361},   {"jGtWfoSlAw", 1295526728},
                                     {"4aJykkHOAF", 2126999392},  {"2GGH80FVhR", 1779882176}};
    std::map<std::string, int> f3 = {
        {"iPEjHNxVvV", 568722464},  {"jLaGD3PeZr", -2091529540}, {"bAgykme23P", 463906157},  {"8vASsFjUra", 1150766839},
        {"yFZe64IQEP", 244594031},  {"c2qBxL3ICD", 1992749811},  {"yekiItpzg5", -842402979}, {"nOVQARMGXD", 255034420},
        {"FfJ4pMtRgG", -649793668}, {"5GHVVbRAzm", 1888484637}};
    std::string f4                = "jVHLt45eFM";
    std::map<std::string, int> f5 = {{"cHHjxYEEK4", 722593805},  {"vuJ8Yg4vAJ", 1517275424}, {"hG9EmAhXaH", -449059719},
                                     {"NJLK0AslsI", -231661239}, {"TIdgQ0tsfv", -826979537}, {"vjJDx5yX6X", 1609913570},
                                     {"K2csgMO2NP", 1755704426}, {"QvwVBeOpmN", 1051724781}, {"Y4ldK57UvG", -204204775},
                                     {"6jE50OeStZ", 920024288}};
    std::map<std::string, int> f6 = {{"eCHbRzV8JM", -1812512750}, {"NgT1AX2iWj", 1335440501},
                                     {"yGU6rzjEvC", 1046034272},  {"aKwrlH8avy", 1900707154},
                                     {"O9s7iYTPPH", -1487465496}, {"FmJZobHIES", -1439865829},
                                     {"h6pYx5JPrn", 661783420},   {"tbNduHmFD4", 1967314736},
                                     {"MXXUhCmxBu", 746611301},   {"H2lwE9mFqn", 1219688623}};
    int f7                        = -867344323;
    std::vector<int> f8           = {815291316,   1236592867, -1706450195, -1621455146, 460068460,
                                     -2021934667, 415320210,  -78385022,   -651971040,  -1867926428};
    std::map<std::string, int> f9 = {{"wYYcn811hW", -2086840136}, {"iTAKYpd1FH", -1305279957},
                                     {"auChuioeWK", 1553277670},  {"L8wuRI49zY", -171577919},
                                     {"CDipJlr6Jb", -1103233968}, {"BAgaZpMO7Z", -639139778},
                                     {"rT8iERYy5s", -953605172},  {"ZlRsizXkFB", -509500295},
                                     {"YDJ6VvU2cr", 1985784144},  {"LUt44FonDB", 1966172968}};
    bool f10                      = false;
    std::map<std::string, int> f11 = {{"JUdPvGUXel", -1767430220}, {"eANal1UySD", 1730546921},
                                      {"5oVA7rSSd1", -1798840477}, {"bvbboSz5pt", -585516128},
                                      {"XWK6dHTkBa", 1184730261},  {"33fXAdu2eN", -1536601824},
                                      {"lMx2GfnWiC", 1035049197},  {"X3xFqK04sd", -587848236},
                                      {"UXQfq7I3ox", -1313674462}, {"rotwrW89qB", 1275277449}};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11)
    NEKO_DECLARE_PROTOCOL(TestStruct12, JsonSerializer)
};
struct TestStruct13 {
    int f0                        = 140439030;
    std::vector<int> f1           = {1199381575,  -1709165624, 1643080124, 2051504241, 850971654,
                                     -1120497398, -2055977578, 29986805,   1846031760, 1158428028};
    int f2                        = -2115305793;
    bool f3                       = true;
    bool f4                       = false;
    std::vector<int> f5           = {1576945102, 708190293,   507199113,  -254500786, 970632424,
                                     -840444616, -1008644274, -681576860, -747807370, -1918306817};
    std::map<std::string, int> f6 = {
        {"OfekZH9pNB", 1089822531}, {"subc1okvYm", 225642239},  {"vqW9n4Sk3L", 712394709},  {"hDDHfLp9x9", -1370341169},
        {"BojNEg00qJ", 1403265386}, {"hYToGFKAkE", 495204614},  {"VHXxUBUuXU", -913068606}, {"bKPCwN6AIL", -1887949359},
        {"as7Ax6SEIX", 1559154635}, {"YEpvngrpJs", -1183367918}};
    std::map<std::string, int> f7 = {{"n4kyI2HuqU", -1725376334}, {"i0E4LrX8Cm", 756766536},
                                     {"ylxcNq4Pl1", 1460483273},  {"XieStuGYOt", -1154133283},
                                     {"Y9igBA8qym", 6568621},     {"4d8RhSQyqP", 1556608812},
                                     {"4vWfkbANQS", 1503370470},  {"X71sDUMXFv", -579118050},
                                     {"w78Td0pBWq", 109537650},   {"vhtMs2awO9", 797391705}};
    std::string f8                = "i8nWdYmiCB";
    std::string f9                = "sQulQx4Avx";
    double f10                    = 9.392091297064629e+299;
    bool f11                      = false;
    std::vector<int> f12          = {1721456034, -1976015974, 407613471,   2015417178,  20930173,
                                     662586649,  405945162,   -1848589438, -1375729264, -271357978};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12)
    NEKO_DECLARE_PROTOCOL(TestStruct13, JsonSerializer)
};
struct TestStruct14 {
    std::map<std::string, int> f0  = {{"9eqZW4rgTw", 1264907124},  {"UpZoVhM9o7", -40737161},
                                      {"MTBFZZiDjy", -1442913617}, {"FGeQiL2SiI", 642110767},
                                      {"7qRwn549Xm", -1766510931}, {"EhqrDSr5HR", -414746096},
                                      {"ian2ToeJIE", -747737622},  {"IdNSCDUVKw", -2048376340},
                                      {"6NlYuVe4f5", 970298841},   {"CjY79XQRW0", 150374998}};
    std::string f1                 = "g50uZortu9";
    double f2                      = 4.54110037257389e+299;
    bool f3                        = false;
    std::string f4                 = "6b44nynBmv";
    int f5                         = 718357576;
    std::map<std::string, int> f6  = {{"9xKuAOYYRn", -1107378642}, {"O5Z14TsoSP", 1301191147},
                                      {"BeN8X0CtTG", -1751965913}, {"V1lg6MVQyj", 1439032196},
                                      {"VvyTZOv1B2", -1527441841}, {"ZHUnyF5Zlb", 8636296},
                                      {"juGKZoiG4T", 937799573},   {"hJWS9hxDDx", -1420050572},
                                      {"HvmTiWCIUo", 1117896630},  {"yVMJ9bLijD", -1254646052}};
    bool f7                        = true;
    bool f8                        = false;
    std::string f9                 = "IzzDPAec89";
    int f10                        = -1263498881;
    std::map<std::string, int> f11 = {{"8zfNNmb5VR", -1808517035}, {"y9d4LKJhCj", -1602762881},
                                      {"2WeWTLBF30", 722017044},   {"TKklV8HWxG", 109529602},
                                      {"HMk57D4rcl", -1524910675}, {"V507KqCmoZ", -1281225634},
                                      {"go5jTIHV51", -569833480},  {"t9N8cyLC6U", -1640272944},
                                      {"WxoJlOYg1I", -1146577958}, {"GODiah1Fll", 1975822081}};
    bool f12                       = true;
    std::vector<int> f13           = {1246824659,  274367630,   -955892856,  -2129439102, 853533940,
                                      -1015389432, -2032437625, -1875700883, 196584421,   -1943568121};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13)
    NEKO_DECLARE_PROTOCOL(TestStruct14, JsonSerializer)
};

struct TestStruct15 {
    std::map<std::string, int> f0 = {{"p7hv2YMJsN", 1301565406},  {"oe9mmor8Xf", -1507268335},
                                     {"pWh1b2JmTw", 1380747324},  {"MicQV8XQqZ", -295454715},
                                     {"HmtUp43zkF", 240848983},   {"EQGDxgCfPv", -1370618413},
                                     {"bbC0D9xhPm", -1440192146}, {"HitpSFsgnr", 1880311753},
                                     {"Af3U2IYXak", 864338892},   {"XjWoCpnVde", 1376765762}};
    bool f1                       = false;
    int f2                        = -1707136232;
    bool f3                       = true;
    bool f4                       = false;
    std::map<std::string, int> f5 = {
        {"ZheP3Hs1DE", -1899070037}, {"gDz9ll4qH3", 1708849796}, {"O2vHEnQCMc", -330619818}, {"8OpIhKYs9H", 357839062},
        {"KeNCIrQIAs", 1411146404},  {"xJ1iYm10im", -517615819}, {"fKT1xmK2hM", -640862379}, {"yBhlnCdpc1", -68955985},
        {"k2W3eTF67c", 141294468},   {"pjcoPIU5im", 1538200631}};
    int f6                         = -112302195;
    int f7                         = 1095664676;
    int f8                         = 973871639;
    double f9                      = -8.553138637753277e+299;
    double f10                     = 3.7528446099373706e+299;
    int f11                        = 2102526383;
    bool f12                       = true;
    std::map<std::string, int> f13 = {
        {"BXoCLwO7OK", -650902479}, {"7JrEyS4fVa", -41779304},  {"5adbTftA6g", -1470022815}, {"fcFe0l5fWI", 2079375942},
        {"m92NTyik6S", 80834182},   {"UlFj8gUCVC", 1257731842}, {"4KuuPHmNWn", -1180781329}, {"GtU3pj00P3", 658514161},
        {"hcDnOotHEP", -593319917}, {"yJ9LnBKj6B", 1743020529}};
    bool f14 = false;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14)
    NEKO_DECLARE_PROTOCOL(TestStruct15, JsonSerializer)
};

struct TestStruct16 {
    std::string f0                = "8UqK152Ly4";
    double f1                     = -5.520162846990078e+299;
    std::map<std::string, int> f2 = {
        {"Rx9IHmJI7j", 1120585797}, {"n5K1EnWzKe", -95448647},   {"rYUgaxUBnR", 1256709512}, {"hAx0nz3VJ1", 809980861},
        {"LQxhkhr81G", -708650655}, {"hCs6XNUa7q", -1581032190}, {"0W163nqXSm", 1255329318}, {"79uD0oM1CI", 1401799066},
        {"N3JyDEraLw", 400286892},  {"aCZgnf3T9D", 460110730}};
    bool f3                        = false;
    std::vector<int> f4            = {-903702834, 752148286,   1509793320, -44826494,   974399094,
                                      188482699,  -1732512977, -218360303, -2095762147, 634026564};
    double f5                      = -4.304484157904056e+299;
    double f6                      = 5.1221361626200565e+299;
    int f7                         = -1209383336;
    std::string f8                 = "C1DKTGqtG4";
    std::vector<int> f9            = {-1541546472, 1278278994, -534906865, 1649176852, 1908457236,
                                      2006001032,  -794317636, 1941787394, 1741192235, 1999795746};
    std::string f10                = "pDywdKrZ50";
    std::vector<int> f11           = {-1957484690, -284939977, -183242407, -133655566, -1730983454,
                                      -1246899606, 68073355,   -388884874, 457207768,  -1290414593};
    std::vector<int> f12           = {1789916530, -2034712133, -1861657011, 218123378, 469577906,
                                      1231522570, -799226635,  1671554726,  175406541, 1805380860};
    int f13                        = 1036926430;
    bool f14                       = false;
    std::map<std::string, int> f15 = {
        {"PLKLyuUN8U", 2082902142}, {"zF7BpRnIM4", 1361145486}, {"9FrnmFL8Cu", 1335604512}, {"olitokZ3cO", 79035524},
        {"X5yykhTJrg", -967871719}, {"UEFKtwbLcZ", -191065107}, {"q1mkt3P9oA", -361657629}, {"qAIu7jHsGn", 1940888234},
        {"B3nJw87DOc", 67012226},   {"M5GFd9PcSa", 1586614656}};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15)
    NEKO_DECLARE_PROTOCOL(TestStruct16, JsonSerializer)
};

struct TestStruct17 {
    std::map<std::string, int> f0 = {{"eFuUbz9OBn", 1800133404},  {"leLyKPCC4z", 1367551459},
                                     {"Opra8jymmW", -959690510},  {"bQ2gQrU0vq", -2125737814},
                                     {"D3UZdhcvlM", 1169837736},  {"N1Li4HpDTS", 2132394065},
                                     {"VYtYSOD7S6", -1561920636}, {"r2Ls9TIX8m", 1914649448},
                                     {"lXo9ItaInE", -2090146326}, {"xepgMQELjo", 1044829346}};
    bool f1                       = false;
    std::map<std::string, int> f2 = {
        {"aClXzQbZdH", -833808678},  {"5EJzTmm4lr", -425489774}, {"EYy5ha0bX5", -746036618}, {"c6dMVg1HfZ", 917657674},
        {"fmYV5p0MX1", -473290114},  {"ACAs2gVha2", 943832429},  {"VIeqnu3Xhv", 1526852568}, {"8swpBB6UcG", 1909576215},
        {"fMTXJxdrAn", -1472944801}, {"4kASKvrqMV", -517549963}};
    std::string f3                = "1gvup75qGE";
    std::string f4                = "8JdUKx3j9f";
    bool f5                       = false;
    bool f6                       = true;
    bool f7                       = true;
    std::map<std::string, int> f8 = {
        {"bLEtyizut0", -220158612}, {"Ikq5YjD2lK", 668827682},  {"cuS7dD3wpp", -1155677953}, {"Xjqi9xFMOO", 1328455667},
        {"8p4jeExHxR", 1109078617}, {"QOSwLAOBJs", 313829593},  {"OaACzP0YaD", -1094213641}, {"QRF69fnlG0", 1748783459},
        {"34P1pnFMUm", 1158877275}, {"LRi0Ixccv5", -1685591447}};
    double f9                      = 5.912785266045141e+299;
    std::vector<int> f10           = {-1049446885, 1980941246, 641461462,   561134667,  -1189007154,
                                      -1558716156, 1926262569, -1250352099, -734054160, -1013161577};
    double f11                     = -5.44111085112734e+299;
    double f12                     = 5.1479877490965724e+299;
    bool f13                       = true;
    std::map<std::string, int> f14 = {{"N1pCY8ZjKS", -383193394},  {"g59232WPuO", 1099368033},
                                      {"6Rhl5wbMDs", -1640439469}, {"hXuw15mzzu", -1486099392},
                                      {"DqavJAPLXK", 1897656735},  {"XbptjVMorS", -1789206648},
                                      {"IOnzlg7fBU", 250181063},   {"mMfB3wROIC", -2006659003},
                                      {"Ce0yE9UXkz", 1493594178},  {"JMrBMULBbb", -1255449589}};
    std::map<std::string, int> f15 = {{"56f0Vrb9d3", -121556205},  {"k0W1fTSXeo", -164643549},
                                      {"p4uVaiCMO9", -283726970},  {"IXbZnbUijV", -1434278203},
                                      {"CxZqIhOWi9", 1000977665},  {"X6S8rokCxx", -1525548417},
                                      {"piFfZxPEzK", -1232377789}, {"hd7Ic4IrlS", 62753913},
                                      {"Yr2XER2WlI", 1436637656},  {"CQnp8NIxwz", -1513856866}};
    std::string f16                = "ZI6TSl14e9";
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16)
    NEKO_DECLARE_PROTOCOL(TestStruct17, JsonSerializer)
};

struct TestStruct18 {
    std::string f0                 = "gK5sWsKxeE";
    std::string f1                 = "Uo2QKcC5tC";
    bool f2                        = false;
    std::string f3                 = "jlrxrCCt5I";
    bool f4                        = false;
    std::string f5                 = "nTYgFG8hWK";
    std::string f6                 = "Vs0uAxENzS";
    std::map<std::string, int> f7  = {{"JyMEaSFYka", -205734349},  {"fUlOnZARqf", 708047835},
                                      {"KyBJdq2TDX", -1184082975}, {"HPUJ8E2rJL", 1624257222},
                                      {"pfDmL0ebHX", -167288432},  {"Ks90xSHkom", -2048872134},
                                      {"fhP57Iq1O4", 1150867519},  {"wL9FLP9o44", 1363348093},
                                      {"pcnPwlvgbu", 1168571919},  {"iewJarqEfC", 2071913611}};
    bool f8                        = true;
    std::map<std::string, int> f9  = {{"kqGO3qQU0d", -1276405907}, {"xCJgiLUxXP", 272158188},
                                      {"rsoCcInomL", 985522758},   {"ugX8mp3x3g", -1093930245},
                                      {"jLeaE0iiDS", 311367716},   {"C8EoxPCSkF", 1371311110},
                                      {"XeOCcejanH", -597600995},  {"Q3Nt9plBhu", -1399829992},
                                      {"mp8cQutjBE", 1329987099},  {"pLBB88dAZN", 969034295}};
    std::map<std::string, int> f10 = {{"GCD5H4mLNz", -2012107728}, {"jReGfsd3og", 1628726362},
                                      {"DeG97IVkZD", 1490896588},  {"DBfhBaaIHI", -85010246},
                                      {"0MEDUMvlqZ", 382469641},   {"8BqpKBCMAh", 936303159},
                                      {"ogAa2642kC", 1361088633},  {"XdXthoCF4R", -1366218033},
                                      {"aDOR5Nj3iJ", 1878911007},  {"lQqxHDWiXX", -1955720766}};
    std::vector<int> f11           = {-1953579093, -1990230928, 81210858,   -1568972053, -1096135825,
                                      500446266,   1179666118,  1265138686, 139583518,   1272784009};
    std::string f12                = "680lHapAjx";
    std::map<std::string, int> f13 = {{"ziWUMlEJ5a", -41935483},  {"WWQ16v2NHE", -1546143464},
                                      {"tZKJrahAxj", -327702319}, {"xsHW1UtCQw", -1094358881},
                                      {"ZwY1e2AjGh", -771986385}, {"ZfA5tqO1mS", 901619184},
                                      {"JvZIJvBBjE", 1812880503}, {"6ncYUDHZsn", -688271268},
                                      {"oKd80gqHeY", 1126509675}, {"BX77m4WbiZ", 555431258}};
    std::map<std::string, int> f14 = {{"wcQPP8s3et", -1296556703}, {"WZ0UNGrQNQ", -1105590133},
                                      {"2beUroJ44E", 1882392933},  {"MwuchfpXO4", 1112098416},
                                      {"cJUFckJsJZ", 104638068},   {"G5wPv0LgCC", 1531258274},
                                      {"BfFYB6ASNj", -1978961530}, {"HkRczraUeI", 951676732},
                                      {"tIMQxpbbFP", 434140882},   {"I01u7P4oBi", 606556761}};
    std::vector<int> f15           = {-1730540083, 1010799014, 185997887,  586817764,  1396283456,
                                      -1757370030, -825449466, 1488124742, 1969021455, -783077939};
    int f16                        = -1136692906;
    std::map<std::string, int> f17 = {
        {"2QG8XZmBiQ", -144032262}, {"0vK2mS3sGi", 952465581},  {"1fneXeN5ZI", 485405504},   {"ePvI0pKSzw", -519052323},
        {"OOOrUgsVck", 1690914018}, {"PGnBtt9sWE", -877096830}, {"CCjjrtyL9F", -1838820520}, {"mdfrz9aE9s", 1985178003},
        {"yxGYJFZ5ed", 1516615749}, {"XoAc98BqQi", 501405147}};
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17)
    NEKO_DECLARE_PROTOCOL(TestStruct18, JsonSerializer)
};

struct TestStruct19 {
    int f0                        = 598075644;
    bool f1                       = false;
    double f2                     = -1.9262207448980825e+299;
    std::vector<int> f3           = {1758433030, 187191160,  -541943362, 527529635,  846650413,
                                     1135127439, 1601474229, -670554514, -215207722, -1186499333};
    std::map<std::string, int> f4 = {{"k0Icmt5MFU", -1762190592}, {"drJ1onP931", -1125762802},
                                     {"fWD8pS0hVB", 1817610784},  {"ow6CcKRH08", 3687983},
                                     {"8ukVaXJ2ji", -703561816},  {"ummZThsP5j", -938556413},
                                     {"1ml0KWgOVz", 81043610},    {"gJ2LSkdzs8", 1102567764},
                                     {"h0WD4L3Ujz", 1871623417},  {"i4azQTgHNy", 1896919646}};
    std::map<std::string, int> f5 = {{"d9YvqnKNES", 493699908},   {"TZEKUnyc5i", 237594798},
                                     {"df2EWorffk", 355855309},   {"wmOIYcTpb3", 93772844},
                                     {"rTL7K3dmVx", -38035850},   {"oMPErm2m97", -1782928231},
                                     {"uuXgkXHapG", -1207790187}, {"81MBYzAjlp", -343327722},
                                     {"Rre2D18Inv", -1478672815}, {"p2gCc4htBy", 1895567497}};
    double f6                     = -4.547741891541037e+298;
    int f7                        = 1252813630;
    int f8                        = 1558974552;
    bool f9                       = true;
    bool f10                      = true;
    double f11                    = -6.026662587681791e+299;
    double f12                    = 1.212091766547016e+299;
    std::vector<int> f13          = {1972136442,  1669211356, 243599697,  -534725913, -2141442556,
                                     -1544881618, -253776146, -614679911, -982103703, -1489310303};
    std::vector<int> f14          = {-377053494, 1624270114, 1541788567,  -519832428, -125356216,
                                     572733296,  1834988006, -1701416845, -773171798, -1441417850};
    double f15                    = -6.1755398757925176e+299;
    int f16                       = -2113865031;
    std::string f17               = "MvBuFwDNjf";
    bool f18                      = true;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18)
    NEKO_DECLARE_PROTOCOL(TestStruct19, JsonSerializer)
};

struct TestStruct20 {
    bool f0                       = true;
    bool f1                       = false;
    std::map<std::string, int> f2 = {
        {"PFbTyAVFdX", -1699892411}, {"rewjcHiC5R", 829651907},  {"DeUrDNGT5U", 925196340},  {"HUamHDSsZi", 1015354322},
        {"0A7wgjq3iH", 1676105526},  {"3KCVFNIh2z", -943099472}, {"VuWKSuiThy", -396884201}, {"s8DMInSY0a", -359559296},
        {"2yALraYz9E", 540430261},   {"la1SE6DRlu", 368994906}};
    int f3                         = -1217186063;
    bool f4                        = true;
    std::string f5                 = "vHH0lV7PdL";
    double f6                      = -8.89007510263416e+299;
    std::string f7                 = "iSW1MvDDhx";
    bool f8                        = true;
    std::vector<int> f9            = {1434168112, 1394394923,  -329658532,  2117281689,  491118759,
                                      1934338126, -1907530676, -1330636265, -1958621240, 873869169};
    std::map<std::string, int> f10 = {{"YEBFkYx7XX", 1681573733},  {"4U1tLopkk6", 1175823387},
                                      {"JcinROPUHo", 380649438},   {"cMJLoJbSjT", 1346257146},
                                      {"SCgRoYSarc", -1101501130}, {"xedsAhkLLS", 1992665467},
                                      {"5mquViNCSx", -1876874064}, {"MRRNBPJotQ", -1256569051},
                                      {"Ovxt8bqTEg", -2089712334}, {"p75hqCJ7Vi", -1135763142}};
    int f11                        = 1846576993;
    double f12                     = -1.2823210943653017e+299;
    double f13                     = 5.042635046449982e+299;
    bool f14                       = false;
    int f15                        = -536594644;
    std::vector<int> f16           = {-515425193,  404299928,   1249495646, 2089501676,  -1430204695,
                                      -1997988037, -1297755677, 1042958908, -1657888221, -1436946684};
    int f17                        = -1144695556;
    int f18                        = -1674200310;
    int f19                        = -1291818235;
    NEKO_SERIALIZER(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19)
    NEKO_DECLARE_PROTOCOL(TestStruct20, JsonSerializer)
};

struct TestStructProto {
    TestStruct1 a  = {};
    TestStruct2 b  = {};
    TestStruct3 c  = {};
    TestStruct4 d  = {};
    TestStruct5 e  = {};
    TestStruct6 f  = {};
    TestStruct7 g  = {};
    TestStruct8 h  = {};
    TestStruct9 i  = {};
    TestStruct10 j = {};
    TestStruct11 k = {};
    TestStruct12 l = {};
    TestStruct13 m = {};
    TestStruct14 n = {};
    TestStruct15 o = {};
    TestStruct16 p = {};
    TestStruct17 q = {};
    TestStruct18 r = {};
    TestStruct19 s = {};
    TestStruct20 t = {};

    NEKO_SERIALIZER(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)
    NEKO_DECLARE_PROTOCOL(TestStructProto, JsonSerializer)
};

constexpr static const char* StructProtoData =
    "{\"a\":{\"f0\":{\"0rWYZLUsS9\":-2143085657,\"3oaqfJCybR\":-804753647,\"FlL5YGN8h0\":-463335142,\"IsQw22r6Dz\":-"
    "1783175530,\"Scdb2dhQme\":1456449854,\"XmFyySGHjw\":-801486992,\"Z9sdL0MSyN\":-1923945060,\"lYAQQJEiJt\":"
    "229373784,\"oCAA2buT1v\":-1207021257,\"pPBdhZDypm\":-412129266}},\"b\":{\"f0\":4,\"f1\":{\"02DGaeNlcE\":-"
    "1532124426,\"B6qlTNOxMS\":1711337555,\"D6bFspHh41\":339153760,\"Fw670fTD4K\":-2001919954,\"HBy4LJO5NA\":-"
    "138078076,\"VP7irxyVbD\":-1069926786,\"WgohiD6Y69\":1548795319,\"f2w4LeQRrW\":664158635,\"rp8Ua0jmJp\":1987895180,"
    "\"svbfNxmvr9\":-1716023598}},\"c\":{\"f0\":[1711541063,-2070819069,76618561,493743238,1189907426,-1143241877,"
    "1146877503,-405659334,-1126159121,2074572908],\"f1\":-8.020863616155006e299,\"f2\":\"NqWUnEusv8\"},\"d\":{\"f0\":-"
    "1997678378,\"f1\":false,\"f2\":-3.1746509726272533e299,\"f3\":\"G34HHyzcl5\"},\"e\":{\"f0\":[-841425325,-"
    "1130877369,2043097867,59654119,-601892324,-1224007400,-950461210,-694312697,2106800445,1680227814],\"f1\":{"
    "\"0U7o3hgCtR\":896095582,\"2otZbZ7cpi\":-1213582835,\"AQ6eiXUjlY\":2082062355,\"CuxZhTfY7j\":199125974,"
    "\"I6oCAuPZDm\":-1986265413,\"Iu7Y6HkA75\":1209328957,\"K3XESgJmVK\":-1935803076,\"QyAtLgjkwR\":-1996194884,"
    "\"sprOPoh3CW\":57442396,\"wjCMpCTI4J\":-432965320},\"f2\":-8.014055252028164e299,\"f3\":\"nCAt5yTzBl\",\"f4\":"
    "\"GhseYHwnaY\"},\"f\":{\"f0\":{\"1pSvhlHldc\":151844755,\"3E8AzussMP\":-834378561,\"5dwB0JHVwS\":-663168167,"
    "\"CjUk9yu1L6\":1391079,\"GwSfm0tpOn\":-1098423654,\"P965eFit32\":-10956276,\"Qaf3EmlP9X\":1554448116,"
    "\"XjQudcPIe4\":1705536507,\"iUM09OVsUk\":-1021717616,\"ifIuEdte2V\":1205238480},\"f1\":-5.293677734807105e299,"
    "\"f2\":false,\"f3\":-8.430619231519632e299,\"f4\":-8.669764205002429e299,\"f5\":1.6864865661185284e299},\"g\":{"
    "\"f0\":\"09poko43AE\",\"f1\":-428971019,\"f2\":false,\"f3\":-1324089226,\"f4\":397916368,\"f5\":-1."
    "3226163234454003e299,\"f6\":\"EUrm8YmUZP\"},\"h\":{\"f0\":712605329,\"f1\":\"kNhygPfWzA\",\"f2\":1666921291,"
    "\"f3\":{\"3u4OknZfEY\":-763215700,\"5EXOIuvucF\":-555997305,\"5Pm3d5pL6S\":-1930683770,\"5V9GSjAcYx\":-1835427543,"
    "\"94MzRfTtBM\":571737101,\"IXdctAWl1o\":-632992051,\"KeTFthdpfJ\":1821966045,\"PSqbUPzkme\":-1687568567,"
    "\"gGfaVFsDoC\":757321775,\"kycIe57yGJ\":1725386047},\"f4\":false,\"f5\":[-165027441,-735330697,922168966,-"
    "1536244760,-281856968,63246468,-1384098944,1519340291,1044952351,-2111042490],\"f6\":true,\"f7\":[464755467,-"
    "879940331,-1019281719,-77544801,1539154156,168279865,-586950980,1789708201,-1758792108,212034928]},\"i\":{\"f0\":{"
    "\"4oipYHWbZs\":375044897,\"H5KRwca7uK\":1494637057,\"HdzflALsmr\":-36613685,\"LAjploxx8O\":1178123253,"
    "\"cUEvjwCcvd\":2023100212,\"cXpsZhEA7b\":-1640271012,\"hUOHvaizWM\":1653690764,\"o687vMQBXc\":-1126061092,"
    "\"pOl0el4Eq9\":1690992401,\"w8w3OvTGkL\":-1612941357},\"f1\":[-1941058169,-1162850707,574390989,-1765191017,-"
    "721703922,-1844633443,752275119,-1009245902,-1073630691,-1158548134],\"f2\":true,\"f3\":-728677935,\"f4\":-3."
    "0922775317346423e298,\"f5\":-1318010881,\"f6\":341412847,\"f7\":[98430907,-1130035720,-2006899859,-1758901675,-"
    "779474323,-852477140,2035277653,405320940,706859183,1177366166],\"f8\":-1428370322},\"j\":{\"f0\":[1260526507,-"
    "1268346044,-1924515098,93549855,-198760288,-1690894174,-909715670,542895005,-1928956624,2046410681],\"f1\":-"
    "1778270581,\"f2\":-1.945749623536153e299,\"f3\":\"tdt2XV6wfS\",\"f4\":true,\"f5\":\"a5LXAITxco\",\"f6\":true,"
    "\"f7\":-1.070931628477785e299,\"f8\":[-1071081592,-886109948,-1581868160,1489268637,-900225185,1097612067,"
    "1693588769,1595434707,-1917566955,-459803661],\"f9\":{\"1njlWivsaz\":2070719656,\"3o2ogevaWk\":1258048995,"
    "\"4AP4UwmqON\":1191045916,\"5msCPGa4aZ\":633468870,\"HaGXgmtvhU\":-1613634433,\"YPJqntpjvL\":-1038533331,"
    "\"cMzmXNeSSm\":-653996030,\"hSD2ROCXAi\":-1136682342,\"rgCNNMhNtI\":1283070840,\"uokvqDOlXb\":-538105088}},\"k\":{"
    "\"f0\":[-160815678,1961244977,-1052692650,2129260414,-1150817350,908480604,-1116126869,502913935,2076035654,"
    "1843131733],\"f1\":9.311396816669503e299,\"f2\":false,\"f3\":[-1259459748,1527577674,176203404,518439881,"
    "1760597335,-826774748,-1005826243,1238429299,711683036,-1175820646],\"f4\":\"q6ciSGN7yy\",\"f5\":1604417695,"
    "\"f6\":{\"1TrCnX9KVy\":-1538505189,\"OmTujHkoUX\":297863076,\"VewNGpNCHO\":-2047364718,\"XrywWCWrTr\":-15517421,"
    "\"a6EA6Dxaoa\":328180395,\"b6XSOEXBNb\":1767906646,\"cKx5OsjsPJ\":1074155433,\"ec4bTb3QSc\":1367588635,"
    "\"l35S6eRc6n\":1179336267,\"qacyWUvirD\":-756098389},\"f7\":[330624278,940488361,1667639906,-1079353061,485396940,"
    "-956628388,-709770812,-1158440861,747822549,115140283],\"f8\":{\"1GXywivNsa\":278079381,\"DuEFdBaPbB\":-"
    "1807750673,\"ISAFZc395y\":232340127,\"IZ8FCKi4Ru\":1225056615,\"OR0MaJS8Hn\":566073110,\"OXXBpLa2wB\":1122665588,"
    "\"TgQgD5HZXC\":65243325,\"Wnr28WY2vT\":1295798595,\"aIBNGMHwL4\":-575711666,\"f9BQVERLqA\":563361278},\"f9\":-"
    "2099173184,\"f10\":1799437830},\"l\":{\"f0\":[1334246526,-93295146,1831157455,-1485097097,1624813195,1332373122,-"
    "765697351,1040759936,222289133,601226087],\"f1\":false,\"f2\":{\"2GGH80FVhR\":1779882176,\"4aJykkHOAF\":"
    "2126999392,\"LKQMysQSG8\":-1462747171,\"RyxHejouI8\":-80681361,\"T05dhM9lCx\":-1468913027,\"eMva65at6v\":"
    "1441280664,\"hHvVQwkjDQ\":80792084,\"jGtWfoSlAw\":1295526728,\"wcYCmCptcy\":-1979199845,\"wmqPYeIkKP\":-"
    "1689098612},\"f3\":{\"5GHVVbRAzm\":1888484637,\"8vASsFjUra\":1150766839,\"FfJ4pMtRgG\":-649793668,\"bAgykme23P\":"
    "463906157,\"c2qBxL3ICD\":1992749811,\"iPEjHNxVvV\":568722464,\"jLaGD3PeZr\":-2091529540,\"nOVQARMGXD\":255034420,"
    "\"yFZe64IQEP\":244594031,\"yekiItpzg5\":-842402979},\"f4\":\"jVHLt45eFM\",\"f5\":{\"6jE50OeStZ\":920024288,"
    "\"K2csgMO2NP\":1755704426,\"NJLK0AslsI\":-231661239,\"QvwVBeOpmN\":1051724781,\"TIdgQ0tsfv\":-826979537,"
    "\"Y4ldK57UvG\":-204204775,\"cHHjxYEEK4\":722593805,\"hG9EmAhXaH\":-449059719,\"vjJDx5yX6X\":1609913570,"
    "\"vuJ8Yg4vAJ\":1517275424},\"f6\":{\"FmJZobHIES\":-1439865829,\"H2lwE9mFqn\":1219688623,\"MXXUhCmxBu\":746611301,"
    "\"NgT1AX2iWj\":1335440501,\"O9s7iYTPPH\":-1487465496,\"aKwrlH8avy\":1900707154,\"eCHbRzV8JM\":-1812512750,"
    "\"h6pYx5JPrn\":661783420,\"tbNduHmFD4\":1967314736,\"yGU6rzjEvC\":1046034272},\"f7\":-867344323,\"f8\":[815291316,"
    "1236592867,-1706450195,-1621455146,460068460,-2021934667,415320210,-78385022,-651971040,-1867926428],\"f9\":{"
    "\"BAgaZpMO7Z\":-639139778,\"CDipJlr6Jb\":-1103233968,\"L8wuRI49zY\":-171577919,\"LUt44FonDB\":1966172968,"
    "\"YDJ6VvU2cr\":1985784144,\"ZlRsizXkFB\":-509500295,\"auChuioeWK\":1553277670,\"iTAKYpd1FH\":-1305279957,"
    "\"rT8iERYy5s\":-953605172,\"wYYcn811hW\":-2086840136},\"f10\":false,\"f11\":{\"33fXAdu2eN\":-1536601824,"
    "\"5oVA7rSSd1\":-1798840477,\"JUdPvGUXel\":-1767430220,\"UXQfq7I3ox\":-1313674462,\"X3xFqK04sd\":-587848236,"
    "\"XWK6dHTkBa\":1184730261,\"bvbboSz5pt\":-585516128,\"eANal1UySD\":1730546921,\"lMx2GfnWiC\":1035049197,"
    "\"rotwrW89qB\":1275277449}},\"m\":{\"f0\":140439030,\"f1\":[1199381575,-1709165624,1643080124,2051504241,"
    "850971654,-1120497398,-2055977578,29986805,1846031760,1158428028],\"f2\":-2115305793,\"f3\":true,\"f4\":false,"
    "\"f5\":[1576945102,708190293,507199113,-254500786,970632424,-840444616,-1008644274,-681576860,-747807370,-"
    "1918306817],\"f6\":{\"BojNEg00qJ\":1403265386,\"OfekZH9pNB\":1089822531,\"VHXxUBUuXU\":-913068606,\"YEpvngrpJs\":-"
    "1183367918,\"as7Ax6SEIX\":1559154635,\"bKPCwN6AIL\":-1887949359,\"hDDHfLp9x9\":-1370341169,\"hYToGFKAkE\":"
    "495204614,\"subc1okvYm\":225642239,\"vqW9n4Sk3L\":712394709},\"f7\":{\"4d8RhSQyqP\":1556608812,\"4vWfkbANQS\":"
    "1503370470,\"X71sDUMXFv\":-579118050,\"XieStuGYOt\":-1154133283,\"Y9igBA8qym\":6568621,\"i0E4LrX8Cm\":756766536,"
    "\"n4kyI2HuqU\":-1725376334,\"vhtMs2awO9\":797391705,\"w78Td0pBWq\":109537650,\"ylxcNq4Pl1\":1460483273},\"f8\":"
    "\"i8nWdYmiCB\",\"f9\":\"sQulQx4Avx\",\"f10\":9.392091297064629e299,\"f11\":false,\"f12\":[1721456034,-1976015974,"
    "407613471,2015417178,20930173,662586649,405945162,-1848589438,-1375729264,-271357978]},\"n\":{\"f0\":{"
    "\"6NlYuVe4f5\":970298841,\"7qRwn549Xm\":-1766510931,\"9eqZW4rgTw\":1264907124,\"CjY79XQRW0\":150374998,"
    "\"EhqrDSr5HR\":-414746096,\"FGeQiL2SiI\":642110767,\"IdNSCDUVKw\":-2048376340,\"MTBFZZiDjy\":-1442913617,"
    "\"UpZoVhM9o7\":-40737161,\"ian2ToeJIE\":-747737622},\"f1\":\"g50uZortu9\",\"f2\":4.54110037257389e299,\"f3\":"
    "false,\"f4\":\"6b44nynBmv\",\"f5\":718357576,\"f6\":{\"9xKuAOYYRn\":-1107378642,\"BeN8X0CtTG\":-1751965913,"
    "\"HvmTiWCIUo\":1117896630,\"O5Z14TsoSP\":1301191147,\"V1lg6MVQyj\":1439032196,\"VvyTZOv1B2\":-1527441841,"
    "\"ZHUnyF5Zlb\":8636296,\"hJWS9hxDDx\":-1420050572,\"juGKZoiG4T\":937799573,\"yVMJ9bLijD\":-1254646052},\"f7\":"
    "true,\"f8\":false,\"f9\":\"IzzDPAec89\",\"f10\":-1263498881,\"f11\":{\"2WeWTLBF30\":722017044,\"8zfNNmb5VR\":-"
    "1808517035,\"GODiah1Fll\":1975822081,\"HMk57D4rcl\":-1524910675,\"TKklV8HWxG\":109529602,\"V507KqCmoZ\":-"
    "1281225634,\"WxoJlOYg1I\":-1146577958,\"go5jTIHV51\":-569833480,\"t9N8cyLC6U\":-1640272944,\"y9d4LKJhCj\":-"
    "1602762881},\"f12\":true,\"f13\":[1246824659,274367630,-955892856,-2129439102,853533940,-1015389432,-2032437625,-"
    "1875700883,196584421,-1943568121]},\"o\":{\"f0\":{\"Af3U2IYXak\":864338892,\"EQGDxgCfPv\":-1370618413,"
    "\"HitpSFsgnr\":1880311753,\"HmtUp43zkF\":240848983,\"MicQV8XQqZ\":-295454715,\"XjWoCpnVde\":1376765762,"
    "\"bbC0D9xhPm\":-1440192146,\"oe9mmor8Xf\":-1507268335,\"p7hv2YMJsN\":1301565406,\"pWh1b2JmTw\":1380747324},\"f1\":"
    "false,\"f2\":-1707136232,\"f3\":true,\"f4\":false,\"f5\":{\"8OpIhKYs9H\":357839062,\"KeNCIrQIAs\":1411146404,"
    "\"O2vHEnQCMc\":-330619818,\"ZheP3Hs1DE\":-1899070037,\"fKT1xmK2hM\":-640862379,\"gDz9ll4qH3\":1708849796,"
    "\"k2W3eTF67c\":141294468,\"pjcoPIU5im\":1538200631,\"xJ1iYm10im\":-517615819,\"yBhlnCdpc1\":-68955985},\"f6\":-"
    "112302195,\"f7\":1095664676,\"f8\":973871639,\"f9\":-8.553138637753277e299,\"f10\":3.7528446099373706e299,\"f11\":"
    "2102526383,\"f12\":true,\"f13\":{\"4KuuPHmNWn\":-1180781329,\"5adbTftA6g\":-1470022815,\"7JrEyS4fVa\":-41779304,"
    "\"BXoCLwO7OK\":-650902479,\"GtU3pj00P3\":658514161,\"UlFj8gUCVC\":1257731842,\"fcFe0l5fWI\":2079375942,"
    "\"hcDnOotHEP\":-593319917,\"m92NTyik6S\":80834182,\"yJ9LnBKj6B\":1743020529},\"f14\":false},\"p\":{\"f0\":"
    "\"8UqK152Ly4\",\"f1\":-5.520162846990078e299,\"f2\":{\"0W163nqXSm\":1255329318,\"79uD0oM1CI\":1401799066,"
    "\"LQxhkhr81G\":-708650655,\"N3JyDEraLw\":400286892,\"Rx9IHmJI7j\":1120585797,\"aCZgnf3T9D\":460110730,"
    "\"hAx0nz3VJ1\":809980861,\"hCs6XNUa7q\":-1581032190,\"n5K1EnWzKe\":-95448647,\"rYUgaxUBnR\":1256709512},\"f3\":"
    "false,\"f4\":[-903702834,752148286,1509793320,-44826494,974399094,188482699,-1732512977,-218360303,-2095762147,"
    "634026564],\"f5\":-4.304484157904056e299,\"f6\":5.1221361626200565e299,\"f7\":-1209383336,\"f8\":\"C1DKTGqtG4\","
    "\"f9\":[-1541546472,1278278994,-534906865,1649176852,1908457236,2006001032,-794317636,1941787394,1741192235,"
    "1999795746],\"f10\":\"pDywdKrZ50\",\"f11\":[-1957484690,-284939977,-183242407,-133655566,-1730983454,-1246899606,"
    "68073355,-388884874,457207768,-1290414593],\"f12\":[1789916530,-2034712133,-1861657011,218123378,469577906,"
    "1231522570,-799226635,1671554726,175406541,1805380860],\"f13\":1036926430,\"f14\":false,\"f15\":{\"9FrnmFL8Cu\":"
    "1335604512,\"B3nJw87DOc\":67012226,\"M5GFd9PcSa\":1586614656,\"PLKLyuUN8U\":2082902142,\"UEFKtwbLcZ\":-191065107,"
    "\"X5yykhTJrg\":-967871719,\"olitokZ3cO\":79035524,\"q1mkt3P9oA\":-361657629,\"qAIu7jHsGn\":1940888234,"
    "\"zF7BpRnIM4\":1361145486}},\"q\":{\"f0\":{\"D3UZdhcvlM\":1169837736,\"N1Li4HpDTS\":2132394065,\"Opra8jymmW\":-"
    "959690510,\"VYtYSOD7S6\":-1561920636,\"bQ2gQrU0vq\":-2125737814,\"eFuUbz9OBn\":1800133404,\"lXo9ItaInE\":-"
    "2090146326,\"leLyKPCC4z\":1367551459,\"r2Ls9TIX8m\":1914649448,\"xepgMQELjo\":1044829346},\"f1\":false,\"f2\":{"
    "\"4kASKvrqMV\":-517549963,\"5EJzTmm4lr\":-425489774,\"8swpBB6UcG\":1909576215,\"ACAs2gVha2\":943832429,"
    "\"EYy5ha0bX5\":-746036618,\"VIeqnu3Xhv\":1526852568,\"aClXzQbZdH\":-833808678,\"c6dMVg1HfZ\":917657674,"
    "\"fMTXJxdrAn\":-1472944801,\"fmYV5p0MX1\":-473290114},\"f3\":\"1gvup75qGE\",\"f4\":\"8JdUKx3j9f\",\"f5\":false,"
    "\"f6\":true,\"f7\":true,\"f8\":{\"34P1pnFMUm\":1158877275,\"8p4jeExHxR\":1109078617,\"Ikq5YjD2lK\":668827682,"
    "\"LRi0Ixccv5\":-1685591447,\"OaACzP0YaD\":-1094213641,\"QOSwLAOBJs\":313829593,\"QRF69fnlG0\":1748783459,"
    "\"Xjqi9xFMOO\":1328455667,\"bLEtyizut0\":-220158612,\"cuS7dD3wpp\":-1155677953},\"f9\":5.912785266045141e299,"
    "\"f10\":[-1049446885,1980941246,641461462,561134667,-1189007154,-1558716156,1926262569,-1250352099,-734054160,-"
    "1013161577],\"f11\":-5.44111085112734e299,\"f12\":5.1479877490965724e299,\"f13\":true,\"f14\":{\"6Rhl5wbMDs\":-"
    "1640439469,\"Ce0yE9UXkz\":1493594178,\"DqavJAPLXK\":1897656735,\"IOnzlg7fBU\":250181063,\"JMrBMULBbb\":-"
    "1255449589,\"N1pCY8ZjKS\":-383193394,\"XbptjVMorS\":-1789206648,\"g59232WPuO\":1099368033,\"hXuw15mzzu\":-"
    "1486099392,\"mMfB3wROIC\":-2006659003},\"f15\":{\"56f0Vrb9d3\":-121556205,\"CQnp8NIxwz\":-1513856866,"
    "\"CxZqIhOWi9\":1000977665,\"IXbZnbUijV\":-1434278203,\"X6S8rokCxx\":-1525548417,\"Yr2XER2WlI\":1436637656,"
    "\"hd7Ic4IrlS\":62753913,\"k0W1fTSXeo\":-164643549,\"p4uVaiCMO9\":-283726970,\"piFfZxPEzK\":-1232377789},\"f16\":"
    "\"ZI6TSl14e9\"},\"r\":{\"f0\":\"gK5sWsKxeE\",\"f1\":\"Uo2QKcC5tC\",\"f2\":false,\"f3\":\"jlrxrCCt5I\",\"f4\":"
    "false,\"f5\":\"nTYgFG8hWK\",\"f6\":\"Vs0uAxENzS\",\"f7\":{\"HPUJ8E2rJL\":1624257222,\"JyMEaSFYka\":-205734349,"
    "\"Ks90xSHkom\":-2048872134,\"KyBJdq2TDX\":-1184082975,\"fUlOnZARqf\":708047835,\"fhP57Iq1O4\":1150867519,"
    "\"iewJarqEfC\":2071913611,\"pcnPwlvgbu\":1168571919,\"pfDmL0ebHX\":-167288432,\"wL9FLP9o44\":1363348093},\"f8\":"
    "true,\"f9\":{\"C8EoxPCSkF\":1371311110,\"Q3Nt9plBhu\":-1399829992,\"XeOCcejanH\":-597600995,\"jLeaE0iiDS\":"
    "311367716,\"kqGO3qQU0d\":-1276405907,\"mp8cQutjBE\":1329987099,\"pLBB88dAZN\":969034295,\"rsoCcInomL\":985522758,"
    "\"ugX8mp3x3g\":-1093930245,\"xCJgiLUxXP\":272158188},\"f10\":{\"0MEDUMvlqZ\":382469641,\"8BqpKBCMAh\":936303159,"
    "\"DBfhBaaIHI\":-85010246,\"DeG97IVkZD\":1490896588,\"GCD5H4mLNz\":-2012107728,\"XdXthoCF4R\":-1366218033,"
    "\"aDOR5Nj3iJ\":1878911007,\"jReGfsd3og\":1628726362,\"lQqxHDWiXX\":-1955720766,\"ogAa2642kC\":1361088633},\"f11\":"
    "[-1953579093,-1990230928,81210858,-1568972053,-1096135825,500446266,1179666118,1265138686,139583518,1272784009],"
    "\"f12\":\"680lHapAjx\",\"f13\":{\"6ncYUDHZsn\":-688271268,\"BX77m4WbiZ\":555431258,\"JvZIJvBBjE\":1812880503,"
    "\"WWQ16v2NHE\":-1546143464,\"ZfA5tqO1mS\":901619184,\"ZwY1e2AjGh\":-771986385,\"oKd80gqHeY\":1126509675,"
    "\"tZKJrahAxj\":-327702319,\"xsHW1UtCQw\":-1094358881,\"ziWUMlEJ5a\":-41935483},\"f14\":{\"2beUroJ44E\":1882392933,"
    "\"BfFYB6ASNj\":-1978961530,\"G5wPv0LgCC\":1531258274,\"HkRczraUeI\":951676732,\"I01u7P4oBi\":606556761,"
    "\"MwuchfpXO4\":1112098416,\"WZ0UNGrQNQ\":-1105590133,\"cJUFckJsJZ\":104638068,\"tIMQxpbbFP\":434140882,"
    "\"wcQPP8s3et\":-1296556703},\"f15\":[-1730540083,1010799014,185997887,586817764,1396283456,-1757370030,-825449466,"
    "1488124742,1969021455,-783077939],\"f16\":-1136692906,\"f17\":{\"0vK2mS3sGi\":952465581,\"1fneXeN5ZI\":485405504,"
    "\"2QG8XZmBiQ\":-144032262,\"CCjjrtyL9F\":-1838820520,\"OOOrUgsVck\":1690914018,\"PGnBtt9sWE\":-877096830,"
    "\"XoAc98BqQi\":501405147,\"ePvI0pKSzw\":-519052323,\"mdfrz9aE9s\":1985178003,\"yxGYJFZ5ed\":1516615749}},\"s\":{"
    "\"f0\":598075644,\"f1\":false,\"f2\":-1.9262207448980825e299,\"f3\":[1758433030,187191160,-541943362,527529635,"
    "846650413,1135127439,1601474229,-670554514,-215207722,-1186499333],\"f4\":{\"1ml0KWgOVz\":81043610,\"8ukVaXJ2ji\":"
    "-703561816,\"drJ1onP931\":-1125762802,\"fWD8pS0hVB\":1817610784,\"gJ2LSkdzs8\":1102567764,\"h0WD4L3Ujz\":"
    "1871623417,\"i4azQTgHNy\":1896919646,\"k0Icmt5MFU\":-1762190592,\"ow6CcKRH08\":3687983,\"ummZThsP5j\":-938556413},"
    "\"f5\":{\"81MBYzAjlp\":-343327722,\"Rre2D18Inv\":-1478672815,\"TZEKUnyc5i\":237594798,\"d9YvqnKNES\":493699908,"
    "\"df2EWorffk\":355855309,\"oMPErm2m97\":-1782928231,\"p2gCc4htBy\":1895567497,\"rTL7K3dmVx\":-38035850,"
    "\"uuXgkXHapG\":-1207790187,\"wmOIYcTpb3\":93772844},\"f6\":-4.547741891541037e298,\"f7\":1252813630,\"f8\":"
    "1558974552,\"f9\":true,\"f10\":true,\"f11\":-6.026662587681791e299,\"f12\":1.212091766547016e299,\"f13\":["
    "1972136442,1669211356,243599697,-534725913,-2141442556,-1544881618,-253776146,-614679911,-982103703,-1489310303],"
    "\"f14\":[-377053494,1624270114,1541788567,-519832428,-125356216,572733296,1834988006,-1701416845,-773171798,-"
    "1441417850],\"f15\":-6.1755398757925176e299,\"f16\":-2113865031,\"f17\":\"MvBuFwDNjf\",\"f18\":true},\"t\":{"
    "\"f0\":true,\"f1\":false,\"f2\":{\"0A7wgjq3iH\":1676105526,\"2yALraYz9E\":540430261,\"3KCVFNIh2z\":-943099472,"
    "\"DeUrDNGT5U\":925196340,\"HUamHDSsZi\":1015354322,\"PFbTyAVFdX\":-1699892411,\"VuWKSuiThy\":-396884201,"
    "\"la1SE6DRlu\":368994906,\"rewjcHiC5R\":829651907,\"s8DMInSY0a\":-359559296},\"f3\":-1217186063,\"f4\":true,"
    "\"f5\":\"vHH0lV7PdL\",\"f6\":-8.89007510263416e299,\"f7\":\"iSW1MvDDhx\",\"f8\":true,\"f9\":[1434168112,"
    "1394394923,-329658532,2117281689,491118759,1934338126,-1907530676,-1330636265,-1958621240,873869169],\"f10\":{"
    "\"4U1tLopkk6\":1175823387,\"5mquViNCSx\":-1876874064,\"JcinROPUHo\":380649438,\"MRRNBPJotQ\":-1256569051,"
    "\"Ovxt8bqTEg\":-2089712334,\"SCgRoYSarc\":-1101501130,\"YEBFkYx7XX\":1681573733,\"cMJLoJbSjT\":1346257146,"
    "\"p75hqCJ7Vi\":-1135763142,\"xedsAhkLLS\":1992665467},\"f11\":1846576993,\"f12\":-1.2823210943653017e299,\"f13\":"
    "5.042635046449982e299,\"f14\":false,\"f15\":-536594644,\"f16\":[-515425193,404299928,1249495646,2089501676,-"
    "1430204695,-1997988037,-1297755677,1042958908,-1657888221,-1436946684],\"f17\":-1144695556,\"f18\":-1674200310,"
    "\"f19\":-1291818235}}";

TEST(RandomProtoTest, StructTest) {
    ProtoFactory factory;
    int offset = NEKO_RESERVED_PROTO_TYPE_SIZE;
    EXPECT_EQ(factory.proto_type<TestStruct1>(), 1 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct10>(), 2 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct11>(), 3 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct12>(), 4 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct13>(), 5 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct14>(), 6 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct15>(), 7 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct16>(), 8 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct17>(), 9 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct18>(), 10 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct19>(), 11 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct2>(), 12 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct20>(), 13 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct3>(), 14 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct4>(), 15 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct5>(), 16 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct6>(), 17 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct7>(), 18 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct8>(), 19 + offset);
    EXPECT_EQ(factory.proto_type<TestStruct9>(), 20 + offset);
    EXPECT_EQ(factory.proto_type<TestStructProto>(), 21 + offset);

    TestStructProto proto;
    proto.b.f0 = {4};
    NEKO_LOG_INFO("unit test", "{}", SerializableToString(proto));
    auto data = proto.makeProto().toData();
    data.push_back('\0');
    EXPECT_STREQ(data.data(), StructProtoData);

    TestStructProto proto2;
    proto2.makeProto().formData(StructProtoData, 18184);
    EXPECT_EQ(proto2.a.f0, proto.a.f0);

    TestStructProto::ProtoType proto3;
    proto3          = proto;
    (*proto3).r.f16 = 100;
}

enum TestEnumBG8AC {
    TestEnumBG8AC0,
    TestEnumBG8AC1,
    TestEnumBG8AC2,
    TestEnumBG8AC3,
    TestEnumBG8AC4,
    TestEnumBG8AC5,
    TestEnumBG8AC6,
    TestEnumBG8AC7,
    TestEnumBG8AC8,
    TestEnumBG8AC9,
    TestEnumBG8AC10,
    TestEnumBG8AC11,
    TestEnumBG8AC12,
    TestEnumBG8AC13,
    TestEnumBG8AC14,
    TestEnumBG8AC15,
    TestEnumBG8AC16,
    TestEnumBG8AC17,
    TestEnumBG8AC18,
    TestEnumBG8AC19,
    TestEnumBG8AC20,
    TestEnumBG8AC21,
    TestEnumBG8AC22,
    TestEnumBG8AC23,
    TestEnumBG8AC24,
    TestEnumBG8AC25,
    TestEnumBG8AC26,
    TestEnumBG8AC27,
    TestEnumBG8AC28,
    TestEnumBG8AC29,
    TestEnumBG8AC30,
    TestEnumBG8AC31,
    TestEnumBG8AC32,
    TestEnumBG8AC33,
    TestEnumBG8AC34,
    TestEnumBG8AC35,
    TestEnumBG8AC36,
    TestEnumBG8AC37,
    TestEnumBG8AC38,
    TestEnumBG8AC39,
    TestEnumBG8AC40,
    TestEnumBG8AC41,
    TestEnumBG8AC42,
    TestEnumBG8AC43,
    TestEnumBG8AC44,
    TestEnumBG8AC45,
    TestEnumBG8AC46,
    TestEnumBG8AC47,
    TestEnumBG8AC48,
    TestEnumBG8AC49,
    TestEnumBG8AC50,
    TestEnumBG8AC51,
    TestEnumBG8AC52,
    TestEnumBG8AC53,
    TestEnumBG8AC54,
    TestEnumBG8AC55,
    TestEnumBG8AC56,
    TestEnumBG8AC57,
    TestEnumBG8AC58,
    TestEnumBG8AC59,
    TestEnumBG8AC60,
    TestEnumBG8AC61
};

TEST(RandomProtoTest, EnumTest) {
    TestEnumBG8AC a = TestEnumBG8AC::TestEnumBG8AC0;
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer out(buffer);
    out.startObject(-1);
    out(makeNameValuePair<const TestEnumBG8AC&>("a", a));
    out(makeNameValuePair<const TestEnumBG8AC&>("b", TestEnumBG8AC::TestEnumBG8AC10));
    out(makeNameValuePair<const TestEnumBG8AC&>("c", TestEnumBG8AC::TestEnumBG8AC20));
    out(makeNameValuePair<const TestEnumBG8AC&>("d", TestEnumBG8AC::TestEnumBG8AC30));
    out(makeNameValuePair<const TestEnumBG8AC&>("e", TestEnumBG8AC::TestEnumBG8AC40));
    out(makeNameValuePair<const TestEnumBG8AC&>("f", TestEnumBG8AC::TestEnumBG8AC50));
    out(makeNameValuePair<const TestEnumBG8AC&>("g", TestEnumBG8AC::TestEnumBG8AC60));
    out(makeNameValuePair<const TestEnumBG8AC&>("h", TestEnumBG8AC::TestEnumBG8AC61));
    out.end();
    buffer.push_back('\0');
    const char* str = buffer.data();
#if __cplusplus >= 201703L || _MSVC_LANG > 201402L
    EXPECT_STREQ(str,
                 "{\"a\":\"TestEnumBG8AC0(0)\",\"b\":\"TestEnumBG8AC10(10)\",\"c\":\"TestEnumBG8AC20(20)\",\"d\":"
                 "\"TestEnumBG8AC30(30)\",\"e\":\"TestEnumBG8AC40(40)\",\"f\":\"TestEnumBG8AC50(50)\",\"g\":\"(60)\","
                 "\"h\":\"(61)\"}");
#else
    EXPECT_STREQ(str, "{\"a\":0,\"b\":10,\"c\":20,\"d\":30,\"e\":40,\"f\":50,\"g\":60,\"h\":61}");
#endif
}

int main(int argc, char** argv) {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}