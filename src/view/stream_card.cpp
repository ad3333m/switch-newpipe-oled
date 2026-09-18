#include "view/stream_card.hpp"

#include <cstdint>
#include <vector>

#include "newpipe/i18n.hpp"
#include "newpipe/image_loader.hpp"

namespace {

// Roughly how wide the title labels are in the 1280x720 design space: a 24%
// card inside the padded grid, minus the card's own padding. borealis lays the
// whole UI out against that fixed canvas whether the console is docked or
// handheld, so this does not drift with the output resolution.
constexpr float kTitleLineWidth = 232.0f;
constexpr float kTitleFontSize = 15.0f;

struct Glyph {
    size_t offset;
    size_t length;
    float width;
};

// Decodes UTF-8 far enough to tell a half-width glyph from a full-width one.
// This only has to be close: borealis measures the real font and ellipsises
// each line itself, so all this decides is where the title breaks.
std::vector<Glyph> measure_glyphs(const std::string& text) {
    std::vector<Glyph> glyphs;
    glyphs.reserve(text.size());

    for (size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        size_t length = 1;
        uint32_t codepoint = lead;

        if ((lead & 0xE0) == 0xC0) {
            length = 2;
            codepoint = lead & 0x1Fu;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            codepoint = lead & 0x0Fu;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            codepoint = lead & 0x07u;
        }

        if (i + length > text.size()) {
            length = 1;
            codepoint = lead;
        } else {
            for (size_t k = 1; k < length; k++) {
                codepoint = (codepoint << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
            }
        }

        // CJK, Hangul, kana and emoji take about a full em; Latin about half.
        const bool wide = codepoint >= 0x1100
            && (codepoint <= 0x115F || (codepoint >= 0x2E80 && codepoint <= 0xA4CF)
                || (codepoint >= 0xAC00 && codepoint <= 0xD7A3)
                || (codepoint >= 0xF900 && codepoint <= 0xFAFF)
                || (codepoint >= 0xFF00 && codepoint <= 0xFF60) || codepoint >= 0x1F000);

        glyphs.push_back(Glyph{i, length, kTitleFontSize * (wide ? 1.0f : 0.5f)});
        i += length;
    }

    return glyphs;
}

// Splits a title over the card's two title lines, breaking on a space where
// there is one. Anything that still does not fit is left for borealis to
// ellipsise on the second line.
void split_title(const std::string& title, std::string* line1, std::string* line2) {
    line1->clear();
    line2->clear();

    const auto glyphs = measure_glyphs(title);
    float used = 0.0f;
    size_t break_offset = title.size();
    size_t last_space_offset = std::string::npos;

    for (const auto& glyph : glyphs) {
        if (glyph.length == 1 && title[glyph.offset] == ' ') {
            last_space_offset = glyph.offset;
        }

        used += glyph.width;
        if (used > kTitleLineWidth) {
            break_offset = last_space_offset != std::string::npos ? last_space_offset : glyph.offset;
            break;
        }
    }

    if (break_offset >= title.size()) {
        *line1 = title;
        return;
    }

    *line1 = title.substr(0, break_offset);
    size_t rest = break_offset;
    while (rest < title.size() && title[rest] == ' ') {
        rest++;
    }
    *line2 = title.substr(rest);
}

}  // namespace

StreamCard::StreamCard() {
    this->inflateFromXMLRes("xml/views/stream_card.xml");
}

void StreamCard::setData(const newpipe::StreamItem& item) {
    std::string line1;
    std::string line2;
    split_title(item.title, &line1, &line2);

    if (titleLine1) {
        titleLine1->setText(line1);
    }
    if (titleLine2) {
        titleLine2->setText(line2);
    }
    if (channelLabel) {
        channelLabel->setText(item.channel_name);
    }

    // Views and age on one line; the duration moved onto the thumbnail, which
    // is where people look for it and frees this row for text that used to be
    // squeezed into half a card.
    if (metaLabel) {
        std::string meta = item.view_count_text;
        if (!item.published_text.empty()) {
            if (!meta.empty()) {
                meta += " \u00B7 ";
            }
            meta += item.published_text;
        }
        metaLabel->setText(meta);
    }

    const std::string badge =
        item.is_live ? newpipe::tr("stream/live_badge") : item.duration_text;
    if (badgeLabel) {
        badgeLabel->setText(badge);
    }
    if (badgeBox) {
        badgeBox->setVisibility(
            badge.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        badgeBox->setBackgroundColor(
            item.is_live ? brls::Application::getTheme()["color/newpipe"]
                         : nvgRGBA(0, 0, 0, 230));
    }

    if (thumbnail && !item.thumbnail_url.empty()) {
        newpipe::ImageLoader::instance().load(item.thumbnail_url, thumbnail);
    }
}
