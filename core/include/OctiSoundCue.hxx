/*
 * OctiSoundCue.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_SOUND_CUE_HXX
#define OCTI_SOUND_CUE_HXX

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "OctiMove.hxx"

namespace octi {

enum class SoundCue {
    POD_PLACE      = 0,
    PRONG_SNAP     = 1,
    POD_STEP       = 2,
    POD_JUMP       = 3,
    POD_CAPTURE    = 4,
    CLOCK_LOW_TICK = 5,
    GAME_VICTORY   = 6,
};

constexpr int OCTI_SOUND_SAMPLE_RATE = 16000;
constexpr std::size_t OCTI_SOUND_CUE_COUNT = 7;

const char *soundCueName(SoundCue cue);
const char *soundCueAssetFileName(SoundCue cue);
bool soundCueFromName(const std::string &name, SoundCue &outCue);
bool soundCueFromAssetFileName(const std::string &fileName, SoundCue &outCue);

const std::uint8_t *soundCueWavBytes(SoundCue cue, std::size_t &outSize);

bool decodeWavPcm16Mono(const std::uint8_t *wavData,
                        std::size_t wavSize,
                        std::vector<std::int16_t> &outPcm,
                        int &outSampleRate);

SoundCue classifyMoveSoundCue(const OctiMove &move,
                              bool edgeless = false,
                              int boardWidth = 9,
                              int boardHeight = 9);

SoundCue classifyMoveNotationSoundCue(const std::string &notation,
                                      bool edgeless = false,
                                      int boardWidth = 9,
                                      int boardHeight = 9);

} // namespace octi

#endif // OCTI_SOUND_CUE_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
