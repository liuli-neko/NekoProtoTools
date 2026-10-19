#include <nekoproto/reflect.hpp>
#include <cstdint>

enum class EmptyEnum {};
enum class Huge : std::uint64_t { High = 1ULL << 63, Max = UINT64_MAX };
static_assert(nekoproto::Reflect<EmptyEnum>::value_count == 0);
static_assert(nekoproto::Reflect<Huge>::values()[1] == Huge::Max);

enum class Sparse {
    Big [[=nekoproto::comment_tag<"large">]] = 10000,
    Alias = 10000,
    Negative = -1000,
    Zero = 0
};
enum class ExplicitEnum { First, Second };
template <> struct nekoproto::Meta<ExplicitEnum> {
    static constexpr auto value = nekoproto::Enumerate("custom", ExplicitEnum::Second);
};
static_assert(nekoproto::Reflect<Sparse>::value_count == 4);
static_assert(nekoproto::Reflect<Sparse>::names()[0] == "Big");
static_assert(nekoproto::Reflect<Sparse>::name(Sparse::Alias) == "Big");
static_assert(nekoproto::Reflect<Sparse>::value("Alias") == Sparse::Big);
static_assert(nekoproto::Reflect<Sparse>::values()[2] == Sparse::Negative);
static_assert(nekoproto::tag_query::get<nekoproto::tag_property::Comment>(std::get<0>(nekoproto::Reflect<Sparse>::field_tags)) == "large");
static_assert(nekoproto::Reflect<ExplicitEnum>::value_count == 1);
static_assert(nekoproto::Reflect<ExplicitEnum>::names()[0] == "custom");
int main() {
    int count = 0;
    nekoproto::Reflect<Sparse>::visitMeta([&](auto, auto, const auto&) { ++count; });
    return count == 4 ? 0 : 1;
}
