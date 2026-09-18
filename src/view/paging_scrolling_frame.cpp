#include "view/paging_scrolling_frame.hpp"

void PagingScrollingFrame::draw(
    NVGcontext* vg,
    float x,
    float y,
    float width,
    float height,
    brls::Style style,
    brls::FrameContext* ctx) {
    brls::ScrollingFrame::draw(vg, x, y, width, height, style, ctx);

    if (!this->onReachEnd_ || !this->armed_) {
        return;
    }

    const float contentHeight = this->getContentHeight();
    if (contentHeight <= 0.0f) {
        // Nothing has been laid out yet; the first page is still on its way.
        return;
    }

    // Negative when the whole list already fits on screen, which still counts
    // as "at the end" - a short first page should keep filling itself.
    const float remaining = contentHeight - height - this->getContentOffsetY();
    if (remaining > height * this->prefetchScreens_) {
        return;
    }

    // Deferred to the next sync tick rather than called from here: the callback
    // touches views (a spinner, a footer label), and borealis' invalidate()
    // re-runs layout for the entire tree synchronously - which is not something
    // to do halfway through a draw pass.
    this->armed_ = false;
    std::weak_ptr<bool> alive = this->alive_;
    brls::sync([this, alive]() {
        if (alive.expired()) {
            return;
        }
        if (!this->onReachEnd_ || !this->onReachEnd_()) {
            // Nothing was started, so ask again on the next frame.
            this->armed_ = true;
        }
    });
}
