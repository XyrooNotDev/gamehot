#pragma once
#include <cstdint>
#include <vector>

namespace hotel {

enum class Sfx { Bell, Footstep, Coin, Click, Place, Error, Count };
constexpr int kSfxCount = static_cast<int>(Sfx::Count);

// Nama file override di assets/sounds/ (jika ada, menggantikan suara sintetis).
const char* sfxFileName(Sfx s);

class SoundBank {
public:
    explicit SoundBank(int sampleRate = 44100) : sampleRate_(sampleRate) {}
    int sampleRate() const { return sampleRate_; }

    // Membuat semua efek secara sintetis (lonceng, langkah kaki, koin, klik, taruh, error).
    void synthesizeDefaults();
    // Ganti satu efek dengan file WAV (PCM16 / float32, mono / stereo, sample rate bebas).
    bool loadWav(Sfx s, const std::vector<uint8_t>& wav);

    const std::vector<float>& get(Sfx s) const { return samples_[static_cast<int>(s)]; }

private:
    int sampleRate_;
    std::vector<float> samples_[kSfxCount];
};

} // namespace hotel
