<h1 align="center">ItlwmCLI</h1>
<p align="center">A command-line tool to communicate with OpenIntelWireless/itlwm (not AirportItlwm). Works in macOS Recovery, from Catalina to Tahoe.</p>

<p align="center">
    <a href="https://github.com/Calebh101/ItlwmCLI/actions/workflows/build.yml">
        <img src="https://github.com/Calebh101/ItlwmCLI/actions/workflows/build.yml/badge.svg">
    </a>
</p>

ItlwmCLI is a small TUI to directly interact with itlwm (the Intel WiFi driver for macOS). Note that this *does not* work for AirportItlwm.

ItlwmCLI is more manual than HeliPort. It's made with FTXUI and HeliPort's ClientKit, and thus *theoretically* has the same capabilities as HeliPort

This tool also works in macOS Recovery, even though HeliPort does not. You just need to download the binary and place it somewhere accessible by macOS (note that macOS Sonoma and Sequoia can't automatically mount exFAT) and run the binary from the terminal (Utilities > Terminal).

To open it, simply run the binary. You can double-click it from Finder, or use the command line to start it.

# Compatibility

- macOS Sierra (10.12) through macOS Tahoe (26) (64-bit).
- itlwm (**not** AirportItlwm). itlwm is the driver that presents your Intel WiFi card as Ethernet, and AirportItlwm presents your card as a WiFi card.

# How to Use

ItlwmCLI may *look* complex at first, but it's really not.

![Screenshot at Calebh101/ItlwmCLI/Assets/Screenshots/home.png](https://github.com/Calebh101/ItlwmCLI/raw/refs/heads/master/Assets/Screenshots/home.png)
<sub>Note that this screenshot might be slightly outdated.</sub>

The top row is the header. This tells you about ItlwmCLI, and about itlwm itself.

The top left section shows you about the active connection. It tells you itlwm's status in the top line. (Note that `Idle (Default)` doesn't always mean itlwm is actually idle. It's the default status returned.) The second line tells you the WiFi standard, the interface being used, and the current WiFi channel you're on. The third line tells you what SSID you're connected too, and the fourth line tells you the current RSSI of your connection. (see below)

The bottom left is a graph that streams your RSSI values. A higher value means a better RSSI, and a lower value is a worse RSSI. It's constantly moving and displaying your RSSI. The graph is relative to the highest RSSI itlwm's reported and the lowest RSSI itlwm's reported.

The right section lists all networks detected by itlwm. It shows the SSID and the RSSI (see below). It'll also show `(locked)` if the network has security and `(connected)` if you're already connected to it.

The bottom section is the command line of this. You type your command and press enter. All arguments are positional, and can be surrounded by either single quotes (`'`) or double quotes (`"`). To start, try `help` to display more commands. In this command line, you can't go back to previously-used commands (maybe some day), but up/down scrolls you in the terminal logs. Left/right also scrolls you, well, left and right.

**Note**: To exit this app quickly, you can just type `e` and press enter (it's the same thing as typing `exit`), and you don't just immediately terminate it!

That should be all, hope you enjoy!

# Index

- RSSI is a way to measure signal strength, and it's always negative. The closer it is to 0, the better your connection is. If it's super far *away* from 0 (like -90), that WiFi connection is *not* good. Typically -30 to -50 is really good, and -80 to -90 is really bad.

# Compiling Yourself

Requirements:
- macOS (because it has specific header files that HeliPort needs)
- HeliPort (run `git submodule update --init`)
- `fmt` and `ftxui` installed (I installed from Brew)

# Credits

- OpenIntelWireless for [itlwm](https://github.com/OpenIntelWireless/itlwm)
- OpenIntelWireless for [HeliPort](https://github.com/OpenIntelWireless/HeliPort)/[ClientKit](https://github.com/OpenIntelWireless/HeliPort/tree/master/ClientKit)
    - Note: I had to make a copy (and edit said copy) their `Api.h` to make it work with C++
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) for the TUI framework
- [fmt](https://github.com/fmtlib/fmt) (I tried to use Qt's QStrings at first, but I couldn't get Qt to work in Recovery; fmt was a very easy replacement)
- [nlohmann/json](https://github.com/nlohmann/json) for doing exactly what you think it does, being an awesome JSON library.
- [gulrak/filesystem](https://github.com/gulrak/filesystem) for letting me compile lower than Catalina.
- [The LLVM Project](https://github.com/llvm/llvm-project) for a static version of the standard library (`libc++` and `libc++abi`)
- Apple for macOS (even though it's a love-hate relationship)