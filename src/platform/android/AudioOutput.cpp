#include "platform/android/AudioOutput.h"
#include <aaudio/AAudio.h>
#include <android/log.h>

namespace hotel {

static aaudio_data_callback_result_t onAudio(AAudioStream*, void* user, void* data, int32_t frames) {
    static_cast<Mixer*>(user)->render(static_cast<float*>(data), frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool AudioOutput::start(Mixer* mixer, int sampleRate) {
    stop();
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 1);
    AAudioStreamBuilder_setSampleRate(b, sampleRate);
    AAudioStreamBuilder_setDataCallback(b, onAudio, mixer);

    AAudioStream* s = nullptr;
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &s);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "HotelTycoon", "AAudio gagal: %s", AAudio_convertResultToText(r));
        return false;
    }
    stream_ = s;
    return AAudioStream_requestStart(s) == AAUDIO_OK;
}

void AudioOutput::stop() {
    if (!stream_) return;
    auto* s = static_cast<AAudioStream*>(stream_);
    AAudioStream_requestStop(s);
    AAudioStream_close(s);
    stream_ = nullptr;
}

void AudioOutput::pause()  { if (stream_) AAudioStream_requestPause(static_cast<AAudioStream*>(stream_)); }
void AudioOutput::resume() { if (stream_) AAudioStream_requestStart(static_cast<AAudioStream*>(stream_)); }

} // namespace hotel
