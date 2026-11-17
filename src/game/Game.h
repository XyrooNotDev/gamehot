#pragma once
#include <string>
#include <vector>
#include "assets/AssetIO.h"
#include "audio/Mixer.h"
#include "audio/SoundBank.h"
#include "furniture/FurnitureManager.h"
#include "game/CameraController.h"
#include "game/Economy.h"
#include "game/Player.h"
#include "npc/GuestAI.h"
#include "renderer/Renderer.h"
#include "ui/TouchUI.h"
#include "world/HotelMap.h"

namespace hotel {

enum class GameMode { Play, Shop, Placing };

// Layar loading: Loading (aset dimuat bertahap, bar progres) -> FadeOut (memudar ke permainan) -> Done.
enum class LoadPhase { Loading, FadeOut, Done };

// Penghubung semua modul. Tidak tahu apa pun soal Android; platform/ yang memanggilnya.
// Siklus: initLogic() sekali -> createGraphics() tiap konteks GL dibuat -> update()/render() per frame
//         -> destroyGraphics() saat konteks hilang.
class Game {
public:
    enum Action {
        ActShop = 1, ActShopClose, ActCheckIn, ActSell, ActRotate, ActConfirm, ActCancel,
        ActBuyBase = 100      // ActBuyBase + id furnitur
    };

    void initLogic(AssetIO* io);
    void createGraphics(Renderer& r);
    void destroyGraphics(Renderer& r);

    void resize(int w, int h);
    void onTouch(TouchAction a, int pointerId, float x, float y) { if (phase_ != LoadPhase::Loading) ui_.onTouch(a, pointerId, x, y); }
    void update(float dt);
    void render(Renderer& r);

    // Simpan / muat (teks). loadState mengganti furnitur awal jika data valid.
    std::string saveState() const;
    bool loadState(const std::string& text);

    void handleAction(int actionId);

    Mixer& mixer() { return mixer_; }
    int sampleRate() const { return bank_.sampleRate(); }
    const SoundBank& soundBank() const { return bank_; }

    LoadPhase phase() const { return phase_; }
    bool loadingFinished() const { return phase_ == LoadPhase::Done; }
    float loadProgress() const { return loadShown_; }   // 0..1 yang tampil di bar

    // akses untuk tes
    GameMode mode() const { return mode_; }
    Economy& economy() { return eco_; }
    FurnitureManager& furniture() { return furn_; }
    GuestManager& guests() { return guests_; }
    HotelMap& map() { return map_; }
    Player& player() { return player_; }
    TouchUI& ui() { return ui_; }
    const std::string& toast() const { return toast_; }

private:
    void setupStarter();
    void say(const std::string& msg);
    void sfx(Sfx s) { mixer_.trigger(s); }
    void computeGhost();
    const PlacedFurniture* sellTarget() const;
    void buildLayout();
    void drawWorldOverlays(UIBatch& b, const Mat4& vp);
    void drawHud(UIBatch& b);

    // --- loading ---
    int loadTotalSteps() const;
    std::string loadLabel() const;
    void runLoadStep(Renderer& r);
    void drawSplash(UIBatch& b) const;

    AssetIO* io_ = nullptr;
    HotelMap map_;
    Economy eco_;
    FurnitureManager furn_;
    GuestManager guests_;
    Player player_;
    CameraController camera_;
    TouchUI ui_;
    UIBatch batch_;

    SoundBank bank_;
    Mixer mixer_;

    GameMode mode_ = GameMode::Play;
    int placingDef_ = -1, placingRot_ = 0;
    int ghostX_ = 0, ghostY_ = 0;
    bool ghostValid_ = false;
    std::string ghostReason_;

    std::string toast_;
    float toastTimer_ = 0;
    struct Floater { Vec3 pos; std::string text; float life; };
    std::vector<Floater> floaters_;
    float stepAccum_ = 0;
    Vec3 lastPos_;

    MeshHandle worldMesh_, playerMesh_, guestMesh_[4];
    std::vector<MeshHandle> itemMesh_;
    int width_ = 1280, height_ = 720;
    bool graphicsReady_ = false;

    LoadPhase phase_ = LoadPhase::Loading;
    int loadStep_ = 0;
    float loadTime_ = 0, loadMin_ = 1.6f, loadShown_ = 0, fade_ = 0;
    bool soundsReady_ = false, firstLoad_ = true;
};

} // namespace hotel
