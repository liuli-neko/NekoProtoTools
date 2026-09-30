#pragma once

#include "benchmark_models.hpp"
#include <nekoproto/serialization/private/helpers.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <vector>

struct BenchmarkAdapter {
    using Buffer = std::vector<char>;
    static constexpr const char* library = "RapidJSON-Raw";
    static constexpr const char* backend = "Streaming";

    static auto encode(const User& user, Buffer& bytes) -> bool {
        bytes.clear();
        nekoproto::detail::OutBufferWrapper stream(bytes);
        rapidjson::Writer<nekoproto::detail::OutBufferWrapper> writer(stream);

        writer.StartObject();
        writer.Key("id", 2, false);
        writer.Uint64(user.id);
        writer.Key("name", 4, false);
        writer.String(user.name.data(), static_cast<rapidjson::SizeType>(user.name.size()), false);
        writer.Key("scores", 6, false);
        writer.StartArray();
        for (double s : user.scores) {
            writer.Double(s);
        }
        writer.EndArray();
        if (user.email) {
            writer.Key("email", 5, false);
            writer.String(user.email->data(), static_cast<rapidjson::SizeType>(user.email->size()), false);
        }
        writer.Key("address", 7, false);
        writer.StartObject();
        writer.Key("city", 4, false);
        writer.String(user.address.city.data(), static_cast<rapidjson::SizeType>(user.address.city.size()), false);
        writer.Key("street", 6, false);
        writer.String(user.address.street.data(), static_cast<rapidjson::SizeType>(user.address.street.size()), false);
        writer.Key("zip", 3, false);
        writer.Int(user.address.zip);
        writer.EndObject();
        writer.EndObject();

        writer.Flush();
        return true;
    }

    static auto decode(const Buffer& bytes, User& user) -> bool {
        rapidjson::Document doc;
        doc.Parse(bytes.data(), bytes.size());
        if (doc.HasParseError() || !doc.IsObject()) return false;

        auto id_it = doc.FindMember("id");
        if (id_it == doc.MemberEnd() || !id_it->value.IsUint64()) return false;
        user.id = id_it->value.GetUint64();

        auto name_it = doc.FindMember("name");
        if (name_it == doc.MemberEnd() || !name_it->value.IsString()) return false;
        user.name.assign(name_it->value.GetString(), name_it->value.GetStringLength());

        auto scores_it = doc.FindMember("scores");
        if (scores_it == doc.MemberEnd() || !scores_it->value.IsArray()) return false;
        const auto& arr = scores_it->value.GetArray();
        user.scores.clear();
        user.scores.reserve(arr.Size());
        for (const auto& item : arr) {
            if (!item.IsNumber()) return false;
            user.scores.push_back(item.GetDouble());
        }

        auto email_it = doc.FindMember("email");
        if (email_it != doc.MemberEnd() && email_it->value.IsString()) {
            user.email = std::string(email_it->value.GetString(), email_it->value.GetStringLength());
        } else {
            user.email.reset();
        }

        auto addr_it = doc.FindMember("address");
        if (addr_it == doc.MemberEnd() || !addr_it->value.IsObject()) return false;
        const auto& addr_obj = addr_it->value.GetObject();

        auto city_it = addr_obj.FindMember("city");
        if (city_it == addr_obj.MemberEnd() || !city_it->value.IsString()) return false;
        user.address.city.assign(city_it->value.GetString(), city_it->value.GetStringLength());

        auto street_it = addr_obj.FindMember("street");
        if (street_it == addr_obj.MemberEnd() || !street_it->value.IsString()) return false;
        user.address.street.assign(street_it->value.GetString(), street_it->value.GetStringLength());

        auto zip_it = addr_obj.FindMember("zip");
        if (zip_it == addr_obj.MemberEnd() || !zip_it->value.IsInt()) return false;
        user.address.zip = zip_it->value.GetInt();

        return true;
    }
};
