#if defined(__SWITCH__)
#include <switch.h>
#endif

#include <borealis.hpp>
#include <cstdlib>
#include <exception>
#include <string>

#include "activity/main_activity.hpp"
#include "newpipe/auth_store.hpp"
#include "newpipe/image_loader.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/settings_store.hpp"
#if defined(__SWITCH__)
#include "newpipe/switch_player.hpp"
#endif
#include "tab/home_tab.hpp"
#include "tab/search_tab.hpp"
#include "tab/settings_tab.hpp"
#include "tab/subscriptions_tab.hpp"
#include "tab/library_tab.hpp"
#include "newpipe/async_runner.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/paging_scrolling_frame.hpp"
#include "view/stream_grid.hpp"
#include "view/svg_image.hpp"

namespace {

// The Switch OLED panel is the target: it lights pixels individually, so a
// true-black background is both a deeper-looking UI and less panel power than
// the near-black #121212 this used to run on, and surfaces have to step up from
// black in small increments to stay separable without glowing.
void configure_theme() {
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    brls::Theme& dark = brls::Theme::getDarkTheme();

    dark.addColor("color/newpipe", nvgRGB(244, 67, 54));
    dark.addColor("color/newpipe_bg", nvgRGB(0, 0, 0));
    dark.addColor("color/newpipe_card", nvgRGB(20, 20, 23));
    dark.addColor("color/newpipe_thumb", nvgRGB(32, 32, 36));
    dark.addColor("color/newpipe_text", nvgRGB(242, 242, 242));
    dark.addColor("color/newpipe_text_dim", nvgRGB(170, 170, 176));
    dark.addColor("color/newpipe_text_faint", nvgRGB(128, 128, 136));
    dark.addColor("color/newpipe_divider", nvgRGB(34, 34, 39));
    dark.addColor("color/grey_1", nvgRGB(20, 20, 23));
    dark.addColor("color/grey_2", nvgRGB(30, 30, 34));
    dark.addColor("color/grey_3", nvgRGBA(160, 160, 160, 160));

    // borealis' own dark theme is built around #2D2D2D panels, which on an OLED
    // reads as washed-out grey next to the app's own surfaces. Pull the shared
    // chrome (frame, sidebar, dialogs, focus glow) onto the same black.
    dark.addColor("brls/clear", nvgRGB(0, 0, 0));
    dark.addColor("brls/background", nvgRGB(0, 0, 0));
    dark.addColor("brls/text", nvgRGB(242, 242, 242));
    dark.addColor("brls/text_disabled", nvgRGB(110, 110, 116));
    dark.addColor("brls/backdrop", nvgRGBA(0, 0, 0, 205));
    dark.addColor("brls/accent", nvgRGB(244, 67, 54));
    dark.addColor("brls/click_pulse", nvgRGBA(244, 67, 54, 38));
    dark.addColor("brls/highlight/background", nvgRGB(26, 26, 30));
    dark.addColor("brls/highlight/color1", nvgRGB(244, 67, 54));
    dark.addColor("brls/highlight/color2", nvgRGB(255, 138, 128));
    dark.addColor("brls/applet_frame/separator", nvgRGB(38, 38, 43));
    dark.addColor("brls/sidebar/background", nvgRGB(10, 10, 12));
    dark.addColor("brls/sidebar/active_item", nvgRGB(244, 67, 54));
    dark.addColor("brls/sidebar/separator", nvgRGB(34, 34, 39));
    dark.addColor("brls/spinner/bar_color", nvgRGBA(200, 200, 208, 90));

    // Settings cells, buttons and sliders ship in borealis' mint-green accent,
    // which is the one thing on screen that does not belong to this app.
    dark.addColor("brls/list/listItem_value_color", nvgRGB(244, 67, 54));
    dark.addColor("brls/button/primary_enabled_background", nvgRGB(244, 67, 54));
    dark.addColor("brls/button/primary_enabled_text", nvgRGB(255, 255, 255));
    dark.addColor("brls/button/default_enabled_background", nvgRGB(30, 30, 34));
    dark.addColor("brls/button/highlight_enabled_text", nvgRGB(255, 138, 128));
    dark.addColor("brls/button/highlight_disabled_text", nvgRGB(150, 90, 86));
    dark.addColor("brls/button/enabled_border_color", nvgRGB(70, 70, 78));
    dark.addColor("brls/button/disabled_border_color", nvgRGB(50, 50, 56));
    dark.addColor("brls/slider/line_filled", nvgRGB(244, 67, 54));
    dark.addColor("brls/slider/line_empty", nvgRGB(60, 60, 66));

    brls::Theme& light = brls::Theme::getLightTheme();

    light.addColor("color/newpipe", nvgRGB(216, 67, 21));
    light.addColor("color/newpipe_bg", nvgRGB(248, 248, 248));
    light.addColor("color/newpipe_card", nvgRGB(255, 255, 255));
    light.addColor("color/newpipe_thumb", nvgRGB(226, 228, 232));
    light.addColor("color/newpipe_text", nvgRGB(24, 24, 26));
    light.addColor("color/newpipe_text_dim", nvgRGB(90, 90, 96));
    light.addColor("color/newpipe_text_faint", nvgRGB(130, 130, 138));
    light.addColor("color/newpipe_divider", nvgRGB(222, 222, 226));
    light.addColor("color/grey_1", nvgRGB(255, 255, 255));
    light.addColor("color/grey_2", nvgRGB(235, 236, 238));
    light.addColor("color/grey_3", nvgRGBA(200, 200, 200, 16));

    // The sidebar is icon-only now, so it only has to fit a 38px icon plus the
    // active-tab accent bar and its margins - the label width that forced 180
    // is no longer a constraint.
    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 100);
}

void register_views() {
    brls::Application::registerXMLView("AutoTabFrame", AutoTabFrame::create);
    brls::Application::registerXMLView("PagingScrollingFrame", PagingScrollingFrame::create);
    brls::Application::registerXMLView("StreamGrid", StreamGrid::create);
    brls::Application::registerXMLView("SVGImage", SVGImage::create);
    brls::Application::registerXMLView("HomeTab", HomeTab::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);
    brls::Application::registerXMLView("SubscriptionsTab", SubscriptionsTab::create);
    brls::Application::registerXMLView("LibraryTab", LibraryTab::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);
}

bool run_borealis_ui() {
    bool image_loader_started = false;
    bool async_runner_started = false;
    newpipe::log_line("main: borealis init begin");

    if (!brls::Application::init()) {
        newpipe::log_line("main: Application::init failed");
        return false;
    }

    newpipe::log_line("main: createWindow");
    brls::Application::createWindow(newpipe::tr("app/title"));
    // With the B-press exit confirmation gone, + is the deliberate way out.
    // registerExitAction() quits straight away, so this adds an exit route
    // without adding a prompt back.
    brls::Application::setGlobalQuit(true);
    configure_theme();

    newpipe::log_line("main: register XML views");
    register_views();

    newpipe::log_line("main: start ImageLoader");
    newpipe::ImageLoader::instance().start();
    image_loader_started = true;

    newpipe::log_line("main: start AsyncRunner");
    newpipe::AsyncRunner::instance().start();
    async_runner_started = true;

    newpipe::log_line("main: push MainActivity");
    brls::Application::pushActivity(new MainActivity());

    const std::string playback_error = newpipe::take_last_playback_error();
    if (!playback_error.empty()) {
        brls::sync([playback_error]() {
            auto* dialog =
                new brls::Dialog(newpipe::tr("app/playback_failed", playback_error));
            dialog->addButton(newpipe::tr("hints/ok"), [dialog]() { dialog->close(); });
            dialog->setCancelable(true);
            dialog->open();
        });
    }

    newpipe::log_line("main: enter mainLoop");
    try {
        while (brls::Application::mainLoop()) {
        }
    } catch (...) {
        if (async_runner_started) {
            newpipe::AsyncRunner::instance().stop();
        }
        if (image_loader_started) {
            newpipe::ImageLoader::instance().stop();
        }
        throw;
    }
    newpipe::log_line("main: mainLoop exit");

    // Stopped before the image loader: in-flight catalog work can still queue
    // thumbnail fetches, and the worker has to be gone before that queue is.
    if (async_runner_started) {
        newpipe::AsyncRunner::instance().stop();
    }
    if (image_loader_started) {
        newpipe::ImageLoader::instance().stop();
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    newpipe::init_log();
    newpipe::log_line("main: start");
    try {
        std::string auth_error;
        if (!newpipe::AuthStore::instance().load(&auth_error) && !auth_error.empty()) {
            newpipe::logf("main: auth load failed error=%s", auth_error.c_str());
        }
        std::string settings_error;
        if (!newpipe::SettingsStore::instance().load(&settings_error) && !settings_error.empty()) {
            newpipe::logf("main: settings load failed error=%s", settings_error.c_str());
        }
        brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        while (true) {
            brls::Platform::APP_LOCALE_DEFAULT = newpipe::locale_from_setting(
                newpipe::SettingsStore::instance().settings().language);
            newpipe::clear_pending_playback();
            if (!run_borealis_ui()) {
                newpipe::shutdown_log();
                return EXIT_FAILURE;
            }

#if defined(__SWITCH__)
            const auto pending_playback = newpipe::take_pending_playback();
            if (!pending_playback.has_value()) {
                break;
            }

            newpipe::logf("main: launch player title=%s", pending_playback->title.c_str());
            std::string playback_error;
            if (!newpipe::run_switch_player(*pending_playback, playback_error)) {
                newpipe::logf("main: player failed error=%s", playback_error.c_str());
                newpipe::set_last_playback_error(playback_error.empty() ? "unknown error" : playback_error);
            } else {
                newpipe::log_line("main: player finished");
            }
            continue;
#else
            break;
#endif
        }

        newpipe::shutdown_log();
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        newpipe::logf("main: exception: %s", ex.what());
    } catch (...) {
        newpipe::log_line("main: unknown exception");
    }

    newpipe::shutdown_log();
    return EXIT_FAILURE;
}
