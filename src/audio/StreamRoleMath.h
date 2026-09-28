#pragma once

#include "TransportClock.h"

namespace rsd {

// AudioEngine runs recording (mic capture) and playback (session mix,
// metronome, preview audition) on two independent, always-open,
// single-direction RtAudio streams rather than one combined duplex stream
// (the input and output devices may be different physical hardware, which
// RtAudio/ALSA can't duplex-synchronize — see AudioEngine.cpp's
// rtOutputCallback/rtInputCallback comments). Exactly one of these should
// be true for any given state, so
// the two streams never both drive the transport clock for the same
// elapsed time and the output stream never plays anything while Recording.

// Whether the output stream should mix and play session audio this block.
inline bool outputStreamShouldMix(TransportState state) { return state == TransportState::Playing; }

// Whether the input stream should write captured frames into the capture
// ring / punch recorder this block.
inline bool inputStreamShouldCapture(TransportState state) {
    return state == TransportState::Recording;
}

} // namespace rsd
