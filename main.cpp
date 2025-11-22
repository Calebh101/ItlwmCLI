#include <QCoreApplication>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/image.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "HeliPort/ClientKit/Api.h"

using namespace ftxui;

// Sections
    // Device info (is itlwm running, device model, driver info)
    // Signal strength/status
    // Available networks
    // Terminal row for connecting and such

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    auto screen = ScreenInteractive::TerminalOutput();

    network_info_list_t networks;
    platform_info_t platformInfo;
    char* currentSsid;
    char* currentBssid;
    bool currentPowerState;
    uint32_t* current80211State;

    std::vector<std::string> output;
    std::string input_str;
    auto input = Input(&input_str, "");

    auto renderer = Renderer([&] {
        Elements output_elements;
        for (auto &line : output) output_elements.push_back(text(line));

        return vbox({
            hbox(),
            vbox({
                vbox(output_elements) | border,
                hbox({text("> "), input->Render()}),
            }),
        });
    });

    auto interactive = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Return) {
            output.push_back("> " + input_str);
            //
            input_str.clear();
            return true;
        }

        return false;
    });
}
