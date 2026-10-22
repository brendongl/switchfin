/*
    Sipflix: minimal Xtream Codes client for Dispatcharr live TV
*/

#include "api/xtream.hpp"
#include "api/http.hpp"
#include "utils/config.hpp"
#include <borealis.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

namespace xtream {

static std::string str(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) return "";
    const auto& v = j.at(key);
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    return "";
}

static std::string base64Decode(const std::string& in) {
    static const std::string tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        auto pos = tbl.find(c);
        if (pos == std::string::npos) break;
        val = (val << 6) + (int)pos;
        bits += 6;
        if (bits >= 0) {
            out.push_back(char((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

Account Account::load() {
    Account acc;
    try {
        std::ifstream f(AppConfig::instance().configDir() + "/iptv.json");
        if (!f.is_open()) return acc;
        auto j = nlohmann::json::parse(f);
        acc.server = str(j, "server");
        acc.user = str(j, "username");
        acc.pass = str(j, "password");
        acc.startCategory = str(j, "start_category");
        while (!acc.server.empty() && acc.server.back() == '/') acc.server.pop_back();
    } catch (const std::exception& ex) {
        brls::Logger::error("xtream: bad iptv.json: {}", ex.what());
    }
    return acc;
}

std::string Account::api(const std::string& action, const std::string& extra) const {
    return fmt::format("{}/player_api.php?username={}&password={}&action={}{}", server, user, pass, action, extra);
}

std::string Account::liveUrl(const std::string& streamId) const {
    return fmt::format("{}/live/{}/{}/{}.ts", server, user, pass, streamId);
}

template <typename Result>
static void fetch(const std::string& url, const std::function<Result(const nlohmann::json&)>& parse,
    std::function<void(Result)> then, OnError error) {
    brls::async([url, parse, then, error]() {
        try {
            auto resp = HTTP::get(url, HTTP::Timeout{});
            if (resp.empty()) throw std::runtime_error("empty response");
            auto j = nlohmann::json::parse(resp);
            if (j.is_object() && j.contains("error")) throw std::runtime_error(str(j, "error"));
            Result r = parse(j);
            brls::sync([then, r = std::move(r)]() mutable { then(std::move(r)); });
        } catch (const std::exception& ex) {
            if (error) brls::sync(std::bind(error, std::string(ex.what())));
        }
    });
}

void getCategories(const Account& acc, std::function<void(std::vector<Category>)> then, OnError error) {
    fetch<std::vector<Category>>(
        acc.api("get_live_categories"),
        [](const nlohmann::json& j) {
            std::vector<Category> list;
            for (auto& it : j) list.push_back({str(it, "category_id"), str(it, "category_name")});
            return list;
        },
        then, error);
}

void getChannels(
    const Account& acc, const std::string& categoryId, std::function<void(std::vector<Channel>)> then, OnError error) {
    fetch<std::vector<Channel>>(
        acc.api("get_live_streams", "&category_id=" + categoryId),
        [](const nlohmann::json& j) {
            std::vector<Channel> list;
            for (auto& it : j) {
                Channel c;
                c.id = str(it, "stream_id");
                c.name = str(it, "name");
                c.icon = str(it, "stream_icon");
                try { c.num = std::stoi(str(it, "num")); } catch (...) {}
                list.push_back(std::move(c));
            }
            return list;
        },
        then, error);
}

void getShortEpg(const Account& acc, const std::string& streamId, std::function<void(std::vector<Program>)> then) {
    fetch<std::vector<Program>>(
        acc.api("get_short_epg", "&limit=2&stream_id=" + streamId),
        [](const nlohmann::json& j) {
            std::vector<Program> list;
            if (!j.contains("epg_listings")) return list;
            for (auto& it : j.at("epg_listings")) {
                Program p;
                p.title = base64Decode(str(it, "title"));
                p.desc = base64Decode(str(it, "description"));
                try { p.start = (time_t)std::stoll(str(it, "start_timestamp")); } catch (...) {}
                try { p.end = (time_t)std::stoll(str(it, "stop_timestamp")); } catch (...) {}
                list.push_back(std::move(p));
            }
            return list;
        },
        then, nullptr);
}

}  // namespace xtream
