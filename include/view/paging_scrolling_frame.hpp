#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <borealis.hpp>

// A ScrollingFrame that asks for the next page before the user reaches the
// bottom, so a feed reads as one continuous list instead of a fixed block of
// results that just stops.
//
// The callback returns true when it actually started loading a page: the frame
// then stays quiet until resetPagingTrigger() says the page landed. Returning
// false (already loading, nothing more to fetch) re-arms it, so the next frame
// asks again.
class PagingScrollingFrame : public brls::ScrollingFrame {
public:
    PagingScrollingFrame() = default;

    void draw(
        NVGcontext* vg,
        float x,
        float y,
        float width,
        float height,
        brls::Style style,
        brls::FrameContext* ctx) override;

    void setOnReachEnd(std::function<bool()> callback) { onReachEnd_ = std::move(callback); }

    // Re-arms the trigger. Call it once an appended page has been laid out, or
    // when the list is reset.
    void resetPagingTrigger() { armed_ = true; }

    // How far ahead of the bottom to ask, as a multiple of the visible height.
    void setPrefetchScreens(float screens) { prefetchScreens_ = screens; }

    static brls::View* create() { return new PagingScrollingFrame(); }

private:
    std::function<bool()> onReachEnd_;
    bool armed_ = true;
    float prefetchScreens_ = 1.25f;

    // The callback is run from brls::sync rather than straight out of draw()
    // (see the note in draw()), so it needs a way to know this frame is still
    // around by the time it runs.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};
