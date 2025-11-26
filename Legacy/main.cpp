#include <iostream>
#include "Api.h"

#include <string>
#include <vector>
#include <cctype>

#define VERSION "0.0.0B"                 // Version of the app.
#define BETA true                        // If the app is in beta.

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

int main(int argc, char* argv[]) {
    print("Starting ItlwmCLI legacy version %s %s...", VERSION, BETA ? "beta" : "release");
    bool power = false;
    bool available = get_power_state(&power);
    print("Itlwm status: %s, %s", available ? "available" : "unavailable", power ? "on" : "off");
    return 0;
}