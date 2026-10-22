/*
    Sipflix: Dispatcharr (Xtream) live TV tab
*/

#pragma once

#include <borealis.hpp>
#include "api/xtream.hpp"

class RecyclingGrid;

class IptvTab : public brls::Box {
public:
    IptvTab();
    ~IptvTab() override;

    static brls::View* create();

    brls::View* getDefaultFocus() override;

    void loadChannels(const xtream::Category& category);

private:
    BRLS_BIND(RecyclingGrid, categories, "iptv/categories");
    BRLS_BIND(RecyclingGrid, channels, "iptv/channels");
    BRLS_BIND(brls::Header, channelsTitle, "iptv/channels/title");

    xtream::Account account;
    void doRequest();
};
