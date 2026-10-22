/*
    Sipflix: minimal Xtream Codes client for Dispatcharr live TV
*/

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <ctime>

namespace xtream {

struct Account {
    std::string server;  // https://tvserver.sipflix.net
    std::string user;
    std::string pass;
    std::string startCategory;  // optional substring of the category to open first

    bool valid() const { return !server.empty() && !user.empty() && !pass.empty(); }
    /// reads <configDir>/iptv.json: {"server":..., "username":..., "password":...}
    static Account load();

    std::string api(const std::string& action, const std::string& extra = "") const;
    std::string liveUrl(const std::string& streamId) const;
};

struct Category {
    std::string id;
    std::string name;
};

struct Channel {
    std::string id;
    std::string name;
    std::string icon;
    int num = 0;
};

struct Program {
    std::string title;
    std::string desc;
    time_t start = 0;
    time_t end = 0;
    bool valid() const { return !title.empty(); }
};

using OnError = std::function<void(const std::string&)>;

void getCategories(const Account& acc, std::function<void(std::vector<Category>)> then, OnError error);
void getChannels(const Account& acc, const std::string& categoryId, std::function<void(std::vector<Channel>)> then,
    OnError error);
/// current + next program (empty vector when the channel has no guide data)
void getShortEpg(const Account& acc, const std::string& streamId, std::function<void(std::vector<Program>)> then);

}  // namespace xtream
