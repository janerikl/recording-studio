#pragma once

#include <array>

#include <QString>

namespace rsd {

// Standard GM percussion-key pitches (channel 10 / bank 128), a common
// subset used for the on-screen drum-pad widget.
struct GMDrumPad {
    int pitch;
    QString label;
};

inline const std::array<GMDrumPad, 10>& gmDrumPads() {
    static const std::array<GMDrumPad, 10> pads = {{
        {36, "Kick"},
        {38, "Snare"},
        {42, "Closed Hi-Hat"},
        {46, "Open Hi-Hat"},
        {49, "Crash"},
        {51, "Ride"},
        {45, "Low Tom"},
        {47, "Mid Tom"},
        {50, "Hi Tom"},
        {39, "Clap"},
    }};
    return pads;
}

} // namespace rsd
