/*
    Sipflix: Dispatcharr (Xtream) live TV tab
*/

#include "tab/iptv_tab.hpp"
#include "tab/remote_view.hpp"
#include "view/recycling_grid.hpp"
#include "utils/image.hpp"
#include "utils/config.hpp"
#include <fmt/format.h>
#include <fstream>
#include <map>
#include <set>

using namespace brls::literals;  // for _i18n

class CategoryCell : public RecyclingGridItem {
public:
    CategoryCell() { this->inflateFromXMLRes("xml/view/category_card.xml"); }
    static CategoryCell* create() { return new CategoryCell(); }

    BRLS_BIND(brls::Label, name, "iptv/category/name");
};

class ChannelCell : public RecyclingGridItem {
public:
    ChannelCell() { this->inflateFromXMLRes("xml/view/channel_card.xml"); }
    static ChannelCell* create() { return new ChannelCell(); }

    void prepareForReuse() override {
        this->logo->setImageFromRes("img/video-card-bg.png");
        this->now->setText("");
    }
    void cacheForReuse() override { Image::cancel(this->logo); }
    ~ChannelCell() override { Image::cancel(this->logo); }

    BRLS_BIND(brls::Image, logo, "iptv/channel/logo");
    BRLS_BIND(brls::Label, name, "iptv/channel/name");
    BRLS_BIND(brls::Label, now, "iptv/channel/now");
    BRLS_BIND(brls::Label, num, "iptv/channel/num");
};

class CategoryDataSource : public RecyclingGridDataSource {
public:
    CategoryDataSource(IptvTab* tab, std::vector<xtream::Category> list) : tab(tab), list(std::move(list)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        auto* cell = dynamic_cast<CategoryCell*>(recycler->dequeueReusableCell("Cell"));
        cell->name->setText(this->list.at(index).name);
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override { this->tab->loadChannels(this->list.at(index)); }

    void clearData() override { this->list.clear(); }

private:
    IptvTab* tab;
    std::vector<xtream::Category> list;
};

class ChannelDataSource : public RecyclingGridDataSource {
public:
    ChannelDataSource(const xtream::Account& acc, std::vector<xtream::Channel> list)
        : acc(acc), list(std::move(list)) {
        for (size_t i = 0; i < this->list.size(); i++) this->indexById[this->list[i].id] = i;
    }

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        auto* cell = dynamic_cast<ChannelCell*>(recycler->dequeueReusableCell("Cell"));
        auto& ch = this->list.at(index);
        cell->name->setText(ch.name);
        cell->num->setText(ch.num > 0 ? std::to_string(ch.num) : "");
        if (!ch.icon.empty()) Image::with(cell->logo, ch.icon);

        // lazily load "now" for visible rows
        auto it = epg.find(ch.id);
        if (it != epg.end()) {
            cell->now->setText(it->second);
        } else if (requested.insert(ch.id).second) {
            std::string id = ch.id;
            std::weak_ptr<bool> alive = this->alive;
            xtream::getShortEpg(this->acc, id, [this, alive, id](std::vector<xtream::Program> p) {
                if (alive.expired()) return;
                std::string text = p.empty() ? "" : p.front().title;
                if (p.size() > 1 && !p[1].title.empty()) text += "   >  " + p[1].title;
                this->epg[id] = text;
                // update the visible cell in place: reloadData() would reset focus
                auto idx = this->indexById.find(id);
                if (this->recycler && idx != this->indexById.end()) {
                    if (auto* item = dynamic_cast<ChannelCell*>(this->recycler->getGridItemByIndex(idx->second)))
                        item->now->setText(text);
                }
            });
        }
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        auto& ch = this->list.at(index);
        RemoteView::play(this->acc.liveUrl(ch.id), ch.name);
    }

    void clearData() override { this->list.clear(); }

    RecyclingGrid* recycler = nullptr;

private:
    xtream::Account acc;
    std::vector<xtream::Channel> list;
    std::map<std::string, std::string> epg;
    std::set<std::string> requested;
    std::map<std::string, size_t> indexById;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

brls::View* IptvTab::create() {
    // a broken Live TV tab must never take the whole app down
    try {
        return new IptvTab();
    } catch (const std::exception& ex) {
        brls::Logger::error("IptvTab: {}", ex.what());
        std::ofstream f(AppConfig::instance().configDir() + "/iptv-error.txt");
        f << ex.what() << "\n";
        auto* box = new brls::Box();
        auto* label = new brls::Label();
        label->setText(std::string("Live TV: ") + ex.what());
        label->setFontSize(18);
        box->addView(label);
        return box;
    }
}

IptvTab::IptvTab() {
    this->inflateFromXMLRes("xml/tabs/iptv.xml");
    this->categories->registerCell("Cell", CategoryCell::create);
    this->channels->registerCell("Cell", ChannelCell::create);
    this->categories->estimatedRowSpace = 4;
    this->channels->estimatedRowSpace = 6;
    this->categories->setDefaultCellFocus(0);
    this->doRequest();
}

IptvTab::~IptvTab() {}

brls::View* IptvTab::getDefaultFocus() { return this->categories; }

void IptvTab::doRequest() {
    this->account = xtream::Account::load();
    if (!this->account.valid()) {
        this->categories->setError("Add " + AppConfig::instance().configDir() + "/iptv.json");
        return;
    }
    this->categories->showSkeleton(8);
    ASYNC_RETAIN
    xtream::getCategories(
        this->account,
        [ASYNC_TOKEN](std::vector<xtream::Category> list) {
            ASYNC_RELEASE
            if (list.empty()) {
                this->categories->setEmpty();
            } else {
                auto first = list.front();
                if (!this->account.startCategory.empty()) {
                    for (auto& c : list) {
                        if (c.name.find(this->account.startCategory) != std::string::npos) {
                            first = c;
                            break;
                        }
                    }
                }
                this->categories->setDataSource(new CategoryDataSource(this, std::move(list)));
                this->loadChannels(first);
            }
        },
        [ASYNC_TOKEN](const std::string& ex) {
            ASYNC_RELEASE
            this->categories->setError(ex);
        });
}

void IptvTab::loadChannels(const xtream::Category& category) {
    this->channelsTitle->setTitle(category.name);
    this->channels->showSkeleton(8);
    ASYNC_RETAIN
    xtream::getChannels(
        this->account, category.id,
        [ASYNC_TOKEN](std::vector<xtream::Channel> list) {
            ASYNC_RELEASE
            if (list.empty()) {
                this->channels->setEmpty();
                return;
            }
            auto* src = new ChannelDataSource(this->account, std::move(list));
            src->recycler = this->channels;
            this->channels->setDataSource(src);
        },
        [ASYNC_TOKEN](const std::string& ex) {
            ASYNC_RELEASE
            this->channels->setError(ex);
        });
}
