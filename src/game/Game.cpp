#include "game/Game.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include "assets/AssetLoader.h"
#include "ui/Font.h"
#include "world/RoomBuilder.h"

namespace hotel {

namespace {
// Denah: lobi (kiri) + 3 kamar di sisi timur. 'E' = pintu masuk hotel.
const std::vector<std::string> kMapRows = {
    "##########################",
    "#.................#......#",
    "#.................D......#",
    "#.................#......#",
    "#.................########",
    "#.................#......#",
    "#..P..............D......#",
    "#.................#......#",
    "#.................########",
    "#.................#......#",
    "#.................D......#",
    "#.................#......#",
    "#.................########",
    "############E#############",
};

constexpr float kQuarter = 1.57079632679f;

float fitScale(const std::string& text, float maxW, float preferred) {
    float need = Font::textWidth(text, 1.0f);
    float s = std::min(preferred, maxW / std::max(1.0f, need));
    return std::max(1.0f, std::floor(s * 2.0f) / 2.0f);
}
}

// ---------------------------------------------------------------- logika

void Game::initLogic(AssetIO* io) {
    io_ = io;
    map_ = HotelMap::fromAscii(kMapRows, 1.0f);
    furn_.bind(&map_);
    furn_.clear();
    guests_.init(&map_, &furn_, &eco_);
    eco_ = Economy{};
    setupStarter();

    player_.position = map_.spawn();
    lastPos_ = player_.position;
    camera_.snapTo(player_.position + Vec3{0, 1, 0});
}

void Game::setupStarter() {
    furn_.place(2, 8, 10, 0);    // resepsionis di lobi, menghadap selatan
    furn_.place(0, 23, 1, 0);    // satu kasur di kamar pertama
}

std::string Game::saveState() const {
    std::ostringstream os;
    os << "HOTEL 1\nmoney " << eco_.money << "\nserved " << eco_.served << '\n' << furn_.serialize();
    return os.str();
}

bool Game::loadState(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.compare(0, 7, "HOTEL 1") != 0) return false;
    Economy e = eco_;
    std::vector<std::string> furnLines;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "money") ls >> e.money;
        else if (tag == "served") ls >> e.served;
        else if (tag == "F") furnLines.push_back(line);
    }
    furn_.clear();
    for (const auto& l : furnLines) furn_.loadLine(l);
    eco_ = e;
    guests_.reset();
    return true;
}

void Game::say(const std::string& msg) {
    toast_ = msg;
    toastTimer_ = 2.2f;
}

void Game::resize(int w, int h) {
    width_ = std::max(1, w);
    height_ = std::max(1, h);
    ui_.setScreenSize(static_cast<float>(width_), static_cast<float>(height_));
}

const PlacedFurniture* Game::sellTarget() const {
    float ts = map_.tileSize();
    Vec3 fwd{std::sin(player_.heading), 0, std::cos(player_.heading)};
    for (float d : {0.8f, 1.3f}) {
        Vec3 p = player_.position + fwd * (d * ts);
        IVec2 t = map_.tileAt(p);
        if (const PlacedFurniture* f = furn_.at(t.x, t.y)) return f;
    }
    return nullptr;
}

void Game::computeGhost() {
    const FurnitureDef* d = findFurniture(placingDef_);
    if (!d) { ghostValid_ = false; return; }
    int w, h;
    FurnitureManager::footprintSize(*d, placingRot_, w, h);
    float ts = map_.tileSize();
    Vec3 f = player_.position + Vec3{std::sin(player_.heading), 0, std::cos(player_.heading)} * (1.3f * ts);
    ghostX_ = static_cast<int>(std::floor(f.x / ts - w * 0.5f + 0.5f));
    ghostY_ = static_cast<int>(std::floor(f.z / ts - h * 0.5f + 0.5f));
    ghostValid_ = furn_.canPlace(placingDef_, ghostX_, ghostY_, placingRot_, map_.tileAt(player_.position), &ghostReason_);
}

void Game::handleAction(int id) {
    switch (id) {
    case ActShop:
        mode_ = GameMode::Shop; sfx(Sfx::Click); break;
    case ActShopClose:
        mode_ = GameMode::Play; sfx(Sfx::Click); break;
    case ActCheckIn: {
        std::string msg;
        bool ok = guests_.checkIn(player_.position, msg);
        say(msg);
        sfx(ok ? Sfx::Click : Sfx::Error);
        break;
    }
    case ActSell:
        if (const PlacedFurniture* f = sellTarget()) {
            const FurnitureDef* d = findFurniture(f->defId);
            int refund = d ? d->price / 2 : 0;
            if (furn_.remove(f->uid)) {
                eco_.earn(refund);
                say("TERJUAL +$" + std::to_string(refund));
                sfx(Sfx::Coin);
            }
        }
        break;
    case ActRotate:
        placingRot_ = (placingRot_ + 1) & 3; sfx(Sfx::Click); break;
    case ActCancel:
        mode_ = GameMode::Shop; sfx(Sfx::Click); break;
    case ActConfirm: {
        const FurnitureDef* d = findFurniture(placingDef_);
        if (!d) break;
        computeGhost();
        if (!ghostValid_) { say(ghostReason_); sfx(Sfx::Error); break; }
        if (!eco_.spend(d->price)) { say("UANG KURANG"); sfx(Sfx::Error); break; }
        furn_.place(placingDef_, ghostX_, ghostY_, placingRot_);
        sfx(Sfx::Place);
        say(std::string(d->name) + " DIPASANG");
        mode_ = GameMode::Play;
        break;
    }
    default:
        if (id >= ActBuyBase) {
            const FurnitureDef* d = findFurniture(id - ActBuyBase);
            if (!d) break;
            if (!eco_.canAfford(d->price)) { say("UANG KURANG"); sfx(Sfx::Error); break; }
            placingDef_ = d->id;
            placingRot_ = 0;
            mode_ = GameMode::Placing;
            sfx(Sfx::Click);
        }
    }
}

void Game::update(float dt) {
    dt = std::min(dt, 0.05f);

    if (phase_ != LoadPhase::Done) {
        loadTime_ += dt;
        float target = static_cast<float>(loadStep_) / static_cast<float>(loadTotalSteps());
        loadShown_ = std::min(target, loadShown_ + 1.4f * dt);           // bar bergerak halus
        if (phase_ == LoadPhase::Loading && graphicsReady_ && loadShown_ >= 1.0f && loadTime_ >= loadMin_) {
            phase_ = LoadPhase::FadeOut;
            fade_ = 0;
        } else if (phase_ == LoadPhase::FadeOut) {
            fade_ += dt / 0.5f;
            if (fade_ >= 1.0f) { fade_ = 1.0f; phase_ = LoadPhase::Done; }
        }
        if (phase_ == LoadPhase::Loading) return;       // permainan belum mulai
    }

    for (int id : ui_.consumeClicks()) handleAction(id);

    Vec2 drag = ui_.consumeCameraDelta();
    camera_.orbit(-drag.x * 0.005f, drag.y * 0.005f);
    player_.update(ui_.moveAxis(), camera_.yaw(), dt, map_);
    camera_.follow(player_.position + Vec3{0, 1, 0}, dt);

    // langkah kaki tiap ~0.9 m
    float dx = player_.position.x - lastPos_.x, dz = player_.position.z - lastPos_.z;
    stepAccum_ += std::sqrt(dx * dx + dz * dz);
    lastPos_ = player_.position;
    if (stepAccum_ >= 0.9f) { stepAccum_ = 0; sfx(Sfx::Footstep); }

    guests_.update(dt);
    for (const GameEvent& e : guests_.takeEvents()) {
        switch (e.type) {
        case GameEvent::GuestArrived: sfx(Sfx::Bell); break;
        case GameEvent::CheckedIn: break;
        case GameEvent::NoRoom: break;
        case GameEvent::Paid:
            sfx(Sfx::Coin);
            floaters_.push_back({e.pos, "+$" + std::to_string(e.amount), 1.6f});
            break;
        case GameEvent::GuestLeftAngry: sfx(Sfx::Error); say("TAMU PERGI MARAH"); break;
        }
    }
    for (auto& f : floaters_) f.life -= dt;
    floaters_.erase(std::remove_if(floaters_.begin(), floaters_.end(), [](const Floater& f) { return f.life <= 0; }), floaters_.end());

    if (toastTimer_ > 0) toastTimer_ -= dt;
    if (mode_ == GameMode::Placing) computeGhost();
    buildLayout();
}

// ---------------------------------------------------------------- UI layout

void Game::buildLayout() {
    const float W = static_cast<float>(width_), H = static_cast<float>(height_);
    const float s = H / 720.0f;
    const float fs = std::max(2.0f, std::round(2.5f * s));
    std::vector<UIButton> btns;
    std::vector<UIRect> panels;

    auto addBtn = [&](int id, float x, float y, float w, float h, const std::string& label, Color c, float scale = 0) {
        UIButton b;
        b.id = id; b.rect = {x, y, w, h}; b.label = label; b.color = c;
        b.textScale = scale > 0 ? scale : fitScale(label, w * 0.9f, fs);
        btns.push_back(b);
    };

    const float bw = 170 * s, bh = 62 * s, mx = 18 * s;
    if (mode_ == GameMode::Play) {
        float y = H - bh - mx;
        addBtn(ActShop, W - bw - mx, y, bw, bh, "BELI", {0.85f, 0.55f, 0.10f, 0.92f});
        y -= bh + 12 * s;
        if (guests_.canCheckIn(player_.position)) {
            addBtn(ActCheckIn, W - bw - mx, y, bw, bh, "CHECK-IN", {0.20f, 0.65f, 0.30f, 0.92f});
            y -= bh + 12 * s;
        }
        if (const PlacedFurniture* t = sellTarget()) {
            const FurnitureDef* d = findFurniture(t->defId);
            UIButton b;
            b.id = ActSell; b.rect = {W - bw - mx, y, bw, bh};
            b.label = "JUAL"; b.sub = "+$" + std::to_string(d ? d->price / 2 : 0);
            b.color = {0.75f, 0.25f, 0.25f, 0.92f};
            b.textScale = fs * 0.85f;
            btns.push_back(b);
        }
    } else if (mode_ == GameMode::Shop) {
        float px = W * 0.34f, py = 40 * s, pw = W - px - 16 * s, ph = H - 80 * s;
        panels.push_back({px, py, pw, ph});
        const float m = 14 * s;
        addBtn(ActShopClose, px + pw - 130 * s - m, py + m, 130 * s, 50 * s, "TUTUP", {0.70f, 0.25f, 0.25f, 0.95f});

        const auto& cat = furnitureCatalog();
        const int cols = 4;
        float cw = (pw - m * (cols + 1)) / cols, ch = 140 * s;
        for (size_t i = 0; i < cat.size(); ++i) {
            int c = static_cast<int>(i) % cols, r = static_cast<int>(i) / cols;
            UIButton b;
            b.id = ActBuyBase + cat[i].id;
            b.rect = {px + m + c * (cw + m), py + 84 * s + r * (ch + m), cw, ch};
            b.label = cat[i].name;
            b.sub = "$" + std::to_string(cat[i].price);
            bool ok = eco_.canAfford(cat[i].price);
            b.color = ok ? Color{0.18f, 0.40f, 0.62f, 0.95f} : Color{0.45f, 0.22f, 0.22f, 0.95f};
            b.textScale = fitScale(b.label, cw * 0.88f, fs);
            btns.push_back(b);
        }
    } else {  // Placing
        float y = H - bh - mx;
        addBtn(ActConfirm, W - bw - mx, y, bw, bh, "OKE", ghostValid_ ? Color{0.20f, 0.65f, 0.30f, 0.95f} : Color{0.40f, 0.40f, 0.42f, 0.95f});
        y -= bh + 12 * s;
        addBtn(ActRotate, W - bw - mx, y, bw, bh, "PUTAR", {0.25f, 0.45f, 0.80f, 0.92f});
        y -= bh + 12 * s;
        addBtn(ActCancel, W - bw - mx, y, bw, bh, "BATAL", {0.70f, 0.25f, 0.25f, 0.92f});
    }
    ui_.setLayout(std::move(btns), std::move(panels));
}

// ---------------------------------------------------------------- grafis

// createGraphics hanya memulai loading; aset dimuat satu langkah per frame (runLoadStep)
// supaya layar loading bisa tampil dan bar progres bergerak.
void Game::createGraphics(Renderer&) {
    graphicsReady_ = false;
    itemMesh_.clear();
    phase_ = LoadPhase::Loading;
    loadStep_ = 0;
    loadTime_ = 0;
    loadShown_ = 0;
    fade_ = 0;
    loadMin_ = firstLoad_ ? 1.6f : 0.25f;     // masuk pertama: layar pembuka; setelah konteks GL pulih: singkat
}

void Game::destroyGraphics(Renderer& r) {
    r.destroyMesh(worldMesh_);
    r.destroyMesh(playerMesh_);
    for (auto& g : guestMesh_) r.destroyMesh(g);
    for (auto& m : itemMesh_) r.destroyMesh(m);
    itemMesh_.clear();
    graphicsReady_ = false;
}

int Game::loadTotalSteps() const { return 4 + static_cast<int>(furnitureCatalog().size()); }

std::string Game::loadLabel() const {
    const int nf = static_cast<int>(furnitureCatalog().size());
    if (loadStep_ >= loadTotalSteps()) return "SIAP";
    switch (loadStep_) {
        case 0: return "MEMUAT SUARA";
        case 1: return "MEMBANGUN LOBI DAN KAMAR";
        case 2: return "MENYIAPKAN KARAKTER";
        case 3: return "MENYIAPKAN TAMU";
        default: return "MEMUAT FURNITUR " + std::to_string(loadStep_ - 3) + "/" + std::to_string(nf);
    }
}

void Game::runLoadStep(Renderer& r) {
    if (graphicsReady_ || loadStep_ >= loadTotalSteps()) { graphicsReady_ = true; return; }
    AssetLoader loader(io_);
    auto characterMesh = [&](const char* name, Color shirt) {
        MeshData m;
        if (io_ && loader.loadModel(std::string("models/") + name, m)) AssetLoader::fit(m, 1, 1, 1.35f);
        else m = Player::buildMesh(shirt);
        return r.createMesh(m);
    };

    switch (loadStep_) {
    case 0:
        if (!soundsReady_) {
            bank_.synthesizeDefaults();
            if (io_) {                           // file WAV di assets/sounds/ menggantikan suara sintetis
                for (int i = 0; i < kSfxCount; ++i) {
                    std::vector<uint8_t> bytes;
                    if (io_->read(std::string("sounds/") + sfxFileName(static_cast<Sfx>(i)), bytes))
                        bank_.loadWav(static_cast<Sfx>(i), bytes);
                }
            }
            mixer_.setBank(&bank_);
            soundsReady_ = true;
        }
        break;
    case 1:
        worldMesh_ = r.createMesh(RoomBuilder::build(map_));
        break;
    case 2:
        playerMesh_ = characterMesh("player", {0.20f, 0.35f, 0.75f, 1});
        break;
    case 3: {
        const Color shirts[4] = {{0.80f, 0.30f, 0.30f, 1}, {0.30f, 0.65f, 0.40f, 1}, {0.85f, 0.65f, 0.20f, 1}, {0.60f, 0.35f, 0.70f, 1}};
        for (int i = 0; i < 4; ++i) guestMesh_[i] = characterMesh("guest", shirts[i]);
        break;
    }
    default: {
        const FurnitureDef& d = furnitureCatalog()[loadStep_ - 4];
        MeshData m;
        if (io_ && loader.loadModel(std::string("models/") + d.modelName, m)) AssetLoader::fit(m, static_cast<float>(d.w), static_cast<float>(d.h));
        else m = buildProceduralFurniture(d);
        itemMesh_.push_back(r.createMesh(m));
    }
    }
    ++loadStep_;
    if (loadStep_ >= loadTotalSteps()) { graphicsReady_ = true; firstLoad_ = false; }
}

void Game::render(Renderer& r) {
    if (phase_ == LoadPhase::Loading) {
        r.beginFrame(0.06f, 0.09f, 0.20f);
        batch_.clear();
        drawSplash(batch_);
        r.drawUI(batch_);
        runLoadStep(r);                  // kerjakan satu langkah SETELAH frame digambar
        return;
    }
    if (!graphicsReady_) return;
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    Mat4 vp = camera_.projection(aspect) * camera_.view();

    r.beginFrame(0.53f, 0.78f, 0.92f);
    r.drawMesh(worldMesh_, Mat4::identity(), vp);

    for (const auto& p : furn_.items()) {
        if (p.defId < 0 || p.defId >= static_cast<int>(itemMesh_.size())) continue;
        Mat4 model = Mat4::translation(furn_.centerOf(p)) * Mat4::rotationY(p.rot * kQuarter);
        r.drawMesh(itemMesh_[p.defId], model, vp);
    }

    r.drawMesh(playerMesh_, player_.modelMatrix(), vp);
    for (const Guest& g : guests_.guests()) {
        if (g.state == GuestState::Staying) continue;                 // sedang tidur di kasur
        Mat4 model = Mat4::translation(g.pos) * Mat4::rotationY(g.heading);
        r.drawMesh(guestMesh_[g.variant & 3], model, vp);
    }

    if (mode_ == GameMode::Placing && placingDef_ >= 0 && placingDef_ < static_cast<int>(itemMesh_.size())) {
        const FurnitureDef* d = findFurniture(placingDef_);
        int w, h;
        FurnitureManager::footprintSize(*d, placingRot_, w, h);
        float ts = map_.tileSize();
        Vec3 c{(ghostX_ + w * 0.5f) * ts, 0.02f, (ghostY_ + h * 0.5f) * ts};
        Color tint = ghostValid_ ? Color{0.5f, 1.0f, 0.5f, 0.65f} : Color{1.0f, 0.4f, 0.4f, 0.65f};
        r.drawMesh(itemMesh_[placingDef_], Mat4::translation(c) * Mat4::rotationY(placingRot_ * kQuarter), vp, tint);
    }

    batch_.clear();
    drawWorldOverlays(batch_, vp);
    ui_.draw(batch_);
    drawHud(batch_);
    if (phase_ == LoadPhase::FadeOut)       // memudar dari layar loading ke permainan
        batch_.addRect(0, 0, static_cast<float>(width_), static_cast<float>(height_), {0.06f, 0.09f, 0.20f, 1.0f - fade_});
    r.drawUI(batch_);
}

void Game::drawSplash(UIBatch& b) const {
    const float W = static_cast<float>(width_), H = static_cast<float>(height_);
    const float s = H / 720.0f, fs = std::max(2.0f, std::round(2.5f * s));
    const float t = loadTime_;

    // latar gradasi langit malam
    const int bands = 18;
    for (int i = 0; i < bands; ++i) {
        float k = static_cast<float>(i) / (bands - 1);
        b.addRect(0, H * i / bands, W, H / bands + 1.0f, {0.06f + 0.10f * k, 0.09f + 0.16f * k, 0.20f + 0.20f * k, 1});
    }
    // bintang berkedip
    for (int i = 0; i < 28; ++i) {
        float sx = std::fmod(i * 0.6180339f * 7.0f, 1.0f) * W, sy = std::fmod(i * 0.3819660f * 5.0f, 1.0f) * H * 0.55f;
        float tw = 0.45f + 0.45f * std::sin(t * 2.0f + i * 1.3f);
        b.addRect(sx, sy, 2.5f * s, 2.5f * s, {1, 1, 1, tw});
    }

    // ikon hotel: gedung + jendela menyala bergantian + pintu + papan
    const float cx = W * 0.5f, bw = 170 * s, bh = 200 * s, by = H * 0.50f - bh;
    b.addRect(cx - bw / 2 - 8 * s, by - 34 * s, bw + 16 * s, 34 * s, {0.85f, 0.35f, 0.25f, 1});         // atap / papan
    b.addTextCentered(cx, by - 17 * s, fs * 1.1f, "HOTEL", {1, 0.95f, 0.8f, 1});
    b.addRect(cx - bw / 2, by, bw, bh, {0.86f, 0.88f, 0.95f, 1});
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) {
            int n = row * 3 + col;
            bool lit = std::sin(t * 2.4f + n * 1.9f) > -0.25f;
            Color c = lit ? Color{1.0f, 0.85f, 0.35f, 1} : Color{0.35f, 0.42f, 0.58f, 1};
            b.addRect(cx - bw / 2 + 22 * s + col * 50 * s, by + 16 * s + row * 36 * s, 30 * s, 22 * s, c);
        }
    b.addRect(cx - 18 * s, by + bh - 34 * s, 36 * s, 34 * s, {0.45f, 0.30f, 0.20f, 1});                    // pintu
    b.addRect(cx - bw / 2 - 20 * s, by + bh, bw + 40 * s, 8 * s, {0.25f, 0.30f, 0.40f, 1});               // tanah

    // judul
    const float ts = std::max(4.0f, std::round(6.0f * s));
    b.addTextCentered(cx + 3 * s, H * 0.62f + 3 * s, ts, "HOTEL TYCOON", {0, 0, 0, 0.45f});
    b.addTextCentered(cx, H * 0.62f, ts, "HOTEL TYCOON", {1.0f, 0.88f, 0.40f, 1});

    // bar progres
    const float pw = std::min(W * 0.55f, 640.0f * s), ph = 20 * s, px = cx - pw / 2, py = H * 0.74f;
    float p = std::min(1.0f, std::max(0.0f, loadShown_));
    b.addRect(px - 4 * s, py - 4 * s, pw + 8 * s, ph + 8 * s, {1, 1, 1, 0.85f});
    b.addRect(px, py, pw, ph, {0.10f, 0.14f, 0.26f, 1});
    b.addRect(px, py, pw * p, ph, {0.95f, 0.72f, 0.20f, 1});
    b.addRect(px, py, pw * p, ph * 0.4f, {1.0f, 0.88f, 0.45f, 1});                                         // kilau
    int dots = static_cast<int>(t * 3.0f) % 4;
    b.addTextCentered(cx, py + ph + 30 * s, fs, loadLabel() + std::string(static_cast<size_t>(dots), '.'), {0.85f, 0.90f, 1.0f, 1});
    std::string pct = std::to_string(static_cast<int>(p * 100.0f + 0.5f)) + "%";
    b.addText(px + pw - Font::textWidth(pct, fs), py - 12 * s - Font::textHeight(fs), fs, pct, {1, 1, 1, 0.9f});

    // tips bergantian
    static const char* kTips[] = {
        "TIPS: DEKATI MEJA RESEPSI LALU TEKAN CHECK-IN",
        "TIPS: DEKORASI KAMAR MENAIKKAN PENDAPATAN",
        "TIPS: TAMU PERGI MARAH JIKA TERLALU LAMA MENUNGGU",
        "TIPS: GESER LAYAR KANAN UNTUK MEMUTAR KAMERA",
    };
    b.addTextCentered(cx, H * 0.93f, fs * 0.8f, kTips[static_cast<int>(t / 2.0f) % 4], {0.75f, 0.80f, 0.92f, 0.9f});
}

void Game::drawWorldOverlays(UIBatch& b, const Mat4& vp) {
    const float s = height_ / 720.0f, fs = std::max(2.0f, std::round(2.0f * s));
    Vec2 sp;
    for (const Guest& g : guests_.guests()) {
        if (g.state == GuestState::Waiting && projectToScreen(g.pos + Vec3{0, 1.75f, 0}, vp, (float)width_, (float)height_, sp)) {
            float bw = 64 * s, bh = 8 * s, frac = std::max(0.0f, g.patience / g.patienceMax);
            b.addRect(sp.x - bw / 2 - 2, sp.y - 2, bw + 4, bh + 4, {0, 0, 0, 0.6f});
            b.addRect(sp.x - bw / 2, sp.y, bw * frac, bh, {1.0f - frac, 0.3f + 0.6f * frac, 0.2f, 1});
            b.addTextCentered(sp.x, sp.y - 16 * s, fs * 1.5f, "!", {1.0f, 0.9f, 0.2f, 1});
        } else if (g.state == GuestState::Staying) {
            const PlacedFurniture* bed = furn_.byUid(g.bedUid);
            if (bed && projectToScreen(furn_.centerOf(*bed) + Vec3{0, 1.3f, 0}, vp, (float)width_, (float)height_, sp))
                b.addTextCentered(sp.x, sp.y, fs, "ZZZ", {0.8f, 0.9f, 1.0f, 0.95f});
        }
    }
    for (const auto& f : floaters_) {
        float rise = (1.6f - f.life) * 0.8f;
        if (projectToScreen(f.pos + Vec3{0, 1.8f + rise, 0}, vp, (float)width_, (float)height_, sp))
            b.addTextCentered(sp.x, sp.y, fs * 1.5f, f.text, {1.0f, 0.9f, 0.3f, std::min(1.0f, f.life * 2)});
    }
}

void Game::drawHud(UIBatch& b) {
    const float W = static_cast<float>(width_), H = static_cast<float>(height_);
    const float s = H / 720.0f, fs = std::max(2.0f, std::round(2.5f * s));

    // uang (ikon koin + angka)
    std::string money = "$" + std::to_string(eco_.money);
    float pw = 52 * s + Font::textWidth(money, fs) + 20 * s, ph = 48 * s;
    b.addRect(12 * s, 12 * s, pw, ph, {0.05f, 0.07f, 0.12f, 0.75f});
    b.addCircle(12 * s + 26 * s, 12 * s + ph / 2, 17 * s, {0.95f, 0.78f, 0.15f, 1});
    b.addRing(12 * s + 26 * s, 12 * s + ph / 2, 17 * s, 3 * s, {0.75f, 0.55f, 0.05f, 1});
    b.addTextCentered(12 * s + 26 * s, 12 * s + ph / 2, fs * 0.9f, "$", {0.6f, 0.4f, 0.0f, 1});
    b.addText(12 * s + 52 * s, 12 * s + ph / 2 - Font::textHeight(fs) / 2, fs, money, {1, 1, 1, 1});

    std::string info = "TAMU: " + std::to_string(eco_.served);
    b.addText(14 * s, 12 * s + ph + 8 * s, fs * 0.8f, info, {1, 1, 1, 0.9f});

    if (mode_ == GameMode::Play && !furn_.hasType(FurnType::Reception))
        b.addTextCentered(W * 0.5f, H - 40 * s, fs, "BELI MEJA RESEPSI AGAR TAMU DATANG", {1, 0.9f, 0.4f, 1});

    if (mode_ == GameMode::Placing) {
        const FurnitureDef* d = findFurniture(placingDef_);
        if (d) {
            std::string t = std::string(d->name) + "  $" + std::to_string(d->price);
            b.addRect(W * 0.5f - Font::textWidth(t, fs) / 2 - 14 * s, 14 * s, Font::textWidth(t, fs) + 28 * s, 44 * s, {0.05f, 0.07f, 0.12f, 0.75f});
            b.addTextCentered(W * 0.5f, 14 * s + 22 * s, fs, t, {1, 1, 1, 1});
            if (!ghostValid_)
                b.addTextCentered(W * 0.5f, 72 * s, fs * 0.9f, ghostReason_, {1.0f, 0.5f, 0.5f, 1});
        }
    } else if (mode_ == GameMode::Shop) {
        float px = W * 0.34f, py = 40 * s;
        b.addText(px + 20 * s, py + 26 * s, fs * 1.2f, "TOKO FURNITUR", {1, 1, 1, 1});
    }

    if (toastTimer_ > 0 && !toast_.empty()) {
        float a = std::min(1.0f, toastTimer_ * 2.0f);
        float tw = Font::textWidth(toast_, fs);
        float y = (mode_ == GameMode::Placing) ? 110 * s : 40 * s;
        b.addRect(W * 0.5f - tw / 2 - 16 * s, y - 6 * s, tw + 32 * s, Font::textHeight(fs) + 12 * s, {0, 0, 0, 0.6f * a});
        b.addText(W * 0.5f - tw / 2, y, fs, toast_, {1, 1, 1, a});
    }
}

} // namespace hotel
