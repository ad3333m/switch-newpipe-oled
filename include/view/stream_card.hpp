#pragma once

#include <string>

#include <borealis.hpp>

#include "newpipe/models.hpp"

class StreamCard : public brls::Box {
public:
    StreamCard();

    void setData(const newpipe::StreamItem& item);

    // Fades and lifts the card into place. `delayMs` staggers it behind the
    // cards ahead of it in the same batch, so an appended page rolls in rather
    // than appearing all at once.
    void playEntrance(int delayMs);

    void draw(
        NVGcontext* vg,
        float x,
        float y,
        float width,
        float height,
        brls::Style style,
        brls::FrameContext* ctx) override;

    void onFocusGained() override;
    void onFocusLost() override;

private:
    // Pushes the current animation values onto the view. Cheap to call every
    // frame: it only touches the view when a value actually moved.
    void applyMotion();
    void animateLift(float target);

    brls::Animatable entrance_{1.0f};
    brls::Animatable lift_{0.0f};
    bool entranceRunning_ = false;
    bool thumbnailShown_ = false;
    float appliedTranslation_ = 0.0f;
    float appliedAlpha_ = 1.0f;

    BRLS_BIND(brls::Image, thumbnail, "stream/thumbnail");
    BRLS_BIND(brls::Box, badgeBox, "stream/badge");
    BRLS_BIND(brls::Label, badgeLabel, "stream/badge_text");
    BRLS_BIND(brls::Label, titleLine1, "stream/title_line1");
    BRLS_BIND(brls::Label, titleLine2, "stream/title_line2");
    BRLS_BIND(brls::Label, channelLabel, "stream/channel");
    BRLS_BIND(brls::Label, metaLabel, "stream/meta");
};
