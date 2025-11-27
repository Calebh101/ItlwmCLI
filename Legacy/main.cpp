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

#define VERSION "0.0.0B"                 // Version of the app.
#define BETA true                        // If the app is in beta.

#define HEADER_LINES 2                   // How many lines are in the header.

static struct termios oldt, newt;
std::vector<std::string> logs;
int positionAway = 0; // How far we have scrolled up in the command line
int logScrolledLeft = 0; // How far we've scrolled right in the command line

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

bool processCommand(std::string input) {
    return false;
}

void* worker(void* arg) {
    std::string input = *static_cast<std::string*>(arg);
    bool valid = processCommand(input);
    if (!valid) logs.push_back(format("Invalid command: %s", input.c_str()));
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

int main(int argc, char* argv[]) {
    print("Starting ItlwmCLI legacy version %s %s...", VERSION, BETA ? "beta" : "release");
    initTerminal();
    clearScreen();

    std::string input;
    char c;

    while (true) {
        std::cout << "\033[H\033[2J";
        std::time_t now = std::time(0);
        winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        std::cout << format("ItlwmCLI Legacy %s %s by Calebh101\n", VERSION, BETA ? "Beta" : "Release");
        std::cout << "Time: " << std::ctime(&now);
        std::cout << std::string(w.ws_col, '_') << "\n\n";

        int maxLogRows = w.ws_row - HEADER_LINES - 3; // header + prompt + divider
        int toShow = std::min((int)logs.size(), maxLogRows);
        int start = std::max(0, (int)logs.size() - toShow - positionAway);
        int topPadding = maxLogRows - toShow;

        for (int i = 0; i < topPadding; i++) std::cout << "\n";
        for (int i = start; i < start + toShow && i < logs.size(); ++i) std::cout << std::setw(4) << (i + 1) << ". " << (logs[i].size() > logScrolledLeft ? logs[i].substr(logScrolledLeft) : "") << "\n";

        std::cout << "\033[" << w.ws_row << ";1H";
        std::cout << "  ##. > " << input << "";

        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') {
                if (!input.empty()) {
                    logs.push_back(format("> %s", input.c_str()));
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
        usleep(10000);
    }

    restoreTerminal();
    return 0;
}