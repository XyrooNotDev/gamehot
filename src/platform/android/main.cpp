// Entry point Android (NativeActivity + EGL + OpenGL ES 3.0 + AAudio).
#include <EGL/egl.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include "game/Game.h"
#include "platform/android/AndroidAssetIO.h"
#include "platform/android/AudioOutput.h"
#include "renderer/Renderer.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HotelTycoon", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "HotelTycoon", __VA_ARGS__)

using namespace hotel;

struct App {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    Renderer renderer;
    Game game;
    AudioOutput audio;
    AndroidAssetIO* assets = nullptr;
    std::string savePath;
    bool ready = false;
    int w = 0, h = 0;
    std::chrono::steady_clock::time_point last;
};

static void saveGame(App& s) {
    if (s.savePath.empty()) return;
    std::ofstream f(s.savePath, std::ios::trunc);
    if (f) f << s.game.saveState();
}

static void loadGame(App& s) {
    std::ifstream f(s.savePath);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();
    if (!s.game.loadState(ss.str())) LOGE("Data simpanan tidak valid, pakai awal baru");
}

static bool initEGL(App& s) {
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(s.display, nullptr, nullptr)) return false;
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig config;
    EGLint n = 0;
    if (!eglChooseConfig(s.display, attribs, &config, 1, &n) || n < 1) { LOGE("Tidak ada EGLConfig GLES3"); return false; }
    const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, ctx);
    s.surface = eglCreateWindowSurface(s.display, config, s.app->window, nullptr);
    if (s.context == EGL_NO_CONTEXT || s.surface == EGL_NO_SURFACE) return false;
    return eglMakeCurrent(s.display, s.surface, s.surface, s.context) == EGL_TRUE;
}

static void termEGL(App& s) {
    if (s.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
        eglTerminate(s.display);
    }
    s.display = EGL_NO_DISPLAY; s.surface = EGL_NO_SURFACE; s.context = EGL_NO_CONTEXT;
}

static void startGraphics(App& s) {
    if (!initEGL(s) || !s.renderer.init()) { LOGE("Init grafis gagal"); return; }
    s.game.createGraphics(s.renderer);
    s.w = s.h = 0;
    s.last = std::chrono::steady_clock::now();
    s.ready = true;
    s.audio.start(&s.game.mixer(), s.game.sampleRate());
    LOGI("Grafis siap");
}

static void stopGraphics(App& s) {
    s.audio.stop();
    if (s.ready) {
        s.game.destroyGraphics(s.renderer);
        s.renderer.shutdown();
    }
    termEGL(s);
    s.ready = false;
}

static void onCmd(android_app* app, int32_t cmd) {
    auto* s = static_cast<App*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: if (app->window) startGraphics(*s); break;
        case APP_CMD_TERM_WINDOW: stopGraphics(*s); break;
        case APP_CMD_PAUSE: saveGame(*s); break;
        case APP_CMD_SAVE_STATE: saveGame(*s); break;
        default: break;
    }
}

static int32_t onInput(android_app* app, AInputEvent* e) {
    auto* s = static_cast<App*>(app->userData);
    if (!s->ready || AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int32_t action = AMotionEvent_getAction(e);
    int32_t type = action & AMOTION_EVENT_ACTION_MASK;
    int32_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    size_t count = AMotionEvent_getPointerCount(e);

    auto send = [&](TouchAction a, size_t i) {
        s->game.onTouch(a, AMotionEvent_getPointerId(e, i), AMotionEvent_getX(e, i), AMotionEvent_getY(e, i));
    };
    switch (type) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: send(TouchAction::Down, idx); break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: send(TouchAction::Up, idx); break;
        case AMOTION_EVENT_ACTION_CANCEL: for (size_t i = 0; i < count; ++i) send(TouchAction::Cancel, i); break;
        case AMOTION_EVENT_ACTION_MOVE: for (size_t i = 0; i < count; ++i) send(TouchAction::Move, i); break;
        default: return 0;
    }
    return 1;
}

void android_main(android_app* app) {
    App state;
    state.app = app;
    app->userData = &state;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;

    AndroidAssetIO assets(app->activity->assetManager);
    state.assets = &assets;
    if (app->activity->internalDataPath) state.savePath = std::string(app->activity->internalDataPath) + "/hotel_save.txt";

    state.game.initLogic(&assets);     // logika + suara: sekali, tidak ikut reset saat konteks GL hilang
    loadGame(state);

    while (true) {
        int events;
        android_poll_source* source;
        int ident;
        while ((ident = ALooper_pollOnce(state.ready ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source))) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) { saveGame(state); stopGraphics(state); return; }
        }
        (void)ident;
        if (!state.ready) continue;

        EGLint w = 0, h = 0;
        eglQuerySurface(state.display, state.surface, EGL_WIDTH, &w);
        eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &h);
        if (w != state.w || h != state.h) {
            state.w = w; state.h = h;
            state.renderer.resize(w, h);
            state.game.resize(w, h);
        }

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - state.last).count();
        state.last = now;

        state.game.update(dt);
        state.game.render(state.renderer);
        eglSwapBuffers(state.display, state.surface);
    }
}
