/*
    Sipflix: Moonfin-style home hero banner
*/

#include "view/hero_banner.hpp"
#include "view/auto_tab_frame.hpp"
#include "tab/media_movie.hpp"
#include "tab/media_series.hpp"
#include "api/jellyfin.hpp"
#include "utils/image.hpp"
#include <fmt/format.h>

using namespace brls::literals;  // for _i18n

static const int HERO_ROTATE_MS = 9000;

const std::string heroBannerXML = R"xml(
    <brls:Box
        width="100%"
        height="290"
        focusable="true"
        cornerRadius="10"
        highlightCornerRadius="12"
        marginBottom="12">

        <brls:Image
            id="hero/backdrop"
            width="100%"
            height="100%"
            scalingType="fill"
            cornerRadius="10"
            backgroundColor="@theme/color/grey_1" />

        <brls:Image
            positionType="absolute"
            positionBottom="0"
            positionLeft="0"
            width="100%"
            height="100%"
            scalingType="stretch"
            image="@res/img/fade-bottom-dark.png" />

        <brls:Box
            positionType="absolute"
            positionBottom="26"
            positionLeft="26"
            positionRight="26"
            axis="column">

            <brls:Box height="82" width="100%">
                <brls:Image
                    id="hero/logo"
                    width="320"
                    height="82"
                    scalingType="fit" />
                <brls:Label
                    id="hero/title"
                    fontSize="34"
                    singleLine="true"
                    visibility="gone" />
            </brls:Box>

            <brls:Label
                id="hero/meta"
                marginTop="6"
                fontSize="17"
                textColor="#ffffff"
                singleLine="true" />

            <brls:Label
                id="hero/overview"
                marginTop="4"
                fontSize="15"
                height="40"
                textColor="#c8c8c8" />
        </brls:Box>

        <brls:Box
            id="hero/dots"
            positionType="absolute"
            positionBottom="8"
            positionLeft="0"
            width="100%"
            height="10"
            justifyContent="center" />
    </brls:Box>
)xml";

brls::View* HeroBanner::create() { return new HeroBanner(); }

HeroBanner::HeroBanner() {
    this->inflateFromXMLString(heroBannerXML);
    this->setAxis(brls::Axis::COLUMN);

    this->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        // first slide: let focus escape to the sidebar
        if (this->current == 0) return false;
        this->step(-1);
        return true;
    });
    this->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        this->step(1);
        return true;
    });
    this->registerAction("hints/ok"_i18n, brls::BUTTON_A, [this](...) {
        if (this->items.empty()) return true;
        auto& item = this->items.at(this->current);
        if (item.Type == jellyfin::mediaTypeSeries)
            this->present(new MediaSeries(item));
        else
            this->present(new MediaMovie(item));
        return true;
    });
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

HeroBanner::~HeroBanner() { brls::cancelDelay(this->timerIter); }

void HeroBanner::doRequest() {
    std::string query = HTTP::encode_form({
        {"includeItemTypes", "Movie,Series"},
        {"fields", "Overview,Genres,CommunityRating,OfficialRating,BasicSyncInfo"},
        {"enableImageTypes", "Backdrop,Logo"},
        {"imageTypeLimit", "1"},
        {"limit", "12"},
    });
    ASYNC_RETAIN
    jellyfin::getJSON<std::vector<jellyfin::HeroItem>>(
        [ASYNC_TOKEN](const std::vector<jellyfin::HeroItem>& r) {
            ASYNC_RELEASE
            this->items.clear();
            for (auto& it : r) {
                if (!it.BackdropImageTags.empty()) this->items.push_back(it);
                if (this->items.size() >= 8) break;
            }
            if (this->items.empty()) {
                this->setVisibility(brls::Visibility::GONE);
                return;
            }
            this->setVisibility(brls::Visibility::VISIBLE);
            this->boxDots->clearViews();
            for (size_t i = 0; i < this->items.size(); i++) {
                auto* dot = new brls::Rectangle();
                dot->setWidth(8);
                dot->setHeight(8);
                dot->setCornerRadius(4);
                dot->setMargins(0, 3, 0, 3);
                this->boxDots->addView(dot);
            }
            this->show(0);
        },
        [ASYNC_TOKEN](const std::string& ex) {
            ASYNC_RELEASE
            this->setVisibility(brls::Visibility::GONE);
        },
        jellyfin::apiUserLatest, AppConfig::instance().getUserId(), query);
}

void HeroBanner::show(size_t index) {
    if (this->items.empty()) return;
    this->current = index % this->items.size();
    auto& item = this->items.at(this->current);

    Image::load(this->imageBackdrop, jellyfin::apiBackdropImage, item.Id, 0,
        HTTP::encode_form({{"tag", item.BackdropImageTags.at(0)}, {"maxWidth", "1280"}}));

    auto logo = item.ImageTags.find(jellyfin::imageTypeLogo);
    if (logo != item.ImageTags.end()) {
        this->labelTitle->setVisibility(brls::Visibility::GONE);
        this->imageLogo->setVisibility(brls::Visibility::VISIBLE);
        Image::load(this->imageLogo, jellyfin::apiLogoImage, item.Id,
            HTTP::encode_form({{"tag", logo->second}, {"maxWidth", "640"}}));
    } else {
        Image::cancel(this->imageLogo);
        this->imageLogo->setVisibility(brls::Visibility::GONE);
        this->labelTitle->setText(item.Name);
        this->labelTitle->setVisibility(brls::Visibility::VISIBLE);
    }

    std::string meta;
    auto add = [&meta](const std::string& s) {
        if (s.empty()) return;
        if (!meta.empty()) meta += "  \xc2\xb7  ";
        meta += s;
    };
    if (item.ProductionYear) add(std::to_string(item.ProductionYear));
    add(item.OfficialRating);
    if (item.CommunityRating > 0) add(fmt::format("\xe2\x98\x85 {:.1f}", item.CommunityRating));
    std::string genres;
    for (size_t i = 0; i < item.Genres.size() && i < 3; i++) genres += (i ? ", " : "") + item.Genres[i];
    add(genres);
    this->labelMeta->setText(meta);
    this->labelOverview->setText(item.Overview);

    auto& dots = this->boxDots->getChildren();
    for (size_t i = 0; i < dots.size(); i++) {
        auto* dot = dynamic_cast<brls::Rectangle*>(dots[i]);
        if (dot) dot->setColor(i == this->current ? nvgRGB(255, 255, 255) : nvgRGBA(255, 255, 255, 90));
    }
    this->scheduleNext();
}

void HeroBanner::step(int delta) {
    if (this->items.empty()) return;
    size_t n = this->items.size();
    this->show((this->current + n + delta) % n);
}

void HeroBanner::scheduleNext() {
    brls::cancelDelay(this->timerIter);
    ASYNC_RETAIN
    this->timerIter = brls::delay(HERO_ROTATE_MS, [ASYNC_TOKEN]() {
        ASYNC_RELEASE
        this->step(1);
    });
}
