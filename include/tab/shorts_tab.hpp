#pragma once

#include <borealis.hpp>

#include "tab/home_tab.hpp"

// The Shorts tab is the Home feed locked to the "shorts" kiosk: same grid,
// paging and playback, no category cycling.
class ShortsTab : public HomeTab {
public:
    ShortsTab() : HomeTab("shorts") {}

    static brls::View* create() { return new ShortsTab(); }
};
