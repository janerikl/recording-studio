#pragma once

#include <cstdint>
#include <optional>

#include <QStringList>
#include <QString>

namespace rsd {

// Formats a sample position as "HH:MM:SS.mmm". Pure function so the
// rounding/rollover math is unit-testable without a live transport clock.
inline QString formatTimecode(int64_t samples, unsigned sampleRate) {
    if (samples < 0 || sampleRate == 0) {
        return QStringLiteral("00:00:00.000");
    }

    const double totalSeconds = static_cast<double>(samples) / static_cast<double>(sampleRate);
    const int64_t totalMillis = static_cast<int64_t>(totalSeconds * 1000.0);

    const int64_t millis = totalMillis % 1000;
    const int64_t totalSecondsInt = totalMillis / 1000;
    const int64_t seconds = totalSecondsInt % 60;
    const int64_t totalMinutes = totalSecondsInt / 60;
    const int64_t minutes = totalMinutes % 60;
    const int64_t hours = totalMinutes / 60;

    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(millis, 3, 10, QChar('0'));
}

// Formats a raw sample count with thousands separators, e.g. "1,234,567".
inline QString formatSampleCount(int64_t samples) {
    if (samples < 0) samples = 0;
    QString digits = QString::number(samples);
    for (int pos = digits.length() - 3; pos > 0; pos -= 3) {
        digits.insert(pos, QChar(','));
    }
    return digits;
}

// Parses a user-typed time into samples. Accepts "HH:MM:SS.mmm",
// "MM:SS.mmm", or plain seconds (e.g. "12.5"), always non-negative.
// Returns nullopt for anything else (garbage, empty, negative).
inline std::optional<int64_t> parseTimecode(const QString& text, unsigned sampleRate) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || sampleRate == 0) return std::nullopt;

    const QStringList parts = trimmed.split(':');
    if (parts.size() > 3) return std::nullopt;

    double hours = 0.0, minutes = 0.0, seconds = 0.0;
    bool ok = true;
    if (parts.size() == 3) {
        hours = parts[0].toDouble(&ok);
        if (!ok) return std::nullopt;
        minutes = parts[1].toDouble(&ok);
        if (!ok) return std::nullopt;
        seconds = parts[2].toDouble(&ok);
        if (!ok) return std::nullopt;
    } else if (parts.size() == 2) {
        minutes = parts[0].toDouble(&ok);
        if (!ok) return std::nullopt;
        seconds = parts[1].toDouble(&ok);
        if (!ok) return std::nullopt;
    } else {
        seconds = parts[0].toDouble(&ok);
        if (!ok) return std::nullopt;
    }

    if (hours < 0.0 || minutes < 0.0 || seconds < 0.0) return std::nullopt;

    const double totalSeconds = hours * 3600.0 + minutes * 60.0 + seconds;
    return static_cast<int64_t>(totalSeconds * static_cast<double>(sampleRate));
}

// Parses a user-typed raw sample count (comma grouping optional). Returns
// nullopt for anything else (garbage, empty, negative).
inline std::optional<int64_t> parseSampleCount(const QString& text) {
    QString digits = text.trimmed();
    digits.remove(',');
    if (digits.isEmpty()) return std::nullopt;

    bool ok = false;
    int64_t value = digits.toLongLong(&ok);
    if (!ok || value < 0) return std::nullopt;
    return value;
}

} // namespace rsd
