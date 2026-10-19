import nekoproto.reflect;
struct PureImport { int value; };
using PureTags = nekoproto::TagList<nekoproto::rename_tag<"pure">>;
int main() {
    PureImport object{1};
    auto [tag] = PureTags{};
    (void)tag;
    nekoproto::Reflect<PureImport>::forEach(object, [](int& v) { ++v; });
    return nekoproto::Reflect<PureImport>::value<0>(object) == 2 ? 0 : 1;
}
