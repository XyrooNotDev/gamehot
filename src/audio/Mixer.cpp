#include "audio/Mixer.h"
#include <algorithm>

namespace hotel {

void Mixer::render(float* out, int frames) {
    std::fill(out, out + frames, 0.0f);
    const SoundBank* bank = bank_.load(std::memory_order_acquire);
    if (!bank) return;

    for (int s = 0; s < kSfxCount; ++s) {
        int n = pending_[s].exchange(0, std::memory_order_relaxed);
        if (n <= 0) continue;
        const auto& data = bank->get(static_cast<Sfx>(s));
        if (data.empty()) continue;
        for (auto& v : voices_) {
            if (!v.data) { v.data = &data; v.pos = 0; v.gain = 1.0f; break; }
        }
    }
    const float vol = volume_.load();
    for (auto& v : voices_) {
        if (!v.data) continue;
        const auto& d = *v.data;
        int n = static_cast<int>(std::min<size_t>(frames, d.size() - v.pos));
        for (int i = 0; i < n; ++i) out[i] += d[v.pos + i] * v.gain * vol;
        v.pos += n;
        if (v.pos >= d.size()) v.data = nullptr;
    }
    for (int i = 0; i < frames; ++i) out[i] = std::max(-1.0f, std::min(1.0f, out[i]));   // clip
}

} // namespace hotel
