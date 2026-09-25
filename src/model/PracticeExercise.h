#pragma once

#include <vector>

#include <QString>

namespace rsd {

// A learn-to-play exercise for the piano practice mode: an ordered
// sequence of expected MIDI pitches to be played one at a time
// (highlight-and-wait, see PracticeMath.h).
struct PracticeExercise {
    QString name;
    std::vector<int> pitches;
};

inline const std::vector<PracticeExercise>& builtInPracticeExercises() {
    static const std::vector<PracticeExercise> exercises = {
        {"C Major Scale", {60, 62, 64, 65, 67, 69, 71, 72}},
        {"Twinkle Twinkle Little Star (opening phrase)",
         {60, 60, 67, 67, 69, 69, 67, 65, 65, 64, 64, 62, 62, 60}},
    };
    return exercises;
}

} // namespace rsd
