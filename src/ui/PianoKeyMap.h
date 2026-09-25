#pragma once

#include <optional>

#include <QString>
#include <Qt>

namespace rsd {

// Maps a computer-keyboard key to a MIDI pitch for the on-screen piano's
// "typing keyboard" shortcuts, following the standard staggered layout
// used by e.g. GarageBand's Musical Typing / VMPK: white keys on the home
// row, black keys on the row above, each aligned above the gap between
// the two white keys it sits between. There's no black key between E-F or
// B-C, so the keys that would sit above those gaps (Q, R, I, Å) aren't
// mapped to anything.
//
// White row (11 keys, C3-F4): A S D F G H J K L Ö Ä
// Black row (7 keys, aligned above gaps): W E _ T Y U _ O P _
inline std::optional<int> pitchForComputerKey(int key) {
    switch (key) {
        case Qt::Key_A: return 48;  // C3
        case Qt::Key_W: return 49;  // C#3
        case Qt::Key_S: return 50;  // D3
        case Qt::Key_E: return 51;  // D#3
        case Qt::Key_D: return 52;  // E3
        case Qt::Key_F: return 53;  // F3
        case Qt::Key_T: return 54;  // F#3
        case Qt::Key_G: return 55;  // G3
        case Qt::Key_Y: return 56;  // G#3
        case Qt::Key_H: return 57;  // A3
        case Qt::Key_U: return 58;  // A#3
        case Qt::Key_J: return 59;  // B3
        case Qt::Key_K: return 60;  // C4
        case Qt::Key_O: return 61;  // C#4
        case Qt::Key_L: return 62;  // D4
        case Qt::Key_P: return 63;  // D#4
        case Qt::Key_Odiaeresis: return 64; // E4
        case Qt::Key_Adiaeresis: return 65; // F4
        default: return std::nullopt;
    }
}

// Inverse of pitchForComputerKey(), for labeling the on-screen keys with
// their shortcut letter. Returns an empty string for a pitch with no
// computer-keyboard shortcut.
inline QString computerKeyLabelForPitch(int pitch) {
    switch (pitch) {
        case 48: return QStringLiteral("A");
        case 49: return QStringLiteral("W");
        case 50: return QStringLiteral("S");
        case 51: return QStringLiteral("E");
        case 52: return QStringLiteral("D");
        case 53: return QStringLiteral("F");
        case 54: return QStringLiteral("T");
        case 55: return QStringLiteral("G");
        case 56: return QStringLiteral("Y");
        case 57: return QStringLiteral("H");
        case 58: return QStringLiteral("U");
        case 59: return QStringLiteral("J");
        case 60: return QStringLiteral("K");
        case 61: return QStringLiteral("O");
        case 62: return QStringLiteral("L");
        case 63: return QStringLiteral("P");
        case 64: return QStringLiteral("Ö"); // Ö
        case 65: return QStringLiteral("Ä"); // Ä
        default: return QString();
    }
}

} // namespace rsd
