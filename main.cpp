// ItlwmCLI main.cpp
// Copyright 2025 by Calebh101
//
// This file contains the entire source code for ItlwmCLI.

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/image.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "Api.h"
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <regex>
#include <fmt/format.h>
#include <fstream>
#include "json.hpp"
#include "filesystem.hpp"

#define VERSION "0.0.0B"                 // Version of the app.
#define BETA true                        // If the app is in beta.

#define CONSTANT_REFRESH_INTERVAL 50     // How many milliseconds the UI should wait to refresh (<= 0 to disable). Must be a factor of 1000.
#define RSSI_RECORD_INTERVAL 5           // How many iterations (CONSTANT_REFRESH_INTERVAL) to wait before the RSSI value should be recorded. The actual interval would be (CONSTANT_REFRESH_INTERVAL * RSSI_RECORD_INTERVAL) milliseconds.

#define RSSI_UNAVAILABLE_THRESHOLD -200  // The RSSI that means unavailable/invalid.
#define MAX_RSSI_RECORD_LENGTH 10000     // The max length of the RSSI list. After this, new values added chop off the old values.

#define HEADER_LINES 2                   // How many lines the header is.
#define VISIBLE_LOG_LINES 6              // How many lines are used for the command line widget.
#define VISIBLE_NETWORKS 32              // How many networks should be visible.

#define BAR_WIDTH 2                      // How wide the bars for the real-time signal graph should be.
#define TAB_MULTIPLIER 4                 // How many spaces a tab is in the command line widget.
#define LOG_INDEX_PADDING 5              // How much to pad the log lines' line numbers with spaces

// Make the script aware if it's running in debug mode
#ifdef __DEBUG
    #define DEBUG true
#else
    #define DEBUG false
#endif

using namespace ftxui;
using namespace ghc::filesystem;
using json = nlohmann::json;

auto screen = ScreenInteractive::TerminalOutput();
std::vector<std::string> output; // Logs
std::deque<int16_t> signalRssis; // Signal strengths recorded (for graphs)
int positionAway = 0; // How far we have scrolled up in the command line widget
int logScrolledLeft = 0; // How far we've scrolled right in the command line
bool running = false; // If the UI update thread should run
bool showSaveSettingsPrompt = true; // If we should ask to save a settings file (if it doesn't already exist)
std::thread refresher; // The UI update thread
ghc::filesystem::path exec; // Parent directory of the executable
ghc::filesystem::path settingsfile; // The file path containing our settings (potentially)
json settings; // Our global settings.

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
        case ITL80211_MODE_11B: return "IEEE 802.11b";
        case ITL80211_MODE_11G: return "IEEE 802.11g";
        case ITL80211_MODE_11N: return "IEEE 802.11n";
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

// Custom debug function, it just prints to the terminal. We shouldn't use this when the GUI is running.
template <typename... Args>
void debug(const std::string& input, Args&&... args) {
    std::cout << "ItlwmCLI: " << fmt::format(input, std::forward<Args>(args)...) << std::endl;
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

json loadSettings() {
    if (ghc::filesystem::exists(settingsfile)) {
        std::ifstream in(settingsfile);

        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();

            try {
                return json::parse(content);
            } catch (...) {
                debug("Failed to read settings file: invalid JSON (ignoring)");
                return json();
            }
        } else {
            debug("Failed to read settings file: {}", settingsfile.string());
            exit(1);
        }
    } else {
        return json();
    }
}

// Helper function so I don't have to duplicate my code
bool _saveSettings(json& settings) {
    std::ofstream out(settingsfile);

    if (out.is_open()) {
        out << settings.dump();
        out.close();
        return true;
    } else {
        log(fmt::format("Failed to write to settings file: {}", settingsfile.string()));
        return false;
    }
}

bool saveSettings(json& settings) {
    if (ghc::filesystem::exists(settingsfile)) {
        return _saveSettings(settings);
    } else {
        if (showSaveSettingsPrompt) {
            log("Unable to save to settings file; we don't know if you want to. Do note that all settings are stored in plain text (even passwords).");
            log(1, "To allow saving to a settings file, run 'settings file allow'. To hide this message ,run 'settings file deny'.");
        }

        return false;
    }
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

inline void trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    })); // Remove leading whitespace

    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end()); // Remove trailing whitespace
}

template <typename T>
T atOrDefault(std::vector<T>& input, int i, T _default) {
    if (i < input.size()) return input[i];
    return _default;
}

template <typename T>
std::optional<T> atOrNull(std::vector<T>& input, int i) {
    if (i < input.size()) return input[i];
    return std::nullopt;
}

void usage(std::string command = "") {
    if (command == "settings") {
        log("settings Usage:");
        log(1, "settings help                             Print this help message.");
        log(1, "settings clear                            Delete the app's settings file.");
        log(1, "settings file [status]                    Decide if you want to allow saving a settings file or not. If a settings file already exists, this is not necessary. 'status' can be 'allow' or 'deny'.");
    } else if (command == "save") {
        log("save/unsave Usage:");
        log(1, "save/unsave help                          Print this help message.");
        log(1, "save/unsace password [SSID] [password]    Save a password for a WiFi network, for use later.");
    } else if (command.empty()) {
        log("Usage:");
        log(1, "help [command]                            Print this help message, or optionally the help message of a different command.");
        log(1, "about                                     Show info about ItlwmCLI.");
        log(1, "exit/e                                    Peacefully exit my tool.");
        log(1, "power [status]                            Turn WiFi on or off. 'status' can be 'on' or 'off'.");
        log(1, "connect [ssid] [password]                 Connect to a WiFi network.");
        log(1, "associate [ssid] [password]               Associate a WiFi network, or make it known to itlwm.");
        log(1, "disassociate [ssid]                       Disassociate a WiFi network.");
        log(1, "save/unsave [subcommand]                  Save something for use later, or \"unsave\" (delete) a saved value.");
        log(1, "settings [subcommand]                     Manage settings.");
    } else {
        log(fmt::format("Invalid command: {}", command));
    }
}

bool processCommand(std::string input) {
    trim(input);
    auto command = parseCommand(input); // The full list of arguments
    auto action = command[0]; // The thing the user is trying to do, the first argument

    if (action == "help") {
        usage(atOrDefault(command, 1, std::string("")));
    } else if (action == "about") {
        log("ItlwmCLI by Calebh101");
        log(1, fmt::format("Version: {}", VERSION));
        log(1, fmt::format("{} release, {} mode", BETA ? "Beta" : "Stable", DEBUG ? "debug" : "release"));
    } else if (action == "exit" || action == "e") { // 'e' is helpful, so the user doesn't think Ctrl-C is the only efficient way to exit
        log("Thanks for stopping by!");
        log("Tip: itlwm will still be running even after you exit my program.");
        screen.PostEvent(Event::Custom);
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
        if (command.size() >= 2) {
            if (!settings.contains("savedPasswords") || !settings["savedPasswords"].is_object()) settings["savedPasswords"] = json::object();
            const std::string ssid = command[1];
            const std::string pswd = atOrDefault(command, 2, settings["savedPasswords"].value(ssid, "")); // Try to get the 3rd argument, then try to get the saved password, then default to empty

            log(fmt::format("Connecting to network '{}' with password '{}'...", ssid, pswd));
            connect_network(ssid.c_str(), pswd.c_str());
        } else {
            log("Command 'connect' needs 1-2 arguments.");
        }
    } else if (action == "associate") {
        if (command.size() >= 2) {
            if (!settings.contains("savedPasswords") || !settings["savedPasswords"].is_object()) settings["savedPasswords"] = json::object();
            const std::string ssid = command[1];
            const std::string pswd = atOrDefault(command, 2, settings["savedPasswords"].value(ssid, "")); // Try to get the 3rd argument, then try to get the saved password, then default to empty

            log(fmt::format("Associating network '{}' with password '{}'...", ssid, pswd));
            associate_ssid(ssid.c_str(), pswd.c_str());
        } else {
            log("Command 'associate' needs 1-2 arguments.");
        }
    } else if (action == "disassociate") {
        if (command.size() >= 2) {
            const std::string ssid = command[1];
            log(fmt::format("Disassociating network '{}'...", ssid));
            dis_associate_ssid(ssid.c_str());
        } else {
            log("Command 'disassociate' needs 1 argument.");
        }
    } else if (action == "save") {
        std::optional<std::string> subcommand = atOrNull(command, 1);

        if (subcommand == "help") {
            usage("save");
        } else if (subcommand == "password") {
            std::optional<std::string> ssid = atOrNull(command, 2);
            std::optional<std::string> pswd = atOrNull(command, 3);

            if (ssid == std::nullopt || pswd == std::nullopt) {
                log("Please provide both an SSID and a password.");
                return true;
            }

            settings["savedPasswords"][*ssid] = pswd;
            bool saved = saveSettings(settings);
            log(fmt::format("Saved SSID '{}' with password '{}'!", ssid.value_or("<unknown>"), pswd.value_or("<unknown>")));
        } else if (subcommand == std::nullopt) {
            log("A subcommand is required. (Run 'save help' for valid subcommands)");
        } else {
            log(fmt::format("Invalid subcommand: {} (run 'save help' for valid subcommands)", subcommand.value_or("<unknown>")));
        }
    } else if (action == "unsave") {
        std::optional<std::string> subcommand = atOrNull(command, 1);

        if (subcommand == "help") {
            usage("unsave");
        } else if (subcommand == "password") {
            std::optional<std::string> ssid = atOrNull(command, 2);

            if (ssid == std::nullopt) {
                log("Please provide ab SSID.");
                return true;
            }

            if (settings["savedPasswords"].contains(ssid.value_or(""))) {
                settings["savedPasswords"].erase(ssid.value_or(""));
                bool saved = saveSettings(settings);
                log(fmt::format("Unsaved SSID '{}'!", ssid.value_or("<unknown>")));
            } else {
                log("Provided SSID doesn't have a password saved.");
            }
        } else if (subcommand == std::nullopt) {
            log("A subcommand is required. (Run 'unsave help' for valid subcommands)");
        } else {
            log(fmt::format("Invalid subcommand: {} (run 'unsave help' for valid subcommands)", subcommand.value_or("<unknown>")));
        }
    } else if (action == "settings") {
        std::optional<std::string> subcommand = atOrNull(command, 1);

        if (subcommand == "help") {
            usage("settings");
        } else if (subcommand == "file") {
            std::optional<std::string> status = atOrNull(command, 2);

            if (status == "allow") {
                showSaveSettingsPrompt = false;
                _saveSettings(settings);
                log("Saved settings!");
            } else if (status == "decline") {
                showSaveSettingsPrompt = false;
                log("Declined to save settings.");
            } else {
                log("Please input a valid status.");
            }
        } else if (subcommand == "clear") {
            if (ghc::filesystem::exists(settingsfile) && std::remove(ghc::filesystem::absolute(settingsfile).c_str()) == 0) {
                log(fmt::format("Settings file at {} removed.", ghc::filesystem::absolute(settingsfile).string()));
            } else {
                log(fmt::format("Unable to remove settings file at {}. (Does it exist?)", ghc::filesystem::absolute(settingsfile).string()));
            }
        } else if (subcommand == std::nullopt) {
            log("A subcommand is required. (Run 'settings help' for valid subcommands)");
        } else {
            log(fmt::format("Invalid subcommand: {} (run 'settings help' for valid subcommands)", subcommand.value_or("<unknown>")));
        }
    } else if (action == "save/unsave") {
        log("No silly, I meant either 'save' or 'unsave'");
    } else {
        return false;
    }

    return true; // So we don't have to specify return true in each command, it's just caught in overflow
}

// For sorting networks by RSSI
bool compareNetworkStrength(const ioctl_network_info& a, const ioctl_network_info& b) {
    return abs(a.rssi) < abs(b.rssi);
}

int main(int argc, char* argv[]) {
    #ifndef __APPLE__
        debug("This program requires macOS to run.") // No Timmy, this doesn't work on Windows 11
    #endif

    debug("Starting app...");
    exec = ghc::filesystem::absolute(argv[0]).parent_path();
    settingsfile = exec / "ItlwmCLI.settings.json"; // File for settings

    // We finally get to the good stuff
    debug("Loading application...");
    settings = loadSettings();

    // Initialize all of itlwm's blah
    network_info_list_t* networks = new network_info_list_t;
    platform_info_t* platformInfo = new platform_info_t;
    station_info_t* stationInfo = new station_info_t;
    char currentSsid[MAX_SSID_LENGTH] = {0};
    char currentBssid[32] = {0};
    bool currentPowerState = false;
    uint32_t current80211State = 0;

    unsigned long iteration = 0; // How many times the refresher thread has iterated
    unsigned long pastIteration = -1; // Keeping track of "have we already recorded for this iteration?"
    int minRssi = 0; // Minimum RSSI of the graph
    int maxRssi = 0; // Maximum RSSI of the graph
    std::string input_str; // What the user has inputted in the command line widget
    auto input = Input(&input_str, "Type 'help' for available commands. Use up/down to scroll."); // The input provider for the command line widget
    debug("Loading renderer...");

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
            output_elements.push_back(text(fmt::format("{}{}.   {}", spaces, index, output[i].size() > logScrolledLeft ? output[i].substr(logScrolledLeft) : "")));
        }

        while (output_elements.size() < VISIBLE_LOG_LINES) { // Pad with blanks to keep FTXUI consistent
            output_elements.insert(output_elements.begin(), text(""));
        }

        // Make sure to clear each time, in case it changes
        std::memset(currentSsid, 0, sizeof(currentSsid));
        std::memset(currentBssid, 0, sizeof(currentBssid));

        // Query itlwm
        bool network_ssid_available = get_network_ssid(currentSsid);
        bool network_bssid_available = get_network_bssid(currentBssid);
        bool network_80211_state_available = get_80211_state(&current80211State);
        bool network_power_state_available = get_power_state(&currentPowerState);
        bool network_platform_info_available = get_platform_info(platformInfo);
        bool network_list_available = get_network_list(networks);
        bool station_info_available = get_station_info(stationInfo);

        // So uh, station_info_available is always false in my experience for some reason, so we're using a different method
        station_info_available = station_info_available || stationInfo ? true : false;
        bool rssi_available = station_info_available && stationInfo->rssi <= 0 && stationInfo->rssi > RSSI_UNAVAILABLE_THRESHOLD;

        // If the WiFi is off, then everything should be off
        if (network_power_state_available == false || currentPowerState == false) {
            network_ssid_available = false;
            network_bssid_available = false;
            network_80211_state_available = false;
            network_platform_info_available = false;
            network_list_available = false;
            station_info_available = false;
            rssi_available = false;
        }

        rssi_stage rssiStage = rssiToRssiStage(rssi_available, stationInfo ? stationInfo->rssi : 0);

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
                networks_elements.push_back(text(fmt::format("{}. {} (RSSI {}) {} {}", amount + 1, ssid, std::to_string(network.rssi), network.rsn_protos == 0 ? "" : "(locked)", network_ssid_available && connected ? "(connected)" : "")));
                amount++;
            }
        }

        while (networks_elements.size() < VISIBLE_NETWORKS) { // If we don't hit how many we want, pad the widget
            networks_elements.push_back(text(""));
        }

        if (rssi_available && iteration % RSSI_RECORD_INTERVAL == 0 && pastIteration != iteration) { // Every X iterations
            signalRssis.push_back(rssi_available ? stationInfo->rssi : RSSI_UNAVAILABLE_THRESHOLD);
            if (signalRssis.size() > MAX_RSSI_RECORD_LENGTH) signalRssis.pop_front();
            pastIteration = iteration;
        }

        // Setup stuff for the graph
        minRssi = *std::min_element(signalRssis.begin(), signalRssis.end()); // Minimum graph point (based on the entire dataset)
        maxRssi = *std::max_element(signalRssis.begin(), signalRssis.end()); // Maximum graph point (based on the entire dataset)
        if (minRssi == maxRssi) maxRssi = minRssi + 1;

        long long rssiSum = 0;
        for (int n : signalRssis) rssiSum += n;
        int rssiAverage = static_cast<int>((rssiSum - numbers.size()/2) / numbers.size());

        auto makeGraph = [rssi_available, minRssi, maxRssi](int width, int height) -> std::vector<int> {
            std::vector<int> scaled(width, 0);
            if (signalRssis.empty()) return scaled; // Empty, we don't have data yet
            std::deque<int16_t> data(signalRssis); // Duplicate the list
            int i;

            if (data.size() * BAR_WIDTH > width) { // Truncate the copied list
                std::deque<int16_t> subvec(data.end() - width / BAR_WIDTH, data.end());
                data = subvec;
            }

            size_t padSize = 0;
            if (data.size() < width) padSize = width - data.size() * BAR_WIDTH; // Padding

            for (size_t i = 0; i < data.size(); ++i) {
                int rssi = data[i];
                int y = (rssi - minRssi) * height / (maxRssi - minRssi); // Make it relative
                if (rssi <= RSSI_UNAVAILABLE_THRESHOLD) y = 0;

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
                text(fmt::format("ItlwmCLI {} {} by Calebh101", VERSION, DEBUG ? (BETA ? "Debug (Beta)" : "Debug") : (BETA ? "Beta" : "Release"))) | center, // We tell the user if the program is a beta release, a debug binary, or both
                text(fmt::format("Powered by itlwm {}", network_platform_info_available ? platformInfo->driver_info_str: "Unknown")) | center,
            }) | border | size(HEIGHT, EQUAL, HEADER_LINES + 2),
            // Body
            hbox({
                vbox({
                    // Stats
                    vbox({
                        text(fmt::format("{}, {}", network_power_state_available ? (currentPowerState ? "On" : "Off") : "Unavailable", parse80211State(network_80211_state_available, current80211State))),
                        text(fmt::format("{} @{} (channel {})", itlPhyModeToString(station_info_available, stationInfo->op_mode), network_platform_info_available ? platformInfo->device_info_str : "??", station_info_available ? std::to_string(stationInfo->channel) : "unavailable")),
                        text(fmt::format("Current SSID: {}", network_ssid_available ? currentSsid : "Unavailable")),
                        text(fmt::format("RSSI: {} ({}) (average: {})", rssi_available ? std::to_string(stationInfo->rssi) : "Unavailable", rssiStageToString(rssiStage), std::to_string(rssiAverage))),
                    }) | border | size(WIDTH, EQUAL, Terminal::Size().dimx / 2) | size(HEIGHT, EQUAL, 6),
                    // Graph showing signal strengths
                    vbox({
                        text("Graph of your RSSI") | center,
                        hbox({
                            vbox({
                                text(std::to_string(maxRssi)),
                                filler(),
                                text(std::to_string(static_cast<int>(std::round((minRssi + maxRssi) / 2.0)))), // Midpoint
                                filler(),
                                text(std::to_string(minRssi)),
                            }) | size(WIDTH, EQUAL, 5),
                            graph(makeGraph),
                        }) | flex,
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
            logScrolledLeft = 0; // Also make sure to scroll back to the right
            screen.PostEvent(Event::Custom); // Update UI

            std::thread([input]() { // Don't block
                bool valid = processCommand(input);
                if (!valid) log("Invalid command: " + input);
            }).detach();

            return true;
        } else if (event == Event::ArrowUp) { // Scroll up
            if (positionAway < output.size() - VISIBLE_LOG_LINES) {
                positionAway++;
            }
        } else if (event == Event::ArrowDown) { // Scroll down
            if (positionAway > 0) {
                positionAway--;
            }
        } else if (event == Event::ArrowRight) { // Scroll left
            logScrolledLeft++;
        } else if (event == Event::ArrowLeft) { // Scroll left
            if (logScrolledLeft > 0) {
                logScrolledLeft--;
            }
        }

        if (input->OnEvent(event)) return true;
        return false;
    });

    if (CONSTANT_REFRESH_INTERVAL > 0) {
        debug("Allowing constant refresh...");

        refresher = std::thread([&] {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(CONSTANT_REFRESH_INTERVAL));

                if (running) {
                    screen.PostEvent(Event::Custom);
                    iteration++;
                }
            }
        });
    }

    debug("Starting application...");
    if (refresher.joinable()) refresher.detach();
    std::system("clear");
    running = true;
    screen.Loop(interactive);
    running = false;
    api_terminate();
    return 0;
}
