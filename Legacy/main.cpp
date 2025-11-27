#include "Api.h"
#include <iostream>
#include <string>
#include <ctime>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <vector>
#include <cctype>
#include <iomanip>
#include <sstream>

#define VERSION "0.0.0B"                 // Version of the app.
#define BETA true                        // If the app is in beta.
#define DEBUG false                      // The legacy version does not have a debug mode.

#define LOOP_INTERVAL 10000              // How much the program should wait in between each refresh, in microseconds.
#define RSSI_UNAVAILABLE_THRESHOLD -200  // The RSSI that means unavailable/invalid.

#define HEADER_LINES 5                   // How many lines are in the header.
#define MIN_LOG_INDEX_WIDTH 5            // The minimum width for the line numbers before the log lines. They dynamically expand.
#define TAB_MULTIPLIER 4                 // How many spaces a tab is in the command line widget.

static struct termios oldt, newt;
std::vector<std::string> logs;
int positionAway = 0; // How far we have scrolled up in the command line
int logScrolledLeft = 0; // How far we've scrolled right in the command line

// Initialize all of itlwm's blah
network_info_list_t* networks = new network_info_list_t;
platform_info_t* platformInfo = new platform_info_t;
station_info_t* stationInfo = new station_info_t;
char currentSsid[MAX_SSID_LENGTH] = {0};
char currentBssid[32] = {0};
bool currentPowerState = false;
uint32_t current80211State = 0;

bool network_ssid_available = false;
bool network_bssid_available = false;
bool network_80211_state_available = false;
bool network_power_state_available = false;
bool network_platform_info_available = false;
bool network_list_available = false;
bool station_info_available = false;

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

std::string vformat(const char* fmt, va_list args) {
    char buffer[1024];
    std::vsprintf(buffer, fmt, args);
    return std::string(buffer);
}

std::string format(const char* formatter, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, formatter);
    std::vsprintf(buffer, formatter, args);
    va_end(args);
    return std::string(buffer);
}

void print(const char* formatter, ...) {
    va_list args;
    va_start(args, formatter);
    std::cout << vformat(formatter, args) << '\n';
}

void log(const std::string input) {
    logs.push_back(input);
}

void log(int tabs, const std::string input) {
    logs.push_back(format("%s%s", std::string(tabs * TAB_MULTIPLIER, ' ').c_str(), input.c_str()));
}

std::vector<std::string> parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::string token;

    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;

    for (std::string::size_type i = 0; i < input.size(); i++) {
        char c = input[i];

        if (inDoubleQuotes) {
            if (c == '"') {
                inDoubleQuotes = false;
                args.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        } else if (inSingleQuotes) {
            if (c == '\'') {
                inSingleQuotes = false;
                args.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        } else {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!token.empty()) {
                    args.push_back(token);
                    token.clear();
                }
            } else if (c == '"') {
                inDoubleQuotes = true;
            } else if (c == '\'') {
                inSingleQuotes = true;
            } else {
                token += c;
            }
        }
    }

    if (!token.empty()) args.push_back(token);
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

void usage(std::string command = "") {
    if (command.empty()) {
        log("Usage:");
        log(1, "help [command]                            Print this help message, or optionally the help message of a different command.");
        log(1, "about                                     Show info about ItlwmCLI.");
        log(1, "exit/e                                    Peacefully exit my tool.");
        log(1, "power [status]                            Turn WiFi on or off. 'status' can be 'on' or 'off'.");
        log(1, "connect [ssid] [password]                 Connect to a WiFi network.");
        log(1, "associate [ssid] [password]               Associate a WiFi network, or make it known to itlwm.");
        log(1, "disassociate [ssid]                       Disassociate a WiFi network.");
        log(1, "list                                      List all networks found by itlwm.");
    } else {
        log(format("Invalid command: %s", command.c_str()));
        log(format("Tip: Not all commands have a dedicated usage page. Run 'help' for a list of all commands!"));
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
        log(1, format("Version: %s Legacy", VERSION));
        log(1, format("%s release, %s mode", BETA ? "Beta" : "Stable", DEBUG ? "debug" : "release"));
    } else if (action == "exit" || action == "e") { // 'e' is helpful, so the user doesn't think Ctrl-C is the only efficient way to exit
        std::cout << "\033[2J" << "\n\nThanks for stopping by!" << '\n' << "Tip: itlwm will still be running even after you exit my program.";
        exit(0);
    } else if (action == "echo") { // Debug command, solely for command parsing tests; won't be listed to the user
        log(format("Received command of '%s' with %d extra arguments", action.c_str(), command.size() - 1));
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

            log(format("Power turned %s with status %s.", status.c_str(), result));
        } else {
            log("Command 'power' needs 1 argument.");
        }
    } else if (action == "connect") {
        if (command.size() >= 2) {
            const std::string ssid = command[1];
            const std::string pswd = atOrDefault(command, 2, std::string("")); // Try to get the 3rd argument, then just default to blank

            log(format("Connecting to network '%s' with password '%s'...", ssid.c_str(), pswd.c_str()));
            connect_network(ssid.c_str(), pswd.c_str());
        } else {
            log("Command 'connect' needs 1-2 arguments.");
        }
    } else if (action == "associate") {
        if (command.size() >= 2) {
            const std::string ssid = command[1];
            const std::string pswd = atOrDefault(command, 2, std::string("")); // Try to get the 3rd argument, then just default to blank

            log(format("Associating network '%s' with password '%s'...", ssid.c_str(), pswd.c_str()));
            associate_ssid(ssid.c_str(), pswd.c_str());
        } else {
            log("Command 'associate' needs 1-2 arguments.");
        }
    } else if (action == "disassociate") {
        if (command.size() >= 2) {
            const std::string ssid = command[1];
            log(format("Disassociating network '%s'...", ssid.c_str()));
            dis_associate_ssid(ssid.c_str());
        } else {
            log("Command 'disassociate' needs 1 argument.");
        }
    } else if (action == "list") {
        if (network_list_available) {
            if (networks->count <= 0) {
                log("The current network list is empty.");
            } else {
                for (int i = 0; i < networks->count; i++) {
                    ioctl_network_info network = networks->networks[i];
                    bool emptySsid = true;

                    for (size_t i = 0; i < sizeof(network.ssid); ++i) {
                        if (network.ssid[i] != 0) {
                            emptySsid = false;
                            break;
                        }
                    }

                    if (emptySsid) continue;
                    log(format("%d. %s (RSSI %d)%s", i + 1, network.ssid, network.rssi, network.rsn_protos == 0 ? "" : " (locked)"));
                }
            }
        } else {
            log("The current network list is unavailable.");
        }
    } else {
        return false;
    }

    return true; // So we don't have to specify return true in each command, it's just caught in overflow
}

void* worker(void* arg) {
    std::string input = *static_cast<std::string*>(arg);
    bool valid = processCommand(input);
    if (!valid) log(format("Invalid command: %s", input.c_str()));
    delete static_cast<std::string*>(arg);
    return nullptr;
}

void initTerminal() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void* querier(void* arg) {
    while (true) {
        network_ssid_available = get_network_ssid(currentSsid);
        network_bssid_available = get_network_bssid(currentBssid);
        network_80211_state_available = get_80211_state(&current80211State);
        network_power_state_available = get_power_state(&currentPowerState);
        network_platform_info_available = get_platform_info(platformInfo);
        network_list_available = get_network_list(networks);
        station_info_available = get_station_info(stationInfo);

        usleep(LOOP_INTERVAL);
    }
}

int main(int argc, char* argv[]) {
    print("Starting ItlwmCLI legacy version %s %s...", VERSION, BETA ? "beta" : "release");
    std::string input;
    char c;
    pthread_t t;

    if (pthread_create(&t, nullptr, querier, nullptr)) {
        std::cerr << "Failed to create thread!\n";
        return 1;
    }

    pthread_detach(t);
    print("Starting application...");
    initTerminal();
    clearScreen();

    while (true) {
        std::cout << "\033[H\033[2J";
        winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        // Make sure to clear each time, in case it changes
        std::memset(currentSsid, 0, sizeof(currentSsid));
        std::memset(currentBssid, 0, sizeof(currentBssid));

        // So uh, station_info_available is always false in my experience for some reason, so we're using a different method
        station_info_available = station_info_available || stationInfo != nullptr ? true : false;
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

        std::cout << format("ItlwmCLI Legacy %s %s by Calebh101\n", VERSION, BETA ? "Beta" : "Release");
        std::cout << format("Powered by itlwm %s\n\n", network_platform_info_available ? platformInfo->driver_info_str: "Unknown");
        std::cout << format("%s @%s (channel %d)\n", itlPhyModeToString(station_info_available, stationInfo->op_mode).c_str(), network_platform_info_available ? platformInfo->device_info_str : "??", station_info_available ? stationInfo->channel : 0);
        std::cout << format("Current SSID: %s, RSSI: %d (%s)\n", network_ssid_available ? currentSsid : "Unavailable", station_info_available ? stationInfo->rssi : 0, rssiStageToString(rssiStage).c_str());
        std::cout << std::string(w.ws_col, '_') << "\n\n"; // Make it look like a solid centered line, even though it's just an underscore

        int maxLogRows = w.ws_row - HEADER_LINES - 3; // header + prompt + divider
        int toShow = std::min((int)logs.size(), maxLogRows);
        int start = std::max(0, (int)logs.size() - toShow - positionAway);
        int topPadding = maxLogRows - toShow;

        for (int i = 0; i < topPadding; i++) std::cout << "\n";
        for (int i = start; i < start + toShow && i < logs.size(); ++i) std::cout << std::setw(MIN_LOG_INDEX_WIDTH) << (i + 1) << ". " << (logs[i].size() > logScrolledLeft ? logs[i].substr(logScrolledLeft) : "") << "\n";

        std::ostringstream ss;
        ss << logs.size();

        std::cout << "\033[" << w.ws_row << ";1H";
        std::cout << std::setw(MIN_LOG_INDEX_WIDTH) << std::string(ss.str().size(), '#') << ". > " << input << "";

        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') {
                if (!input.empty()) {
                    log(format("> %s", input.c_str()));
                    std::string* string = new std::string(input.c_str());
                    input.clear();
                    positionAway = 0; // Make sure to scroll down
                    logScrolledLeft = 0; // Also make sure to scroll back to the right
                    pthread_t t;

                    if (pthread_create(&t, nullptr, worker, string)) {
                        std::cerr << "Failed to create thread!\n";
                        return 1;
                    }

                    pthread_detach(t);
                }
            } else if (c == 127 || c == 8) {
                if (!input.empty()) input.erase(input.size() - 1);
            } else if (c == 27) {
                char seq[2];

                if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                    if (seq[0] == '[') {
                        switch (seq[1]) {
                            case 'A' /* up */:
                                if (positionAway < logs.size() - maxLogRows) positionAway++;
                                break;
                            case 'B' /* down */:
                                if (positionAway > 0) positionAway--;
                                break;
                            case 'C' /* right */:
                                logScrolledLeft++;
                                break;
                            case 'D' /* left */:
                                if (logScrolledLeft > 0) logScrolledLeft--;
                                break;
                            default: break; // Unknown sequence
                        }
                    }
                }
            } else if (std::isprint(static_cast<unsigned char>(c))) { // If it's a valid character
                input += c;
            }
        }

        std::cout.flush();
        usleep(LOOP_INTERVAL);
    }

    restoreTerminal();
    return 0;
}