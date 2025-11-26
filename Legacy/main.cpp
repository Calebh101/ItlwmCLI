#include <iostream>
#include "Api.h"

int main(int argc, char* argv[]) {
    std::cout << "Starting ItlwmHelper legacy..." << std::endl;
    bool power = false;
    get_power_state(&power);
    std::cout << "itlwm status: " << (power ? "on": "off") << std::endl;
    return 0;
}