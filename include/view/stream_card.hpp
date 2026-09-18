#pragma once

#include <string>

#include <borealis.hpp>

#include "newpipe/models.hpp"

class StreamCard : public brls::Box {
public:
    StreamCard();

    void setData(const newpipe::StreamItem& item);

private:
    BRLS_BIND(brls::Image, thumbnail, "stream/thumbnail");
    BRLS_BIND(brls::Box, badgeBox, "stream/badge");
    BRLS_BIND(brls::Label, badgeLabel, "stream/badge_text");
    BRLS_BIND(brls::Label, titleLine1, "stream/title_line1");
    BRLS_BIND(brls::Label, titleLine2, "stream/title_line2");
    BRLS_BIND(brls::Label, channelLabel, "stream/channel");
    BRLS_BIND(brls::Label, metaLabel, "stream/meta");
};
