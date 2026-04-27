// ==========================================
// Source/GUI/AnalysisFifo.h
// ==========================================
#ifndef LUMINA_ANALYSISFIFO_H
#define LUMINA_ANALYSISFIFO_H

#include <juce_core/juce_core.h>
#include <array>

struct AnalysisFrame {
    bool isOnset = false;
    std::array<float, 24> barkPower{};
    std::array<float, 24> barkGainReduction{};

    std::array<float, 512> unprocessedSpectrum{}; // 原音(Pre)の波形用
    std::array<float, 512> magnitudeSpectrum{};   // 処理後(Post)の波形用
    std::array<float, 512> tameSpectrum{};        // Tameの削減量描画用

    int activeSoloBand = -1;
};

class AnalysisFifo {
public:
    AnalysisFifo() : abstractFifo(32) {}

    void push(const AnalysisFrame& frame) {
        auto writeHandle = abstractFifo.write(1);
        if (writeHandle.blockSize1 > 0) {
            buffer[static_cast<size_t>(writeHandle.startIndex1)] = frame;
        }
    }

    bool pop(AnalysisFrame& frame) {
        auto readHandle = abstractFifo.read(1);
        if (readHandle.blockSize1 > 0) {
            frame = buffer[static_cast<size_t>(readHandle.startIndex1)];
            return true;
        }
        return false;
    }

private:
    juce::AbstractFifo abstractFifo;
    std::array<AnalysisFrame, 32> buffer;
};

#endif // LUMINA_ANALYSISFIFO_H