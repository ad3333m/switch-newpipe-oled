#pragma once

#include <optional>
#include <string>
#include <vector>

namespace newpipe {

struct Kiosk {
    std::string id;
    std::string title;
};

struct StreamItem {
    std::string id;
    std::string url;
    std::string title;
    std::string channel_name;
    std::string channel_url;
    std::string channel_id;
    std::string thumbnail_url;
    std::string duration_text;
    std::string view_count_text;
    std::string published_text;
    bool is_live = false;
};

struct CommentItem {
    std::string author_name;
    std::string author_url;
    std::string author_thumbnail_url;
    std::string body;
    std::string published_text;
    std::string like_count_text;
    std::string reply_count_text;
    bool is_verified = false;
};

// Where the next page of a feed has to be asked for. YouTube hands back an
// opaque token plus nothing at all about which endpoint understands it, so the
// endpoint has to be remembered alongside the token.
enum class ContinuationSource {
    NONE,
    SEARCH,
    BROWSE,
};

struct Continuation {
    ContinuationSource source = ContinuationSource::NONE;
    std::string token;

    bool valid() const { return source != ContinuationSource::NONE && !token.empty(); }
};

struct FeedPage {
    std::vector<StreamItem> items;
    Continuation continuation;
};

struct HomeFeed {
    Kiosk kiosk;
    std::vector<StreamItem> items;
    Continuation continuation;
};

struct SearchResults {
    std::string query;
    std::vector<StreamItem> items;
    Continuation continuation;
    bool used_fallback = false;
};

struct StreamDetail {
    StreamItem item;
    std::string description;
    std::string channel_subscriber_count_text;
    std::vector<StreamItem> related_items;
    std::optional<std::string> playback_url;
};

struct CommentPage {
    std::string title;
    std::vector<CommentItem> items;
};

}  // namespace newpipe
