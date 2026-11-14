/*
 * OctiSoundCue.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiSoundCue.hxx"
#include "OctiSoundAssets.hxx"
#include <cstring>

namespace octi {

namespace {

struct SoundCueDescriptor {
    SoundCue cue;
    const char *name;
    const char *fileName;
    const std::uint8_t *wavData;
    std::size_t wavSize;
};

const SoundCueDescriptor kDescriptors[OCTI_SOUND_CUE_COUNT] = {
    {
        SoundCue::POD_PLACE,
        "POD_PLACE",
        "pod_place.wav",
        kOctiWav_pod_place,
        kOctiWavSize_pod_place
    },
    {
        SoundCue::PRONG_SNAP,
        "PRONG_SNAP",
        "prong_snap.wav",
        kOctiWav_prong_snap,
        kOctiWavSize_prong_snap
    },
    {
        SoundCue::POD_STEP,
        "POD_STEP",
        "pod_step.wav",
        kOctiWav_pod_step,
        kOctiWavSize_pod_step
    },
    {
        SoundCue::POD_JUMP,
        "POD_JUMP",
        "pod_jump.wav",
        kOctiWav_pod_jump,
        kOctiWavSize_pod_jump
    },
    {
        SoundCue::POD_CAPTURE,
        "POD_CAPTURE",
        "pod_capture.wav",
        kOctiWav_pod_capture,
        kOctiWavSize_pod_capture
    },
    {
        SoundCue::CLOCK_LOW_TICK,
        "CLOCK_LOW_TICK",
        "clock_low_tick.wav",
        kOctiWav_clock_low_tick,
        kOctiWavSize_clock_low_tick
    },
    {
        SoundCue::GAME_VICTORY,
        "GAME_VICTORY",
        "game_victory.wav",
        kOctiWav_game_victory,
        kOctiWavSize_game_victory
    },
};

std::uint16_t readLe16(const std::uint8_t *p)
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t readLe32(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int64_t wrappedAbsDelta(int from, int to, bool edgeless, int span)
{
    std::int64_t d = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from);
    if (edgeless && span > 2) {
        std::int64_t s = static_cast<std::int64_t>(span);
        d %= s;
        if (d > s / 2) {
            d -= s;
        } else if (d < -(s / 2)) {
            d += s;
        }
    }
    return d < 0 ? -d : d;
}

SoundCue classifyOps(const std::vector<OctiMoveOp> &ops,
                     bool edgeless,
                     int boardWidth,
                     int boardHeight)
{
    if (ops.empty()) {
        return SoundCue::POD_PLACE;
    }

    for (const OctiMoveOp &op : ops) {
        for (const OctiHop &hop : op.hops) {
            if (hop.capture) {
                return SoundCue::POD_CAPTURE;
            }
        }
    }

    for (const OctiMoveOp &op : ops) {
        int curX = op.x0;
        int curY = op.y0;
        for (const OctiHop &hop : op.hops) {
            std::int64_t dx = wrappedAbsDelta(curX, hop.x, edgeless, boardWidth);
            std::int64_t dy = wrappedAbsDelta(curY, hop.y, edgeless, boardHeight);
            std::int64_t cheb = dx > dy ? dx : dy;
            if (cheb == 2) {
                return SoundCue::POD_JUMP;
            }
            curX = hop.x;
            curY = hop.y;
        }
    }

    bool hasRegStep = false;
    bool hasRegProng = false;
    bool hasRegPod = false;
    bool hasBonusStep = false;

    for (const OctiMoveOp &op : ops) {
        if (op.isSuperProngBonusMove) {
            if (!op.hops.empty()) {
                hasBonusStep = true;
            }
        } else {
            if (!op.hops.empty()) {
                hasRegStep = true;
            }
            if (op.addProngDir != DIR_UNDEF) {
                hasRegProng = true;
            }
            if (op.addPod) {
                hasRegPod = true;
            }
        }
    }

    if (hasRegStep) {
        return SoundCue::POD_STEP;
    }
    if (hasRegProng) {
        return SoundCue::PRONG_SNAP;
    }
    if (hasRegPod) {
        return SoundCue::POD_PLACE;
    }
    if (hasBonusStep) {
        return SoundCue::POD_STEP;
    }
    return SoundCue::POD_PLACE;
}

} // namespace

const char *soundCueName(SoundCue cue)
{
    int idx = static_cast<int>(cue);
    if (idx < 0 || static_cast<std::size_t>(idx) >= OCTI_SOUND_CUE_COUNT) {
        return "";
    }
    return kDescriptors[idx].name;
}

const char *soundCueAssetFileName(SoundCue cue)
{
    int idx = static_cast<int>(cue);
    if (idx < 0 || static_cast<std::size_t>(idx) >= OCTI_SOUND_CUE_COUNT) {
        return "";
    }
    return kDescriptors[idx].fileName;
}

bool soundCueFromName(const std::string &name, SoundCue &outCue)
{
    if (name.empty() || name.size() > 32u || name.find('\0') != std::string::npos) {
        return false;
    }
    if (name == "POD_DROP") {
        outCue = SoundCue::POD_PLACE;
        return true;
    }
    for (std::size_t i = 0; i < OCTI_SOUND_CUE_COUNT; ++i) {
        if (name == kDescriptors[i].name) {
            outCue = kDescriptors[i].cue;
            return true;
        }
    }
    return false;
}

bool soundCueFromAssetFileName(const std::string &fileName, SoundCue &outCue)
{
    if (fileName.empty() || fileName.size() > 32u ||
        fileName.find('\0') != std::string::npos ||
        fileName.find('/') != std::string::npos ||
        fileName.find('\\') != std::string::npos ||
        fileName.find("..") != std::string::npos) {
        return false;
    }
    for (char ch : fileName) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 0x20u || uch >= 0x7Fu) {
            return false;
        }
    }
    for (std::size_t i = 0; i < OCTI_SOUND_CUE_COUNT; ++i) {
        if (fileName == kDescriptors[i].fileName) {
            outCue = kDescriptors[i].cue;
            return true;
        }
    }
    return false;
}

const std::uint8_t *soundCueWavBytes(SoundCue cue, std::size_t &outSize)
{
    int idx = static_cast<int>(cue);
    if (idx < 0 || static_cast<std::size_t>(idx) >= OCTI_SOUND_CUE_COUNT) {
        outSize = 0;
        return nullptr;
    }
    outSize = kDescriptors[idx].wavSize;
    return kDescriptors[idx].wavSize > 0 ? kDescriptors[idx].wavData : nullptr;
}

bool decodeWavPcm16Mono(const std::uint8_t *wavData,
                        std::size_t wavSize,
                        std::vector<std::int16_t> &outPcm,
                        int &outSampleRate)
{
    outPcm.clear();
    outSampleRate = 0;

    if (wavData == nullptr || wavSize < 44u) {
        return false;
    }

    if (std::memcmp(&wavData[0], "RIFF", 4) != 0 ||
        std::memcmp(&wavData[8], "WAVE", 4) != 0) {
        return false;
    }

    const std::uint32_t riffSize = readLe32(&wavData[4]);
    if (riffSize < 36u || static_cast<std::size_t>(riffSize) > wavSize - 8u) {
        return false;
    }

    const std::size_t riffEnd = 8u + static_cast<std::size_t>(riffSize);
    std::size_t offset = 12u;
    bool seenFmt = false;
    bool seenData = false;
    int parsedSampleRate = 0;
    std::vector<std::int16_t> parsedPcm;

    while (offset < riffEnd) {
        if (riffEnd - offset < 8u) {
            return false;
        }

        const std::uint8_t *chunkId = &wavData[offset];
        const std::uint32_t chunkSize = readLe32(&wavData[offset + 4u]);
        const std::size_t payloadOffset = offset + 8u;
        if (static_cast<std::size_t>(chunkSize) > riffEnd - payloadOffset) {
            return false;
        }

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            if (seenFmt || chunkSize < 16u) {
                return false;
            }
            const std::uint16_t audioFormat = readLe16(&wavData[payloadOffset]);
            const std::uint16_t numChannels = readLe16(&wavData[payloadOffset + 2u]);
            const std::uint32_t sampleRate = readLe32(&wavData[payloadOffset + 4u]);
            const std::uint32_t byteRate = readLe32(&wavData[payloadOffset + 8u]);
            const std::uint16_t blockAlign = readLe16(&wavData[payloadOffset + 12u]);
            const std::uint16_t bitsPerSample = readLe16(&wavData[payloadOffset + 14u]);

            if (audioFormat != 1u ||
                numChannels != 1u ||
                bitsPerSample != 16u ||
                blockAlign != 2u ||
                sampleRate == 0u ||
                sampleRate > 0x3FFFFFFFu ||
                byteRate != sampleRate * 2u) {
                return false;
            }
            parsedSampleRate = static_cast<int>(sampleRate);
            seenFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            if (!seenFmt || seenData || (chunkSize & 1u) != 0u) {
                return false;
            }
            const std::size_t sampleCount = static_cast<std::size_t>(chunkSize) / 2u;
            parsedPcm.resize(sampleCount);
            for (std::size_t i = 0; i < sampleCount; ++i) {
                std::uint16_t u = readLe16(&wavData[payloadOffset + i * 2u]);
                std::int16_t s = 0;
                std::memcpy(&s, &u, sizeof(s));
                parsedPcm[i] = s;
            }
            seenData = true;
        }

        const std::size_t step = static_cast<std::size_t>(chunkSize);
        offset = payloadOffset + step;
        if ((step & 1u) != 0u && offset < riffEnd) {
            ++offset;
        }
    }

    if (!seenFmt || !seenData) {
        return false;
    }

    outSampleRate = parsedSampleRate;
    outPcm = std::move(parsedPcm);
    return true;
}

SoundCue classifyMoveSoundCue(const OctiMove &move,
                              bool edgeless,
                              int boardWidth,
                              int boardHeight)
{
    return classifyOps(move.getOps(), edgeless, boardWidth, boardHeight);
}

SoundCue classifyMoveNotationSoundCue(const std::string &notation,
                                      bool edgeless,
                                      int boardWidth,
                                      int boardHeight)
{
    if (notation.empty() || notation.find('\0') != std::string::npos) {
        return SoundCue::POD_PLACE;
    }
    std::vector<OctiMoveOp> ops;
    try {
        OctiMove::parseMoveString(notation, ops);
    } catch (...) {
        return SoundCue::POD_PLACE;
    }
    return classifyOps(ops, edgeless, boardWidth, boardHeight);
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
