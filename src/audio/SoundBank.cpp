#include "audio/SoundBank.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hotel {

const char* sfxFileName(Sfx s) {
    static const char* kNames[kSfxCount] = {"bell.wav", "footstep.wav", "coin.wav", "click.wav", "place.wav", "error.wav"};
    return kNames[static_cast<int>(s)];
}

namespace {
constexpr float kTwoPi = 6.28318530718f;

struct Noise {
    uint32_t s = 22222;
    float next() { s = s * 1664525u + 1013904223u; return ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f; }
};

std::vector<float> make(float seconds, int sr) { return std::vector<float>(static_cast<size_t>(seconds * sr), 0.0f); }
}

void SoundBank::synthesizeDefaults() {
    const int sr = sampleRate_;

    {   // Lonceng resepsionis: ding logam dengan partial tidak harmonis, meluruh ~1.2 dtk
        auto d = make(1.3f, sr);
        const float f0 = 1568.0f;
        const float ratio[4] = {1.0f, 2.76f, 5.4f, 8.9f}, amp[4] = {0.55f, 0.28f, 0.12f, 0.05f}, dec[4] = {3.2f, 5.0f, 8.0f, 12.0f};
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr, v = 0;
            for (int k = 0; k < 4; ++k) v += amp[k] * std::sin(kTwoPi * f0 * ratio[k] * t) * std::exp(-dec[k] * t);
            float attack = std::min(1.0f, t * 800.0f);
            d[i] = v * attack * 0.8f;
        }
        samples_[static_cast<int>(Sfx::Bell)] = std::move(d);
    }
    {   // Langkah kaki: noise low-pass singkat
        auto d = make(0.11f, sr);
        Noise n;
        float lp = 0;
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr;
            lp += 0.18f * (n.next() - lp);
            d[i] = lp * std::exp(-42.0f * t) * 1.6f;
        }
        samples_[static_cast<int>(Sfx::Footstep)] = std::move(d);
    }
    {   // Koin: dua nada naik, "bling"
        auto d = make(0.5f, sr);
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr;
            float f = t < 0.07f ? 988.0f : 1319.0f;
            float tt = t < 0.07f ? t : t - 0.07f;
            float env = std::exp(-(t < 0.07f ? 10.0f : 7.5f) * tt);
            float v = std::sin(kTwoPi * f * t) + 0.35f * std::sin(kTwoPi * f * 3.0f * t);
            d[i] = v * env * 0.35f * std::min(1.0f, t * 1500.0f);
        }
        samples_[static_cast<int>(Sfx::Coin)] = std::move(d);
    }
    {   // Klik UI
        auto d = make(0.05f, sr);
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr;
            d[i] = std::sin(kTwoPi * 1200.0f * t) * std::exp(-90.0f * t) * 0.4f;
        }
        samples_[static_cast<int>(Sfx::Click)] = std::move(d);
    }
    {   // Taruh furnitur: bunyi "duk"
        auto d = make(0.22f, sr);
        Noise n;
        float phase = 0;
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr;
            float f = 60.0f + 110.0f * std::exp(-30.0f * t);
            phase += kTwoPi * f / sr;
            d[i] = (std::sin(phase) * 0.8f + n.next() * 0.12f * std::exp(-60.0f * t)) * std::exp(-16.0f * t);
        }
        samples_[static_cast<int>(Sfx::Place)] = std::move(d);
    }
    {   // Error: dengung rendah
        auto d = make(0.28f, sr);
        for (size_t i = 0; i < d.size(); ++i) {
            float t = static_cast<float>(i) / sr;
            float sq = std::sin(kTwoPi * 140.0f * t) > 0 ? 1.0f : -1.0f;
            d[i] = sq * 0.22f * std::exp(-7.0f * t) * std::min(1.0f, t * 800.0f);
        }
        samples_[static_cast<int>(Sfx::Error)] = std::move(d);
    }
}

bool SoundBank::loadWav(Sfx s, const std::vector<uint8_t>& w) {
    auto u16 = [&](size_t o) { return static_cast<uint32_t>(w[o] | (w[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
    if (w.size() < 44 || std::memcmp(w.data(), "RIFF", 4) != 0 || std::memcmp(w.data() + 8, "WAVE", 4) != 0) return false;

    uint32_t fmt = 0, channels = 0, rate = 0, bits = 0;
    size_t dataOff = 0, dataLen = 0;
    size_t p = 12;
    while (p + 8 <= w.size()) {
        uint32_t len = u32(p + 4);
        if (std::memcmp(w.data() + p, "fmt ", 4) == 0 && p + 8 + 16 <= w.size()) {
            fmt = u16(p + 8); channels = u16(p + 10); rate = u32(p + 12); bits = u16(p + 22);
        } else if (std::memcmp(w.data() + p, "data", 4) == 0) {
            dataOff = p + 8;
            dataLen = std::min<size_t>(len, w.size() - dataOff);
            break;
        }
        p += 8 + len + (len & 1);
    }
    if (!dataOff || channels < 1 || rate == 0) return false;
    bool isFloat = (fmt == 3 && bits == 32), isPcm16 = (fmt == 1 && bits == 16);
    if (!isFloat && !isPcm16) return false;

    size_t bytesPer = bits / 8, frames = dataLen / (bytesPer * channels);
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0;
        for (uint32_t c = 0; c < channels; ++c) {
            size_t o = dataOff + (i * channels + c) * bytesPer;
            if (isFloat) { float f; std::memcpy(&f, &w[o], 4); sum += f; }
            else sum += static_cast<int16_t>(w[o] | (w[o + 1] << 8)) / 32768.0f;
        }
        mono[i] = sum / channels;
    }
    if (static_cast<int>(rate) != sampleRate_) {            // resample linear
        size_t outN = static_cast<size_t>(static_cast<double>(frames) * sampleRate_ / rate);
        std::vector<float> rs(outN);
        for (size_t i = 0; i < outN; ++i) {
            double src = static_cast<double>(i) * rate / sampleRate_;
            size_t i0 = static_cast<size_t>(src);
            size_t i1 = std::min(i0 + 1, frames - 1);
            float fr = static_cast<float>(src - i0);
            rs[i] = mono[i0] * (1 - fr) + mono[i1] * fr;
        }
        mono.swap(rs);
    }
    samples_[static_cast<int>(s)] = std::move(mono);
    return true;
}

} // namespace hotel
