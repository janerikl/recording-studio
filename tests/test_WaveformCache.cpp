#include <QTest>

#include "model/AudioBuffer.h"
#include "waveform/WaveformCache.h"

using rsd::AudioBuffer;
using rsd::WaveformCache;

namespace {

AudioBuffer makeBuffer(int channels, int64_t frameCount) {
    AudioBuffer buf;
    buf.channels = channels;
    buf.samples.resize(static_cast<size_t>(frameCount) * channels);
    for (int64_t f = 0; f < frameCount; ++f) {
        for (int ch = 0; ch < channels; ++ch) {
            // Deterministic, non-trivial waveform so min/max per block vary.
            double t = static_cast<double>(f) + ch * 0.37;
            buf.samples[static_cast<size_t>(f * channels + ch)] =
                static_cast<float>(std::sin(t * 0.05) * 0.8 + std::sin(t * 0.011) * 0.2);
        }
    }
    return buf;
}

} // namespace

class WaveformCacheTests : public QObject {
    Q_OBJECT

private slots:
    // Repeatedly extending block peaks in small increments (as new samples
    // trickle in every drain tick) must produce the exact same block array
    // as extending once over the whole, final buffer — the incremental path
    // is a performance optimization, not a different computation.
    void extendBlockPeaks_incrementalMatchesOneShot() {
        const int64_t blockFrames = 64;
        AudioBuffer full = makeBuffer(2, 2000);

        WaveformCache::BlockPeaks incremental;
        AudioBuffer growing;
        growing.channels = full.channels;
        const int64_t chunk = 137; // deliberately not a multiple of blockFrames
        int64_t framesSoFar = 0;
        while (framesSoFar < full.frameCount()) {
            int64_t take = std::min<int64_t>(chunk, full.frameCount() - framesSoFar);
            auto begin = full.samples.begin() + framesSoFar * full.channels;
            auto end = begin + take * full.channels;
            growing.samples.insert(growing.samples.end(), begin, end);
            framesSoFar += take;
            WaveformCache::extendBlockPeaks(growing, /*channel=*/0, blockFrames, incremental);
        }

        WaveformCache::BlockPeaks oneShot;
        WaveformCache::extendBlockPeaks(full, /*channel=*/0, blockFrames, oneShot);

        QCOMPARE(incremental.blocks.size(), oneShot.blocks.size());
        for (int i = 0; i < incremental.blocks.size(); ++i) {
            QCOMPARE(incremental.blocks[i].first, oneShot.blocks[i].first);
            QCOMPARE(incremental.blocks[i].second, oneShot.blocks[i].second);
        }
        QCOMPARE(incremental.framesDone, full.frameCount());
    }

    // Once a block is fully closed, appending more samples must not change
    // its recorded min/max — only the still-open tail block gets redone.
    void extendBlockPeaks_closedBlocksAreStable() {
        const int64_t blockFrames = 100;
        AudioBuffer buf = makeBuffer(1, 100);
        WaveformCache::BlockPeaks state;
        WaveformCache::extendBlockPeaks(buf, 0, blockFrames, state);
        QCOMPARE(state.blocks.size(), 1);
        auto firstBlockPeak = state.blocks[0];

        AudioBuffer grown = makeBuffer(1, 250);
        // Keep the first 100 frames identical to `buf` (already asserted
        // equal by construction: makeBuffer is deterministic per-frame).
        WaveformCache::extendBlockPeaks(grown, 0, blockFrames, state);
        QCOMPARE(state.blocks.size(), 3); // frames [0,100) [100,200) [200,250)
        QCOMPARE(state.blocks[0].first, firstBlockPeak.first);
        QCOMPARE(state.blocks[0].second, firstBlockPeak.second);
    }

    void extendBlockPeaks_emptyBufferProducesNoBlocks() {
        AudioBuffer buf = makeBuffer(1, 0);
        WaveformCache::BlockPeaks state;
        WaveformCache::extendBlockPeaks(buf, 0, 64, state);
        QCOMPARE(state.blocks.size(), 0);
        QCOMPARE(state.framesDone, static_cast<qint64>(0));
    }

    void downsampleBlockPeaks_combinesViaMinOfMinsMaxOfMaxes() {
        QVector<WaveformCache::PeakPair> blocks = {
            {-0.1f, 0.2f}, {-0.5f, 0.1f}, {-0.05f, 0.9f}, {-0.3f, 0.05f}};
        // 4 blocks -> 2 columns: column 0 covers blocks[0..1], column 1 covers blocks[2..3].
        auto peaks = WaveformCache::downsampleBlockPeaks(blocks, 2);
        QCOMPARE(peaks.size(), 2);
        QCOMPARE(peaks[0].first, -0.5f);
        QCOMPARE(peaks[0].second, 0.2f);
        QCOMPARE(peaks[1].first, -0.3f);
        QCOMPARE(peaks[1].second, 0.9f);
    }

    void downsampleBlockPeaks_emptyBlocksProducesZeroedColumns() {
        QVector<WaveformCache::PeakPair> blocks;
        auto peaks = WaveformCache::downsampleBlockPeaks(blocks, 5);
        QCOMPARE(peaks.size(), 5);
        for (auto& p : peaks) {
            QCOMPARE(p.first, 0.0f);
            QCOMPARE(p.second, 0.0f);
        }
    }
};

QTEST_MAIN(WaveformCacheTests)
#include "test_WaveformCache.moc"
