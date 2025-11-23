# ItlwmCLI

ItlwmCLI is a small TUI to directly interact with itlwm (the Intel WiFi driver for macOS). Note that this *does not* work for AirportItlwm.

ItlwmCLI is more manual than HeliPort, as it *is* a TUI and not a GUI. It's made with FTXUI and HeliPort's ClientKit (which only works on macOS).

This tool also works in macOS Recovery, even though HeliPort does not. You just need to download the binary and place it somewhere accessible by macOS (note that macOS Sonoma and Sequoia can't automatically mount exFAT) and run the binary from the terminal (Utilities > Terminal).

# How to Use

# Changelog

## 0.0.0A

Initial beta.

# Compiling Yourself

Requirements:
- macOS (because it has specific header files for HeliPort)
- FTXUI header files accessible (I installed via Brew)
