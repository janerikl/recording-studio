#pragma once

#include "model/Clip.h"

namespace rsd {

// Gain multiplier for a sample `pos` samples into a fade region of length
// `len` (0 = at the fade's silent edge, len-1 = one sample before full
// volume). Pure math, safe to call from the realtime audio thread. A
// zero-length fade (the common case: no fade configured) is fully open.
float fadeMultiplier(int64_t pos, int64_t len, FadeCurve curve);

// Mixes one clip's contribution into `out` for this callback block, applying
// the clip's own gain/fades on top of the track's gainL/gainR. Shared by the
// live RT callback and offline export so both paths stay identical. Pure
// per-sample math over an already-snapshotted Clip — no locks, RT-safe.
void mixClipInto(float* out, unsigned int nFrames, unsigned int channels, int64_t playheadStart,
                  const Clip& clip, float gainL, float gainR);

} // namespace rsd
