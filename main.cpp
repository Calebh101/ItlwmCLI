// ItlwmCLI main.cpp
// Copyright 2025 by Calebh101

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/image.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "Api.h"
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fmt/format.h>
#include <fstream>
#include <filesystem>

#define VERSION "0.0.0A"                 // Version of the app.
#define ENABLE_SETTINGS false            // Whether to use and store settings
#define CONSTANT_REFRESH_INTERVAL 50     // How many milliseconds the UI should wait to refresh (<= 0 to disable). Must be a factor of 1000.
#define RSSI_RECORD_INTERVAL 50          // How many milliseconds to wait before the RSSI value should be recorded (dependent on CONSTANT_REFRESH_INTERVAL)

#define HEADER_LINES 3                   // How many lines the header is.
#define VISIBLE_LOG_LINES 4              // How many lines are used for the command line widget.
#define VISIBLE_NETWORKS 20              // How many networks should be visible.
#define BAR_WIDTH 2                      // How wide the bars for the real-time signal graph should be.
#define TAB_MULTIPLIER 4                 // How many spaces a tab is in the command line widget.
#define LOG_INDEX_PADDING 5              // How much to pad the log lines' line numbers with spaces

using namespace ftxui;

auto screen = ScreenInteractive::TerminalOutput();
std::vector<std::string> output; // Logs
std::vector<int> signalStrengths; // Signal strengths recorded (for graphs)
int positionAway = 0; // How far we have scrolled up in the command line
bool running = false; // If the UI update thread should run
std::thread refresher; // The UI update thread
auto lastTime = std::chrono::steady_clock::now(); // How long it's been since we've recorded an RSSI

enum rssi_stage {
    rssi_stage_excellent,
    rssi_stage_good,
    rssi_stage_fair,
    rssi_stage_poor,
    rssi_stage_unavailable,
};

// RSSI is negative, and the closer it is to 0, the better the signal strength
rssi_stage rssiToRssiStage(bool valid, int rssi) {
    if (!valid) return rssi_stage_unavailable;
    rssi = abs(rssi);

    if (rssi <= 0) return rssi_stage_unavailable;
    if (rssi <= 50) return rssi_stage_excellent;
    if (rssi <= 60) return rssi_stage_good;
    if (rssi <= 70) return rssi_stage_fair;
    return rssi_stage_poor;
}

std::string rssiStageToString(rssi_stage stage) {
    switch (stage) {
        case rssi_stage_excellent: return "excellent";
        case rssi_stage_good: return "good";
        case rssi_stage_fair: return "fair";
        case rssi_stage_poor: return "poor";
        case rssi_stage_unavailable: return "unavailable";
    }
}

Color rssiStageToColor(rssi_stage stage) {
    switch (stage) {
        case rssi_stage_excellent: return Color::Green;
        case rssi_stage_good: return Color::Green;
        case rssi_stage_fair: return Color::Green;
        case rssi_stage_poor: return Color::Green;
        case rssi_stage_unavailable: return Color::Green;
    }
}

std::string itlPhyModeToString(bool valid, itl_phy_mode mode) {
    if (!valid) return "Mode Unavailable";

    switch (mode) {
        case ITL80211_MODE_11A: return "IEEE 802.11a";
        case ITL80211_MODE_11B: return "IEEE 802.11ab";
        case ITL80211_MODE_11G: return "IEEE 802.11ag";
        case ITL80211_MODE_11N: return "IEEE 802.11an";
        case ITL80211_MODE_11AC: return "IEEE 802.11ac";
        case ITL80211_MODE_11AX: return "IEEE 802.11ax";
        default: return "Unknown Mode";
    }
}

std::string parse80211State(bool valid, uint32_t state) {
    if (!valid) return "Unavailable";

    switch (state) {
        case ITL80211_S_INIT: return "Idle (Default)";
        case ITL80211_S_SCAN: return "Scanning...";
        case ITL80211_S_AUTH: return "Authenticating...";
        case ITL80211_S_ASSOC: return "Associating...";
        case ITL80211_S_RUN: return "Running";
        default: return "Unknown";
    }
}

// Add to command line widget logs ('output')
void log(std::string input) {
    if (positionAway != 0) positionAway++; // If we're not following the logs, then scroll even farther away from them to stay where we are now
    output.push_back(input);
}

// Same thing as the above, but with an indent
void log(int indent, std::string input) {
    std::string spaces(indent * TAB_MULTIPLIER, ' ');
    return log(fmt::format("{}{}", spaces, input));
}

std::vector<std::string> parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::regex re(R"((\"[^\"]*\"|'[^']*'|\S+))"); // Double-quoted strings, single-quoted strings, non-whitespace strings

    auto begin = std::sregex_iterator(input.begin(), input.end(), re); // First match
    auto end = std::sregex_iterator(); // Last iteration

    for (auto iterator = begin; iterator != end; ++iterator) {
        std::string arg = iterator->str();
        if ((arg.front() == '"' && arg.back() == '"') || (arg.front() == '\'' && arg.back() == '\'')) arg = arg.substr(1, arg.length() - 2); // Remove quotes
        args.push_back(arg);
    }

    return args;
}

void usage() {
    log("Usage:");
    log(1, "help                                Print this help message");
    log(1, "exit/e                              Peacefully exit my tool");
    log(1, "power [status]                      Turn WiFi on or off. 'status' can be 'on' or 'off'.");
    log(1, "connect [ssid] [password]           Connect to a WiFi network.");
    log(1, "associate [ssid] [password]         Associate a WiFi network.");
    log(1, "disassociate [ssid]                 Disassociate a WiFi network.");
}

inline void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    })); // Remove leading whitespace

    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end()); // Remove trailing whitespace
}

bool processCommand(std::string input) {
    trim(input);
    auto command = parseCommand(input);
    auto action = command[0]; // The thing the user is trying to do

    if (action == "help") {
        usage();
    } else if (action == "exit" || action == "e") { // 'e' is helpful, so the user doesn't think Ctrl-C is the only efficient way to exit
        log("Thanks for stopping by!");
        running = false;
        if (refresher.joinable()) refresher.join(); // Wait to exit (so we don't crash)
        screen.Exit();
    } else if (action == "echo") { // Debug command, solely for command parsing tests; won't be listed to the user
        log(fmt::format("Received command of '{}' with {} extra arguments", action, command.size() - 1));
    } else if (action == "power") {
        if (command.size() <= 2) {
            const std::string status = command[1];
            int result;

            if (status == "on") {
                result = power_on();
            } else if (status == "off") {
                result = power_off();
            } else {
                log("State must be 'on' or 'off'.");
                return true;
            }

            log(fmt::format("Power turned {} with status {}.", status, result));
        } else {
            log("Command 'power' needs 1 argument.");
        }
    } else if (action == "connect") {
        if (command.size() >= 3) {
            const std::string ssid = command[1];
            const std::string pswd = command[2];

            connect_network(ssid.c_str(), pswd.c_str());
            log(fmt::format("Connecting to network '{}' with password '{}'...", ssid, pswd));
        } else {
            log("Command 'connect' needs 2 arguments.");
        }
    } else if (action == "associate") {
        if (command.size() >= 3) {
            const std::string ssid = command[1];
            const std::string pswd = command[2];

            associate_ssid(ssid.c_str(), pswd.c_str());
            log(fmt::format("Associating network '{}' with password '{}'...", ssid, pswd));
        } else {
            log("Command 'associate' needs 2 arguments.");
        }
    } else if (action == "disassociate") {
        if (command.size() >= 2) {
            const std::string ssid = command[1];
            dis_associate_ssid(ssid.c_str());
            log(fmt::format("Disassociating network '{}'...", ssid));
        } else {
            log("Command 'disassociate' needs 1 argument.");
        }
    } else {
        return false;
    }

    return true; // So we don't have to specify return true in each command, it's just caught in overflow
}

// For sorting
bool compareNetworkStrength(const ioctl_network_info& a, const ioctl_network_info& b) {
    return abs(a.rssi) < abs(b.rssi);
}

// Custom debug function
template <typename... Args>
void debug(const std::string& input, Args&&... args) {
    std::cout << "ItlwmCLI: " << fmt::format(input, std::forward<Args>(args)...) << std::endl;
}

int main(int argc, char *argv[]) {
    debug("Starting app...");
    std::filesystem::path exec = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path settingsfile = exec / "ItlwmCLI.settings.json"; // File for settings

    if (ENABLE_SETTINGS) {
        if (!std::filesystem::exists(settingsfile)) {
            std::ofstream out(settingsfile);
            if (out.is_open()) {
                out << "{}"; // Default JSON
                out.close();
            } else {
                debug("Failed to create settings file: {}", settingsfile.string());
                exit(1);
            }
        }

        std::ifstream in(settingsfile);

        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
        } else {
            debug("Failed to read settings file: {}", settingsfile.string());
            exit(1);
        }
    }

    // We finally get to the good stuff
    debug("Loading application...");

    // Initialize all of itlwm's blah
    network_info_list_t* networks = new network_info_list_t;
    platform_info_t* platformInfo = new platform_info_t;
    station_info_t* stationInfo = new station_info_t;
    char currentSsid[MAX_SSID_LENGTH] = {0};
    char currentBssid[32] = {0};
    bool currentPowerState = false;
    uint32_t current80211State = 0;

    unsigned long iteration = 0; // How many times the refresher thread has iterated
    std::string input_str; // What the user has inputted in the command line widget
    auto input = Input(&input_str, "Type 'help' for available commands"); // The input provider for the command line widget
    debug("Starting renderer...");

    auto renderer = Renderer([&] {
        Elements output_elements;
        Elements networks_elements;

        size_t start = (output.size() > VISIBLE_LOG_LINES) ? (output.size() - VISIBLE_LOG_LINES) : 0; // Where should we start rendering command logs?
        bool foundConnected = false; // If one of the networks returned from itlwm is the one we're connected to (it doesn't seem to do this in my testing)
        std::string inputIndexPadding(LOG_INDEX_PADDING - 1, ' '); // Padding for the command line prompt row

        for (size_t i = start - positionAway; i < output.size() - positionAway; ++i) {
            std::string index = std::to_string(i + 1);
            std::string spaces = "";
            while (index.size() + spaces.size() < LOG_INDEX_PADDING) spaces += " "; // Pad so the line numbers line up correctly
            output_elements.push_back(text(fmt::format("{}{}.   {}", spaces, index, output[i])));
        }

        while (output_elements.size() < VISIBLE_LOG_LINES) { // Pad with blanks to keep FTXUI consistent
            output_elements.insert(output_elements.begin(), text(""));
        }

        // Query itlwm
        bool network_ssid_available = get_network_ssid(currentSsid);
        bool network_bssid_available = get_network_bssid(currentBssid);
        bool network_80211_state_available = get_80211_state(&current80211State);
        bool network_power_state_available = get_power_state(&currentPowerState);
        bool network_platform_info_available = get_platform_info(platformInfo);
        bool network_list_available = get_network_list(networks);
        bool station_info_available = get_station_info(stationInfo);

        // Reset the cache if the WiFi is off
        if (network_power_state_available == false || currentPowerState == false) {
            memset(currentSsid, 0, sizeof(currentSsid));
            memset(currentBssid, 0, sizeof(currentBssid));
            if (platformInfo) memset(platformInfo, 0, sizeof(*platformInfo));

            if (networks) {
                memset(networks, 0, sizeof(*networks));
                memset(networks->networks, 0, sizeof(networks->networks));
                networks->count = 0;
            }

            if (stationInfo) memset(stationInfo, 0, sizeof(*stationInfo));
            current80211State = 0;
        }

        // So uh, station_info_available is always false for some reason, so we're using a different method
        station_info_available = stationInfo ? true : false;
        rssi_stage rssiStage = rssiToRssiStage(station_info_available, stationInfo ? stationInfo->rssi : 0);

        if (network_list_available) {
            std::sort(networks->networks, networks->networks + networks->count, compareNetworkStrength); // Sort
            int amount = 0;

            for (int i = 0; i < networks->count; i++) {
                auto network = networks->networks[i];

                bool emptySsid = std::all_of(std::begin(network.ssid), std::end(network.ssid), [](unsigned char c) {
                    return c == 0;
                }); // Is the SSID empty? Let's find out!

                if (emptySsid) continue; // If it is, then we skip
                bool connected = network_ssid_available ? strcmp(currentSsid, reinterpret_cast<char*>(network.ssid)) == 0 : false; // If our current SSID matches the one we're scanning
                if (connected) foundConnected = true;

                std::string ssid(reinterpret_cast<const char*>(network.ssid), strnlen(reinterpret_cast<const char*>(network.ssid), 32)); // fmt is stingy
                networks_elements.push_back(text(fmt::format("{}. {} ({} RSSI) {} {}", amount + 1, ssid, std::to_string(network.rssi), network.rsn_protos == 0 ? "" : "(locked)", network_ssid_available && connected ? "(connected)" : "")));
                amount++;
            }
        }

        while (networks_elements.size() < VISIBLE_NETWORKS) { // If we don't hit how many we want, pad the widget
            networks_elements.push_back(text(""));
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();

        if (elapsed >= RSSI_RECORD_INTERVAL) { // Every X milliseconds
            signalStrengths.push_back(station_info_available ? stationInfo->rssi : 0);
            lastTime = now;
        }

        auto makeGraph = [](int width, int height) -> std::vector<int> {
            std::vector<int> scaled(width, 0);
            if (signalStrengths.empty()) return scaled; // Empty, we don't have data yet
            std::vector<int> data(signalStrengths); // Duplicate the list
            int i;

            if (data.size() * BAR_WIDTH > width) { // Truncate the copied list
                std::vector<int> subvec(data.end() - width / BAR_WIDTH, data.end());
                data = subvec;
            }

            int minRssi = *std::min_element(signalStrengths.begin(), signalStrengths.end()); // Minimum graph point (based on the entire dataset)
            int maxRssi = *std::max_element(signalStrengths.begin(), signalStrengths.end()); // Maximum graph point (based on the entire dataset)
            if (minRssi == maxRssi) maxRssi = minRssi + 1;

            size_t padSize = 0;
            if (data.size() < width) padSize = width - data.size() * BAR_WIDTH; // Padding

            for (size_t i = 0; i < data.size(); ++i) {
                int rssi = data[i];
                int y = (rssi - minRssi) * height / (maxRssi - minRssi); // Make it relative

                for (size_t j = 0; j < BAR_WIDTH; ++j) { // Make X points, for however wide we want the bars
                    if (padSize + i * BAR_WIDTH + j < scaled.size()) {
                        scaled[padSize + i * BAR_WIDTH + j] = y;
                    }
                }
            }

            return scaled;
        };

        return vbox({
            // Header
            vbox({
                text(fmt::format("ItlwmCLI {} by Calebh101", VERSION)) | center,
                text(fmt::format("Intel Wireless @{}", platformInfo->device_info_str)) | center,
                text(fmt::format("Powered by itlwm v. {}", platformInfo->driver_info_str)) | center,
            }) | border | size(HEIGHT, EQUAL, HEADER_LINES + 2),
            // Body
            hbox({
                vbox({
                    // Stats
                    vbox({
                        text(fmt::format("{}, {}", network_power_state_available ? (currentPowerState ? "On" : "Off") : "Unavailable", parse80211State(network_80211_state_available, current80211State))),
                        text(fmt::format("{} (channel {})", itlPhyModeToString(station_info_available, stationInfo->op_mode), station_info_available ? std::to_string(stationInfo->channel) : "unavailable")),
                        text(fmt::format("Current SSID: {}", network_ssid_available ? currentSsid : "Unavailable")),
                        text(fmt::format("Signal strength: {} ({})", station_info_available ? std::to_string(stationInfo->rssi) : "Unavailable", rssiStageToString(rssiStage))),
                    }) | border | size(WIDTH, EQUAL, Terminal::Size().dimx / 2) | size(HEIGHT, EQUAL, 6),
                    // Graph showing signal strengths
                    hbox({
                        graph(makeGraph),
                    }) | border | flex | size(WIDTH, EQUAL, Terminal::Size().dimx / 2),
                }),
                // Shows what networks have been found
                vbox({
                    networks_elements,
                }) | border | size(WIDTH, EQUAL, Terminal::Size().dimx / 2),
            }) | flex | size(HEIGHT, EQUAL, Terminal::Size().dimy - VISIBLE_LOG_LINES - HEADER_LINES - 5), // visible log lines, header lines, then 2 for the header border and 3 for the command line border + input
            // Command line
            vbox({
                vbox(output_elements),
                hbox({text(fmt::format("{}#. > ", inputIndexPadding)), input->Render()}),
            }) | border | size(HEIGHT, EQUAL, VISIBLE_LOG_LINES + 3),
        });
    });

    auto interactive = CatchEvent(renderer, [&](Event event) { // Catch events, like keystrokes
        if (event == Event::Return) { // User tried to enter a command
            trim(input_str);
            if (input_str.empty()) return true;
            log("> " + input_str); // Echo command back into the log
            std::string input(input_str); // Make a copy
            input_str.clear();
            positionAway = 0; // Make sure to scroll down
            screen.PostEvent(Event::Custom); // Update UI
            bool valid = processCommand(input);
            if (!valid) log("Invalid command: " + input);
            return true;
        } else if (event == Event::ArrowUp) { // Scroll up
            if (positionAway < output.size() - VISIBLE_LOG_LINES) {
                positionAway++;
            }
        } else if (event == Event::ArrowDown) { // Scroll down
            if (positionAway > 0) {
                positionAway--;
            }
        }

        if (input->OnEvent(event)) return true;
        return false;
    });

    running = true;

    if (CONSTANT_REFRESH_INTERVAL > 0) {
        debug("Allowing constant refresh...");

        refresher = std::thread([&] {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(CONSTANT_REFRESH_INTERVAL));
                screen.PostEvent(Event::Custom);
            }
        });
    }

    debug("Starting application...");
    std::system("clear");
    screen.Loop(interactive);
    running = false;
    return 0;
}
