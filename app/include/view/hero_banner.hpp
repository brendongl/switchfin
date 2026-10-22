/*
    Sipflix: Moonfin-style home hero banner (backdrop + logo + info panel + pager dots)
*/

#pragma once

#include <borealis.hpp>
#include "api/jellyfin/media.hpp"

namespace jellyfin {
struct HeroItem : public Episode {
    std::vector<std::string> BackdropImageTags;
    std::vector<std::string> Genres;
    std::string OfficialRating;
    float CommunityRating = 0.0f;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HeroItem, Id, Name, Type, ImageTags, ProductionYear, UserData, Chapters,
    RunTimeTicks, IndexNumber, ParentIndexNumber, Overview, ParentThumbImageTag, ParentThumbItemId, SeriesId,
    SeriesName, SeriesPrimaryImageTag, ParentBackdropItemId, ParentBackdropImageTags, MediaSources, BackdropImageTags,
    Genres, OfficialRating, CommunityRating);
}  // namespace jellyfin

class HeroBanner : public brls::Box {
public:
    HeroBanner();
    ~HeroBanner() override;

    static brls::View* create();

    /// load the latest movies / series from the server
    void doRequest();

    brls::View* getDefaultFocus() override { return this; }

private:
    BRLS_BIND(brls::Image, imageBackdrop, "hero/backdrop");
    BRLS_BIND(brls::Image, imageLogo, "hero/logo");
    BRLS_BIND(brls::Label, labelTitle, "hero/title");
    BRLS_BIND(brls::Label, labelMeta, "hero/meta");
    BRLS_BIND(brls::Label, labelOverview, "hero/overview");
    BRLS_BIND(brls::Box, boxDots, "hero/dots");

    std::vector<jellyfin::HeroItem> items;
    size_t current = 0;
    size_t timerIter = 0;

    void show(size_t index);
    void step(int delta);
    void scheduleNext();
};
