#pragma once
#include <atomic>
#include "audio/SoundBank.h"

namespace hotel {

// Mixer mono. trigger() aman dipanggil dari thread game; render() dari thread audio (tanpa lock).
class Mixer {
public:
    // Panggil setelah bank selesai terisi; thread audio hanya membaca bank setelah ini.
    void setBank(const SoundBank* bank) { bank_.store(bank, std::memory_order_release); }
    void setVolume(float v) { volume_.store(v); }

    void trigger(Sfx s) { pending_[static_cast<int>(s)].fetch_add(1, std::memory_order_relaxed); }
    // Isi 'frames' sample float mono (menimpa isi buffer).
    void render(float* out, int frames);

private:
    static constexpr int kMaxVoices = 16;
    struct Voice { const std::vector<float>* data = nullptr; size_t pos = 0; float gain = 1; };

    std::atomic<const SoundBank*> bank_{nullptr};
    std::atomic<int> pending_[kSfxCount] = {};
    std::atomic<float> volume_{0.8f};
    Voice voices_[kMaxVoices];
};

} // namespace hotel
