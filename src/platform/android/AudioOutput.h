#pragma once
#include "audio/Mixer.h"

namespace hotel {

// Output audio Android via AAudio (API 26+). Callback menarik data dari Mixer.
class AudioOutput {
public:
    ~AudioOutput() { stop(); }
    bool start(Mixer* mixer, int sampleRate);
    void stop();
    void pause();
    void resume();

private:
    void* stream_ = nullptr;   // AAudioStream*
};

} // namespace hotel
