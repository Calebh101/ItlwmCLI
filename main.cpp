#include <QCoreApplication>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/image.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "Api.h"
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QDir>
#include <QFile>
#include <iostream>

#define VERSION "0.0.0A"
#define ALLOW_CONSTANT_REFRESH true

#define VISIBLE_LOG_LINES 4
#define VISIBLE_NETWORKS 20
#define BAR_WIDTH 2

using namespace ftxui;

auto screen = ScreenInteractive::TerminalOutput();
std::vector<std::string> output;
std::vector<int> signalStrengths;
int positionAway = 0;
bool running = false;
std::thread refresher;

// Sections
    // Device info (is itlwm running, device model, driver info)
    // Signal strength/status
    // Available networks
    // Terminal row for connecting and such

// From HeliPort Common.h:
    // enum itl_80211_state {
    //     ITL80211_S_INIT    = 0,    /* default state */
    //     ITL80211_S_SCAN    = 1,    /* scanning */
    //     ITL80211_S_AUTH    = 2,    /* try to authenticate */
    //     ITL80211_S_ASSOC   = 3,    /* try to assoc */
    //     ITL80211_S_RUN     = 4     /* associated */
    // };

enum rssi_stage {
    rssi_stage_excellent,
    rssi_stage_good,
    rssi_stage_fair,
    rssi_stage_poor,
    rssi_stage_unavailable,
};

rssi_stage rssiToEnum(bool valid, int rssi) {
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

QString parse80211State(bool valid, uint32_t state) {
    if (!valid) return QString("Unavailable");

    switch (state) {
        case ITL80211_S_INIT: return QString("Idle (Default)");
        case ITL80211_S_SCAN: return QString("Scanning...");
        case ITL80211_S_AUTH: return QString("Authenticating...");
        case ITL80211_S_ASSOC: return QString("Associating...");
        case ITL80211_S_RUN: return QString("Running");
        default: return QString("Unknown");
    }
}

QString itlPhyModeToString(bool valid, itl_phy_mode mode) {
    if (!valid) return QString("Mode Unavailable");

    switch (mode) {
        case ITL80211_MODE_11A: return QString("IEEE 802.11a");
        case ITL80211_MODE_11B: return QString("IEEE 802.11ab");
        case ITL80211_MODE_11G: return QString("IEEE 802.11ag");
        case ITL80211_MODE_11N: return QString("IEEE 802.11an");
        case ITL80211_MODE_11AC: return QString("IEEE 802.11ac");
        case ITL80211_MODE_11AX: return QString("IEEE 802.11ax");
        default: return QString("Unknown Mode");
    }
}

void log(std::string input) {
    if (positionAway != 0) positionAway++;
    output.push_back(input);
}

void log(int indent, std::string input) {
    std::string spaces(indent * 4, ' ');
    return log(QString("%1%2").arg(spaces).arg(input).toStdString());
}

QStringList parseCommand(const QString& input) {
    QStringList args;
    QRegularExpression re(R"((\"[^\"]*\"|'[^']*'|\S+))");
    QRegularExpressionMatchIterator i = re.globalMatch(input);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString arg = match.captured(0);

        if ((arg.startsWith('"') && arg.endsWith('"')) ||
            (arg.startsWith('\'') && arg.endsWith('\''))) {
            arg = arg.mid(1, arg.length() - 2);
        }

        args << arg;
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

bool processCommand(std::string input) {
    auto command = parseCommand(QString::fromStdString(input).trimmed());
    auto action = command[0];

    if (action == "help") {
        usage();
    } else if (action == "exit" || action == "e") {
        log("Thanks for stopping by!");
        running = false;
        if (refresher.joinable()) refresher.join();
        screen.Exit();
    } else if (action == "echo") { // Debug command
        log(QString("Received command of '%1' with %2 extra arguments").arg(action).arg(command.size() - 1).toStdString());
    } else if (action == "power") {
        if (command.size() <= 2) {
            const QString status = command[1];
            int result;

            if (status == "on") {
                result = power_on();
            } else if (status == "off") {
                result = power_off();
            } else {
                log("State must be 'on' or 'off'.");
                return true;
            }

            log(QString("Power turned %1 with status %2.").arg(status).arg(result).toStdString());
        } else {
            log("Command 'power' needs 1 argument.");
        }
    } else if (action == "connect") {
        if (command.size() >= 3) {
            const QString ssid = command[1];
            const QString pswd = command[2];

            connect_network(ssid.toUtf8().constData(), pswd.toUtf8().constData());
            log(QString("Connecting to network '%1' with password '%2'...").arg(ssid).arg(pswd).toStdString());
        }
    } else if (action == "associate") {
        if (command.size() >= 3) {
            const QString ssid = command[1];
            const QString pswd = command[2];

            associate_ssid(ssid.toUtf8().constData(), pswd.toUtf8().constData());
            log(QString("Associating network '%1' with password '%2'...").arg(ssid).arg(pswd).toStdString());
        }
    } else if (action == "disassociate") {
        if (command.size() >= 2) {
            const QString ssid = command[1];
            dis_associate_ssid(ssid.toUtf8().constData());
            log(QString("Disassociating network '%1'...").arg(ssid).toStdString());
        }
    } else {
        return false;
    }

    return true;
}

bool compareNetworkStrength(const ioctl_network_info& a, const ioctl_network_info& b) {
    return abs(a.rssi) < abs(b.rssi);
}

int main(int argc, char *argv[]) {
    std::cout << "Loading settings..." << std::endl;
    QCoreApplication app(argc, argv);
    QDir exec(QCoreApplication::applicationDirPath());
    QFile settingsfile(exec.filePath("ItlwmCLI.settings.json"));

    if (!settingsfile.exists()) {
        if (settingsfile.open(QIODevice::WriteOnly)) {
            QTextStream out(&settingsfile);
            out << "{}";
            settingsfile.close();
        } else {
            std::cout << "Failed to create settings file: " << settingsfile.fileName().toStdString() << std::endl;
            exit(1);
        }
    }

    if (settingsfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&settingsfile);
        settingsfile.close();
    } else {
        std::cout << "Failed to read settings file: " << settingsfile.fileName().toStdString() << std::endl;
        exit(1);
    }

    std::cout << "Loading application..." << std::endl;

    network_info_list_t* networks = new network_info_list_t;
    platform_info_t* platformInfo = new platform_info_t;
    station_info_t* stationInfo = new station_info_t;
    char currentSsid[MAX_SSID_LENGTH] = {0};
    char currentBssid[32] = {0};
    bool currentPowerState = false;
    uint32_t current80211State = 0;

    std::string input_str;
    auto lastTime = std::chrono::steady_clock::now();
    auto input = Input(&input_str, "Type 'help' for available commands");
    std::cout << "Starting renderer..." << std::endl;

    auto renderer = Renderer([&] {
        Elements output_elements;
        Elements networks_elements;
        size_t start = (output.size() > VISIBLE_LOG_LINES) ? (output.size() - VISIBLE_LOG_LINES) : 0;
        bool foundConnected = false;

        for (size_t i = start - positionAway; i < output.size() - positionAway; ++i) {
            output_elements.push_back(text(QString("%1 %2").arg(QString("%1.").arg(static_cast<qlonglong>(i + 1)).leftJustified(4, ' ')).arg(output[i]).toStdString()));
        }

        while (output_elements.size() < VISIBLE_LOG_LINES) {
            output_elements.insert(output_elements.begin(), text(""));
        }

        bool network_ssid_available = get_network_ssid(currentSsid);
        bool network_bssid_available = get_network_bssid(currentBssid);
        bool network_80211_state_available = get_80211_state(&current80211State);
        bool network_power_state_available = get_power_state(&currentPowerState);
        bool network_platform_info_available = get_platform_info(platformInfo);
        bool network_list_available = get_network_list(networks);
        bool station_info_available = get_station_info(stationInfo);

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
        rssi_stage rssiStage = rssiToEnum(station_info_available, stationInfo ? stationInfo->rssi : 0);

        if (network_list_available) {
            std::sort(networks->networks, networks->networks + networks->count, compareNetworkStrength);
            int amount = 0;

            for (int i = 0; i < networks->count; i++) {
                auto network = networks->networks[i];

                bool emptySsid = std::all_of(std::begin(network.ssid), std::end(network.ssid), [](unsigned char c) {
                    return c == 0;
                });

                if (emptySsid) continue;
                bool connected = strcmp(currentSsid, reinterpret_cast<char*>(network.ssid)) == 0;
                if (connected) foundConnected = true;

                networks_elements.push_back(text(QString("%1. %2 %3 %4 %5").arg(amount + 1).arg(network.ssid).arg(network.rssi).arg(network.rsn_protos == 0 ? "" : "(locked)").arg(network_ssid_available && connected ? "(connected)" : "").toStdString()));
                amount++;
            }
        }

        while (networks_elements.size() < VISIBLE_NETWORKS) {
            networks_elements.push_back(text(""));
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime);

        if (elapsed.count() >= 200) {
            signalStrengths.push_back(station_info_available ? stationInfo->rssi : 0);
            lastTime = currentTime;
        }

        auto makeGraph = [](int width, int height) -> std::vector<int> {
            std::vector<int> scaled(width, 0);
            if (signalStrengths.empty()) return scaled;
            int i;

            if (signalStrengths.size() * BAR_WIDTH > width) {
                std::vector<int> subvec(signalStrengths.end() - width / BAR_WIDTH, signalStrengths.end());
                signalStrengths = subvec;
            }

            int minRssi = *std::min_element(signalStrengths.begin(), signalStrengths.end());
            int maxRssi = *std::max_element(signalStrengths.begin(), signalStrengths.end());
            if (minRssi == maxRssi) maxRssi = minRssi + 1;

            size_t padSize = 0;
            if (signalStrengths.size() < width) padSize = width - signalStrengths.size() * BAR_WIDTH;

            for (size_t i = 0; i < signalStrengths.size(); ++i) {
                int rssi = signalStrengths[i];
                int y = (rssi - minRssi) * height / (maxRssi - minRssi);

                for (size_t j = 0; j < BAR_WIDTH; ++j) {
                    if (padSize + i * BAR_WIDTH + j < scaled.size()) {
                        scaled[padSize + i * BAR_WIDTH + j] = y;
                    }
                }
            }

            return scaled;
        };

        return vbox({
            vbox({
                text(QString("ItlwmCLI %1 by Calebh101").arg(VERSION).toStdString()) | center,
                text(QString("Intel Wireless @%1").arg(platformInfo->device_info_str).toStdString()) | center,
                text(QString("Powered by itlwm v. %1").arg(platformInfo->driver_info_str).toStdString()) | center,
            }) | border,
            hbox({
                vbox({
                    vbox({
                        text(QString("%1, %2").arg(network_power_state_available ? (currentPowerState ? "On" : "Off") : "Unavailable").arg(parse80211State(network_80211_state_available, current80211State)).toStdString()),
                        text(QString("%1 (channel %2)").arg(itlPhyModeToString(station_info_available, stationInfo->op_mode)).arg(station_info_available ? QString::number(stationInfo->channel) : "unavailable").toStdString()),
                        text(QString("Current SSID: %1").arg(network_ssid_available ? currentSsid : "Unavailable").toStdString()),
                        text(QString("Signal strength: %1 (%2)").arg(station_info_available ? QString::number(stationInfo->rssi) : "Unavailable").arg(rssiStageToString(rssiStage)).toStdString()),
                    }) | border | size(WIDTH, EQUAL, Terminal::Size().dimx / 2) | size(HEIGHT, EQUAL, 6),
                    hbox({
                        graph(makeGraph),
                    }) | border | flex | size(WIDTH, EQUAL, Terminal::Size().dimx / 2),
                }),
                vbox({
                    networks_elements,
                }) | border | size(WIDTH, EQUAL, Terminal::Size().dimx / 2),
            }) | flex,
            vbox({
                vbox(output_elements),
                hbox({text("#.   > "), input->Render()}),
            }) | border | size(HEIGHT, EQUAL, VISIBLE_LOG_LINES + 3) | vscroll_indicator,
        });
    });

    auto interactive = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Return) {
            if (QString::fromStdString(input_str).trimmed().isEmpty()) return true;
            log("> " + input_str);
            bool valid = processCommand(input_str);
            if (!valid) log("Invalid command: " + input_str);
            input_str.clear();
            positionAway = 0;
            return true;
        } else if (event == Event::ArrowUp) {
            if (positionAway < output.size() - VISIBLE_LOG_LINES) {
                positionAway++;
            }
        } else if (event == Event::ArrowDown) {
            if (positionAway > 0) {
                positionAway--;
            }
        }

        if (input->OnEvent(event)) return true;
        return false;
    });

    running = true;

    if (ALLOW_CONSTANT_REFRESH) {
        std::cout << "Allowing constant refresh..." << std::endl;

        refresher = std::thread([&] {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                screen.PostEvent(Event::Custom);
            }
        });
    }

    std::cout << "Starting application..." << std::endl;
    std::system("clear");
    screen.Loop(interactive);
    running = false;
    return 0;
}
