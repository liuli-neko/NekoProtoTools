#pragma once
struct Shared { int value; };
using SharedTags = nekoproto::TagList<nekoproto::rename_tag<"shared_value">>;
template <> struct nekoproto::Meta<Shared> {
    static constexpr auto value = nekoproto::Object("value", &Shared::value);
};
int header_increment(Shared&, SharedTags);
