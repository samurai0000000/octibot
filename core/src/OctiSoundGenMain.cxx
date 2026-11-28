/*
 * OctiSoundGenMain.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "OctiSoundCue.hxx"

namespace {

constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double TWO_PI = 2.0 * PI;

struct CueSpec {
    octi::SoundCue cue;
    const char *name;
    const char *fileName;
    int durationMs;
};

constexpr CueSpec kCueSpecs[octi::OCTI_SOUND_CUE_COUNT] = {
    { octi::SoundCue::POD_PLACE,         "POD_PLACE",         "pod_place.wav",         120 },
    { octi::SoundCue::PRONG_SNAP,        "PRONG_SNAP",        "prong_snap.wav",         75 },
    { octi::SoundCue::POD_STEP,          "POD_STEP",          "pod_step.wav",           95 },
    { octi::SoundCue::POD_JUMP,          "POD_JUMP",          "pod_jump.wav",          150 },
    { octi::SoundCue::POD_CAPTURE,       "POD_CAPTURE",       "pod_capture.wav",       210 },
    { octi::SoundCue::CLOCK_LOW_TICK,    "CLOCK_LOW_TICK",    "clock_low_tick.wav",     45 },
    { octi::SoundCue::GAME_WIN,          "GAME_WIN",          "game_win.wav",          560 },
    { octi::SoundCue::GAME_START,        "GAME_START",        "game_start.wav",        640 },
    { octi::SoundCue::GAME_DRAW,         "GAME_DRAW",         "game_draw.wav",         560 },
    { octi::SoundCue::GAME_LOSS,         "GAME_LOSS",         "game_loss.wav",         580 },
    { octi::SoundCue::PLAYER_ELIMINATED, "PLAYER_ELIMINATED", "player_eliminated.wav", 420 },
    { octi::SoundCue::NOTIFY_PING,       "NOTIFY_PING",       "notify_ping.wav",       240 },
};

std::int16_t clampPcm16(double sample)
{
    double clamped = std::max(-1.0, std::min(1.0, sample));
    return static_cast<std::int16_t>(std::floor(clamped * 30000.0 + 0.5));
}

struct ChimeNote {
    double freq;
    double gain;
};

struct ChimeEvent {
    double startSec;
    std::vector<ChimeNote> notes;
    double decayRate;
    double sheenGain;
    double attackMs;
};

double octiChimeVoice(double freq,
                      double t,
                      double decayRate,
                      double sheenGain,
                      double attackMs)
{
    const double sr = static_cast<double>(octi::OCTI_SOUND_SAMPLE_RATE);
    const double attackSamples = std::max(1.0, (attackMs / 1000.0) * sr);
    const double attack = std::min(1.0, (t * sr) / attackSamples);
    const double env = attack * std::exp(-t * decayRate);
    const double pingEnv = attack * std::exp(-t * (decayRate * 2.8));

    const double body =
        0.58 * std::sin(TWO_PI * freq * t) +
        0.26 * std::sin(TWO_PI * (freq * 1.5) * t) +
        0.16 * std::sin(TWO_PI * (freq * 2.0) * t);
    const double sheen = (sheenGain > 0.0)
        ? (sheenGain * pingEnv * std::sin(TWO_PI * (freq * 3.0) * t))
        : 0.0;
    return env * body + sheen;
}

std::vector<std::int16_t> renderChimeEvents(const std::vector<ChimeEvent> &events,
                                            int durationMs,
                                            double targetPeak,
                                            double overlapDamp,
                                            int fadeMs)
{
    const double sr = static_cast<double>(octi::OCTI_SOUND_SAMPLE_RATE);
    const int numSamples = (octi::OCTI_SOUND_SAMPLE_RATE * durationMs) / 1000;
    std::vector<double> buf(static_cast<std::size_t>(numSamples), 0.0);

    for (std::size_t idx = 0; idx < events.size(); ++idx) {
        const ChimeEvent &ev = events[idx];
        const int startSample =
            static_cast<int>(std::floor(ev.startSec * sr + 0.5));
        if (idx > 0) {
            for (int k = startSample; k < numSamples; ++k) {
                const double dt = static_cast<double>(k - startSample) / sr;
                buf[static_cast<std::size_t>(k)] *= std::exp(-dt * overlapDamp);
            }
        }
        for (int i = startSample; i < numSamples; ++i) {
            const double t = static_cast<double>(i - startSample) / sr;
            double val = 0.0;
            for (const ChimeNote &n : ev.notes) {
                val += n.gain * octiChimeVoice(n.freq, t, ev.decayRate,
                                               ev.sheenGain, ev.attackMs);
            }
            buf[static_cast<std::size_t>(i)] += val;
        }
    }

    const int fadeSamples = std::min(
        numSamples,
        (octi::OCTI_SOUND_SAMPLE_RATE * fadeMs) / 1000);
    for (int k = 0; k < fadeSamples; ++k) {
        const int idx = numSamples - 1 - k;
        buf[static_cast<std::size_t>(idx)] *=
            0.5 * (1.0 - std::cos(PI * (static_cast<double>(k) /
                                        static_cast<double>(fadeSamples))));
    }

    double peak = 0.0;
    for (double x : buf) {
        const double ax = std::abs(x);
        if (ax > peak) {
            peak = ax;
        }
    }
    if (peak <= 0.0) {
        peak = 1.0;
    }
    const double scale = targetPeak / peak;
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(numSamples), 0);
    for (int i = 0; i < numSamples; ++i) {
        const double scaled = std::max(
            -30000.0,
            std::min(30000.0, buf[static_cast<std::size_t>(i)] * scale));
        pcm[static_cast<std::size_t>(i)] =
            static_cast<std::int16_t>(std::floor(scaled + 0.5));
    }
    return pcm;
}

std::vector<std::int16_t> synthesizeCanonicalPcm(octi::SoundCue cue, int durationMs)
{
    switch (cue) {
    case octi::SoundCue::GAME_WIN:
        return renderChimeEvents(
            {
                { 0.000, { { 523.25, 0.80 } },                   11.0, 0.10, 3.0 },
                { 0.085, { { 659.25, 0.84 } },                   11.0, 0.10, 3.0 },
                { 0.170, { { 783.99, 0.88 } },                   11.0, 0.11, 3.0 },
                { 0.255, { { 1046.50, 0.90 }, { 523.25, 0.35 } }, 6.5, 0.12, 3.0 },
            },
            durationMs, 26000.0, 12.0, 90);
    case octi::SoundCue::GAME_START:
        return renderChimeEvents(
            {
                { 0.000, { { 783.99, 0.92 } }, 11.5, 0.12, 3.0 },
                { 0.160, { { 783.99, 0.90 } }, 11.5, 0.12, 3.0 },
                { 0.320, { { 783.99, 1.00 } },  7.2, 0.12, 3.0 },
            },
            durationMs, 26000.0, 12.0, 95);
    case octi::SoundCue::GAME_DRAW:
        return renderChimeEvents(
            {
                { 0.000, { { 783.99, 0.85 } },                   10.5, 0.08, 3.0 },
                { 0.180, { { 783.99, 0.65 }, { 523.25, 0.58 } },  7.2, 0.07, 3.0 },
            },
            durationMs, 26000.0, 10.0, 90);
    case octi::SoundCue::GAME_LOSS:
        return renderChimeEvents(
            {
                { 0.000, { { 659.25, 0.82 } },                   10.0, 0.04, 3.0 },
                { 0.125, { { 523.25, 0.80 } },                   10.0, 0.03, 3.0 },
                { 0.250, { { 440.00, 0.92 }, { 220.00, 0.25 } },  6.2, 0.02, 3.0 },
            },
            durationMs, 26000.0, 9.0, 90);
    case octi::SoundCue::PLAYER_ELIMINATED:
        return renderChimeEvents(
            {
                { 0.000, { { 293.66, 0.90 } },                   11.5, 0.01, 4.0 },
                { 0.130, { { 220.00, 1.00 }, { 329.63, 0.18 } },  7.8, 0.00, 4.0 },
            },
            durationMs, 24000.0, 10.0, 60);
    case octi::SoundCue::NOTIFY_PING:
        return renderChimeEvents(
            {
                { 0.000, { { 880.00, 1.00 } }, 14.5, 0.05, 5.5 },
            },
            durationMs, 19500.0, 11.0, 60);
    default:
        break;
    }

    const int numSamples = (octi::OCTI_SOUND_SAMPLE_RATE * durationMs) / 1000;
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(numSamples), 0);

    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) /
                   static_cast<double>(octi::OCTI_SOUND_SAMPLE_RATE);
        double progress = static_cast<double>(i) /
                          static_cast<double>(std::max(1, numSamples));
        double attack = std::min(1.0, static_cast<double>(i) / 48.0);
        double val = 0.0;

        switch (cue) {
        case octi::SoundCue::POD_PLACE: {
            double freq = 145.0 * std::exp(-t * 14.0) + 68.0;
            double env = attack * std::exp(-t * 24.0);
            val = env * (0.72 * std::sin(TWO_PI * freq * t) +
                         0.28 * std::sin(TWO_PI * (freq * 1.9) * t));
            break;
        }
        case octi::SoundCue::PRONG_SNAP: {
            double env = attack * std::exp(-t * 48.0);
            val = env * (0.50 * std::sin(TWO_PI * 1420.0 * t) +
                         0.35 * std::sin(TWO_PI * 2680.0 * t) +
                         0.15 * std::sin(TWO_PI * 3920.0 * t));
            break;
        }
        case octi::SoundCue::POD_STEP: {
            double freq = 195.0 * std::exp(-t * 16.0) + 85.0;
            double env = attack * std::exp(-t * 30.0);
            val = env * (0.75 * std::sin(TWO_PI * freq * t) +
                         0.25 * std::sin(TWO_PI * (freq * 2.1) * t));
            break;
        }
        case octi::SoundCue::POD_JUMP: {
            double freq = (progress < 0.45) ? 440.0 : 587.33;
            double env = attack * std::exp(-t * 14.0);
            val = env * (0.68 * std::sin(TWO_PI * freq * t) +
                         0.32 * std::sin(TWO_PI * (freq * 1.5) * t));
            break;
        }
        case octi::SoundCue::POD_CAPTURE: {
            double freq = (progress < 0.33) ? 440.0 :
                          (progress < 0.66) ? 554.37 : 659.25;
            double env = attack * std::exp(-t * 10.0);
            val = env * (0.65 * std::sin(TWO_PI * freq * t) +
                         0.25 * std::sin(TWO_PI * (freq * 2.0) * t) +
                         0.10 * std::sin(TWO_PI * (freq * 3.0) * t));
            break;
        }
        case octi::SoundCue::CLOCK_LOW_TICK: {
            double env = attack * std::exp(-t * 75.0);
            val = env * std::sin(TWO_PI * 1050.0 * t);
            break;
        }
        default:
            break;
        }

        pcm[static_cast<std::size_t>(i)] = clampPcm16(val);
    }

    return pcm;
}

void writeLe16(std::vector<std::uint8_t> &buf, std::size_t offset, std::uint16_t val)
{
    buf[offset] = static_cast<std::uint8_t>(val & 0xFFu);
    buf[offset + 1u] = static_cast<std::uint8_t>((val >> 8) & 0xFFu);
}

void writeLe32(std::vector<std::uint8_t> &buf, std::size_t offset, std::uint32_t val)
{
    buf[offset] = static_cast<std::uint8_t>(val & 0xFFu);
    buf[offset + 1u] = static_cast<std::uint8_t>((val >> 8) & 0xFFu);
    buf[offset + 2u] = static_cast<std::uint8_t>((val >> 16) & 0xFFu);
    buf[offset + 3u] = static_cast<std::uint8_t>((val >> 24) & 0xFFu);
}

std::vector<std::uint8_t> buildCanonicalWav(const CueSpec &spec)
{
    std::vector<std::int16_t> pcm = synthesizeCanonicalPcm(spec.cue, spec.durationMs);
    const std::uint32_t sampleRate =
        static_cast<std::uint32_t>(octi::OCTI_SOUND_SAMPLE_RATE);
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(pcm.size() * 2u);
    const std::uint32_t riffBytes = 36u + dataBytes;

    std::vector<std::uint8_t> wav(44u + static_cast<std::size_t>(dataBytes), 0);
    std::memcpy(&wav[0], "RIFF", 4);
    writeLe32(wav, 4, riffBytes);
    std::memcpy(&wav[8], "WAVE", 4);
    std::memcpy(&wav[12], "fmt ", 4);
    writeLe32(wav, 16, 16u);
    writeLe16(wav, 20, 1u); // PCM
    writeLe16(wav, 22, 1u); // mono
    writeLe32(wav, 24, sampleRate);
    writeLe32(wav, 28, sampleRate * 2u);
    writeLe16(wav, 32, 2u);
    writeLe16(wav, 34, 16u);
    std::memcpy(&wav[36], "data", 4);
    writeLe32(wav, 40, dataBytes);

    for (std::size_t i = 0; i < pcm.size(); ++i) {
        std::uint16_t u = 0;
        std::memcpy(&u, &pcm[i], sizeof(u));
        writeLe16(wav, 44u + i * 2u, u);
    }

    return wav;
}

bool atomicWriteBytes(const std::filesystem::path &target,
                      const void *data,
                      std::size_t size)
{
    if (target.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    std::filesystem::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Error: Failed to open temporary output: "
                      << tmp.string() << "\n";
            return false;
        }
        if (size > 0) {
            out.write(static_cast<const char *>(data),
                      static_cast<std::streamsize>(size));
        }
        out.flush();
        if (!out.good()) {
            std::cerr << "Error: Failed to write output: "
                      << target.string() << "\n";
            out.close();
            std::error_code rmEc;
            std::filesystem::remove(tmp, rmEc);
            return false;
        }
    }

    std::error_code renEc;
    std::filesystem::rename(tmp, target, renEc);
    if (renEc) {
        std::error_code cpEc;
        std::filesystem::copy_file(
            tmp, target, std::filesystem::copy_options::overwrite_existing, cpEc);
        std::error_code rmEc;
        std::filesystem::remove(tmp, rmEc);
        if (cpEc) {
            std::cerr << "Error: Failed to commit output: "
                      << target.string() << "\n";
            return false;
        }
    }
    return true;
}

bool readFileBytes(const std::filesystem::path &p, std::vector<std::uint8_t> &out)
{
    out.clear();
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamoff len = in.tellg();
    if (len < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(len));
    if (len > 0) {
        in.read(reinterpret_cast<char *>(out.data()), len);
        if (!in) {
            out.clear();
            return false;
        }
    }
    return true;
}

std::string encodeBase64(const std::vector<std::uint8_t> &data)
{
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2u) / 3u) * 4u);

    std::size_t i = 0;
    while (i + 3u <= data.size()) {
        std::uint32_t triple =
            (static_cast<std::uint32_t>(data[i]) << 16) |
            (static_cast<std::uint32_t>(data[i + 1u]) << 8) |
            static_cast<std::uint32_t>(data[i + 2u]);
        out.push_back(kAlphabet[(triple >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3Fu]);
        out.push_back(kAlphabet[(triple >> 6) & 0x3Fu]);
        out.push_back(kAlphabet[triple & 0x3Fu]);
        i += 3u;
    }

    std::size_t rem = data.size() - i;
    if (rem == 1u) {
        std::uint32_t triple = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kAlphabet[(triple >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3Fu]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2u) {
        std::uint32_t triple =
            (static_cast<std::uint32_t>(data[i]) << 16) |
            (static_cast<std::uint32_t>(data[i + 1u]) << 8);
        out.push_back(kAlphabet[(triple >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3Fu]);
        out.push_back(kAlphabet[(triple >> 6) & 0x3Fu]);
        out.push_back('=');
    }

    return out;
}

bool generateSoundAssetsDir(const std::string &outDir)
{
    std::filesystem::path dirPath(outDir);
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
        std::cerr << "Error: Failed to create sound asset directory '"
                  << outDir << "': " << ec.message() << "\n";
        return false;
    }

    for (const CueSpec &spec : kCueSpecs) {
        std::vector<std::uint8_t> wav = buildCanonicalWav(spec);
        std::filesystem::path filePath = dirPath / spec.fileName;
        if (!atomicWriteBytes(filePath, wav.data(), wav.size())) {
            return false;
        }
    }
    return true;
}

bool verifySoundAssetsDir(const std::string &verifyDir)
{
    std::filesystem::path dirPath(verifyDir);
    for (const CueSpec &spec : kCueSpecs) {
        std::vector<std::uint8_t> expected = buildCanonicalWav(spec);
        std::filesystem::path filePath = dirPath / spec.fileName;
        std::vector<std::uint8_t> actual;
        if (!readFileBytes(filePath, actual)) {
            std::cerr << "Error: Missing or unreadable sound asset: "
                      << filePath.string() << "\n";
            return false;
        }
        if (actual != expected) {
            std::cerr << "Error: Byte mismatch in sound asset '"
                      << filePath.string() << "' (expected "
                      << expected.size() << " bytes, got "
                      << actual.size() << " bytes)\n";
            return false;
        }
    }
    return true;
}

bool syncJsFileFromDir(const std::string &fromDir, const std::string &jsPath)
{
    std::filesystem::path dirPath(fromDir);
    std::vector<std::string> encodedWav(octi::OCTI_SOUND_CUE_COUNT);
    for (std::size_t i = 0; i < octi::OCTI_SOUND_CUE_COUNT; ++i) {
        std::filesystem::path wavPath = dirPath / kCueSpecs[i].fileName;
        std::vector<std::uint8_t> raw;
        if (!readFileBytes(wavPath, raw)) {
            std::cerr << "Error: Missing sound asset for JS sync: "
                      << wavPath.string() << "\n";
            return false;
        }
        if (raw.size() < 44u ||
            std::memcmp(&raw[0], "RIFF", 4) != 0 ||
            std::memcmp(&raw[8], "WAVE", 4) != 0) {
            std::cerr << "Error: Invalid RIFF/WAVE header in: "
                      << wavPath.string() << "\n";
            return false;
        }
        encodedWav[i] = encodeBase64(raw);
    }

    std::ifstream in(jsPath, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Failed to open target JS/C++ file: "
                  << jsPath << "\n";
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    std::string content = oss.str();
    in.close();

    const std::string beginMarker = "/* BEGIN_OCTI_SHARED_SOUND_ASSETS */";
    const std::string endMarker = "/* END_OCTI_SHARED_SOUND_ASSETS */";
    std::size_t beginPos = content.find(beginMarker);
    std::size_t endPos = content.find(endMarker);
    if (beginPos == std::string::npos || endPos == std::string::npos ||
        beginPos >= endPos) {
        std::cerr << "Error: Missing BEGIN/END_OCTI_SHARED_SOUND_ASSETS markers in: "
                  << jsPath << "\n";
        return false;
    }

    std::ostringstream block;
    block << beginMarker << "\n"
          << "var OCTI_SHARED_WAV_BASE64 = {\n";
    for (std::size_t i = 0; i < octi::OCTI_SOUND_CUE_COUNT; ++i) {
        block << "  " << kCueSpecs[i].name << ": \"" << encodedWav[i] << "\""
              << (i + 1u < octi::OCTI_SOUND_CUE_COUNT ? "," : "") << "\n";
    }
    block << "};\n"
          << endMarker;

    std::string updated = content.substr(0, beginPos) +
                          block.str() +
                          content.substr(endPos + endMarker.size());
    return atomicWriteBytes(jsPath, updated.data(), updated.size());
}

void printUsage(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [options]\n\n"
        << "Deterministic Canonical 16kHz PCM16 Sound Asset Generator & Web Sync Tool\n\n"
        << "Options:\n"
        << "  -h, --help             Show this help message and exit\n"
        << "  --out-dir <dir>        Generate all 12 canonical .wav files into <dir>\n"
        << "  --verify-dir <dir>     Verify all 12 .wav files in <dir> match canonical bytes\n"
        << "  --from-dir <dir>       Source directory of .wav files for --out-js\n"
        << "  --out-js <file>        Sync OCTI_SHARED_WAV_BASE64 block into <file>\n";
}

} // namespace

int main(int argc, char **argv)
{
    std::string outDir;
    std::string verifyDir;
    std::string fromDir;
    std::vector<std::string> outJsFiles;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--out-dir" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg == "--verify-dir" && i + 1 < argc) {
            verifyDir = argv[++i];
        } else if (arg == "--from-dir" && i + 1 < argc) {
            fromDir = argv[++i];
        } else if (arg == "--out-js" && i + 1 < argc) {
            outJsFiles.push_back(argv[++i]);
        } else {
            std::cerr << "Error: Unknown or incomplete option '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (outDir.empty() && verifyDir.empty() && outJsFiles.empty()) {
        std::cerr << "Error: No action specified (use --out-dir, --verify-dir, or --out-js)\n";
        printUsage(argv[0]);
        return 1;
    }

    if (!outDir.empty()) {
        if (!generateSoundAssetsDir(outDir)) {
            return 1;
        }
    }

    if (!verifyDir.empty()) {
        if (!verifySoundAssetsDir(verifyDir)) {
            return 1;
        }
    }

    if (!outJsFiles.empty()) {
        std::string srcDir = !fromDir.empty() ? fromDir : outDir;
        if (srcDir.empty()) {
            std::cerr << "Error: --out-js requires --from-dir <dir> (or --out-dir <dir>)\n";
            return 1;
        }
        for (const std::string &jsFile : outJsFiles) {
            if (!syncJsFileFromDir(srcDir, jsFile)) {
                return 1;
            }
        }
    }

    return 0;
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
