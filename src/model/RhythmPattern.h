#pragma once

#include <vector>

#include <QString>

namespace rsd {

// One note (or rest) in a rhythm-reading exercise, in beats (1.0 = a
// quarter note at the session's BPM).
struct RhythmNote {
    double beats = 0.0;
    bool isRest = false;
};

// A word-mnemonic rhythm pattern for the rhythm-reading trainer (see
// PracticePanel's Rhythm mode): the word's syllables map to the notes'
// durations, e.g. "Mo-zza-rel-la" -> four equal fast notes.
struct RhythmPattern {
    QString word;
    std::vector<RhythmNote> notes;
};

// Approximate reading of a reference teaching sheet (word + rhythmic
// notation pairs); precision beyond "reads about right" wasn't required
// (approved as a v1 best-effort transcription).
inline const std::vector<RhythmPattern>& builtInRhythmPatterns() {
    static const std::vector<RhythmPattern> patterns = {
        {"Mozzarella", {{0.25, false}, {0.25, false}, {0.25, false}, {0.25, false}}},
        {"Coconut", {{0.5, false}, {0.5, false}, {0.25, false}, {0.25, false}}},
        {"Strawberry", {{0.25, false}, {0.25, false}, {0.5, false}, {0.5, false}}},
        {"Cucumber", {{0.5, false}, {0.25, false}, {0.25, false}, {0.5, false}}},
        {"Orange", {{0.5, false}, {0.25, false}, {0.5, true}, {1.0, true}}},
        {"Lemon", {{0.5, false}, {0.5, false}, {1.0, false}}},
        {"Mango", {{0.5, false}, {0.25, false}, {0.25, false}, {0.5, false}}},
    };
    return patterns;
}

} // namespace rsd
