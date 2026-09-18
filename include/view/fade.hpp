#pragma once

#include <borealis.hpp>

namespace newpipe {

// borealis drives a fade through hide()/show(): show() bails out unless the
// view is already flagged hidden, so an instant hide followed by an animated
// show is what actually fades something in. Spinners and the paging footer used
// to blink in and out of existence a frame at a time; these wrap that pattern
// so they ease instead.

inline void fade_in(brls::View* view, float durationMs = 160.0f) {
    if (!view) {
        return;
    }

    view->setVisibility(brls::Visibility::VISIBLE);
    view->hide([]() {}, false, 0.0f);
    view->show([]() {}, true, durationMs);
}

// Fades out, then collapses the view so it stops taking up layout space.
inline void fade_out_gone(brls::View* view, float durationMs = 160.0f) {
    if (!view || view->getVisibility() == brls::Visibility::GONE) {
        return;
    }

    view->hide(
        [view]() { view->setVisibility(brls::Visibility::GONE); },
        true,
        durationMs);
}

}  // namespace newpipe
