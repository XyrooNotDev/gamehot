// Tes desktop: logika inti, loader aset, audio, UI, dan simulasi Game tanpa GL (RendererStub).
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstring>
#include "assets/AssetLoader.h"
#include "assets/Json.h"
#include "assets/Texture.h"
#include "audio/Mixer.h"
#include "game/Game.h"
#include "ui/Font.h"
#include "world/Pathfinder.h"
#include "world/RoomBuilder.h"

using namespace hotel;

static int g_fail = 0, g_count = 0;
#define CHECK(cond) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("GAGAL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

static const char* kData = TEST_DATA_DIR;

static void testMapAndPaths() {
    Game g;
    g.initLogic(nullptr);
    HotelMap& m = g.map();
    CHECK(m.width() == 26 && m.height() == 14);
    CHECK(m.hasEntrance() && m.entrance() == IVec2({12, 13}));
    CHECK(m.regionCount() == 4 && m.lobbyRegion() == 0);
    CHECK(m.regionAt(21, 2) > 0 && m.regionAt(21, 6) > 0 && m.regionAt(21, 2) != m.regionAt(21, 6));

    MeshData mesh = RoomBuilder::build(m);
    CHECK(!mesh.vertices.empty() && mesh.indices.size() % 3 == 0);
    for (uint32_t i : mesh.indices) CHECK(i < mesh.vertices.size());
    bool winding = true;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const Vertex &a = mesh.vertices[mesh.indices[i]], &b = mesh.vertices[mesh.indices[i + 1]], &c = mesh.vertices[mesh.indices[i + 2]];
        Vec3 n = cross(Vec3{b.px - a.px, b.py - a.py, b.pz - a.pz}, Vec3{c.px - a.px, c.py - a.py, c.pz - a.pz});
        if (dot(n, Vec3{a.nx, a.ny, a.nz}) <= 0) winding = false;
    }
    CHECK(winding);

    std::vector<IVec2> path;
    CHECK(Pathfinder::findPath(m, m.entrance(), {23, 3}, path) && path.size() > 15 && path.back() == IVec2({23, 3}));
    CHECK(!Pathfinder::findPath(m, m.entrance(), {0, 0}, path));   // dinding
    for (IVec2 t : path) CHECK(m.isWalkable(t.x, t.y));
}

static void testFurniture() {
    Game g;
    g.initLogic(nullptr);
    FurnitureManager& f = g.furniture();
    CHECK(f.items().size() == 2);
    std::string why;
    IVec2 far{3, 6};
    CHECK(f.canPlace(3, 5, 5, 0, far, &why));                       // kursi di lobi
    CHECK(!f.canPlace(3, 0, 0, 0, far, &why) && why == "HARUS DI ATAS LANTAI");
    CHECK(!f.canPlace(2, 21, 5, 0, far, &why) && why == "RESEPSI HARUS DI LOBI");
    CHECK(!f.canPlace(0, 5, 5, 0, far, &why) && why == "KASUR HANYA DI KAMAR");
    CHECK(!f.canPlace(3, 8, 10, 0, far, &why) && why == "TEMPAT SUDAH TERISI");
    CHECK(!f.canPlace(3, 3, 6, 0, {3, 6}, &why) && why == "ADA KARAKTER DI SINI");
    CHECK(!f.canPlace(3, 19, 2, 0, far, &why) && why == "MENGHALANGI JALAN");   // menutup pintu kamar 1
    CHECK(!f.canPlace(3, 17, 2, 0, far, &why) && why == "MENGHALANGI JALAN");   // menutup sisi lobi pintu
    CHECK(f.canPlace(1, 20, 5, 0, far, &why));                      // kasur besar di kamar 2

    int uid = f.place(3, 5, 5, 0);
    CHECK(g.map().isBlocked(5, 5) && !g.map().isWalkable(5, 5));
    CHECK(f.at(5, 5) && f.at(5, 5)->uid == uid);
    CHECK(f.remove(uid) && !g.map().isBlocked(5, 5));

    // rotasi: kasur 1x2 diputar -> 2x1
    int w, h;
    FurnitureManager::footprintSize(*findFurniture(0), 1, w, h);
    CHECK(w == 2 && h == 1);

    // furnitur menahan pemain
    Player p;
    p.position = {22.5f, 0, 1.5f};
    for (int i = 0; i < 240; ++i) p.update({1, 0}, 0.0f, 1.0f / 60, g.map());   // ke timur menuju kasur di x=23
    CHECK(p.position.x <= 23.0f - p.radius + 1e-3f);
}

static void testGuestFlow() {
    Game g;
    g.initLogic(nullptr);
    Vec3 near_desk = g.map().tileCenter(9, 12);
    CHECK(!g.guests().canCheckIn(near_desk));
    float t = 0;
    bool arrived = false;
    for (; t < 30 && !arrived; t += 0.05f) {
        g.guests().update(0.05f);
        for (auto& gu : g.guests().guests()) if (gu.state == GuestState::Waiting) arrived = true;
    }
    CHECK(arrived);
    CHECK(g.guests().canCheckIn(near_desk));
    CHECK(!g.guests().canCheckIn(g.map().tileCenter(2, 2)));          // pemain jauh dari resepsi

    std::string msg;
    CHECK(!g.guests().checkIn(g.map().tileCenter(2, 2), msg) && msg == "DEKATI RESEPSI");
    int before = g.economy().money;
    CHECK(g.guests().checkIn(near_desk, msg) && msg == "CHECK-IN BERHASIL");
    for (int i = 0; i < 2400 && g.economy().served == 0; ++i) g.guests().update(0.05f);
    CHECK(g.economy().served == 1);
    CHECK(g.economy().money == before + 45);                           // tarif kasur tunggal, tanpa dekorasi
    // tip dekorasi: 1 tanaman di kamar + 1 di lobi
    g.furniture().place(5, 21, 1, 0);
    g.furniture().place(5, 5, 5, 0);
    const PlacedFurniture* bed = g.furniture().byUid(2);
    CHECK(bed && g.guests().fareFor(*bed) == 45 + 8 + 2);
}

static void testGuestEdgeCases() {
    {   // tanpa kasur -> KAMAR PENUH, tamu tetap menunggu lalu pergi marah
        Game g;
        g.initLogic(nullptr);
        g.furniture().remove(2);
        for (int i = 0; i < 400; ++i) g.guests().update(0.05f);
        std::string msg;
        CHECK(!g.guests().checkIn(g.map().tileCenter(9, 12), msg) && msg == "KAMAR PENUH");
        bool angry = false;
        for (int i = 0; i < 2000 && !angry; ++i) {
            g.guests().update(0.05f);
            for (auto& e : g.guests().takeEvents()) if (e.type == GameEvent::GuestLeftAngry) angry = true;
        }
        CHECK(angry);
        CHECK(g.economy().money == 300);
    }
    {   // kasur dijual saat tamu tidur -> tidak crash, tamu hilang tanpa bayar
        Game g;
        g.initLogic(nullptr);
        for (int i = 0; i < 400; ++i) g.guests().update(0.05f);
        std::string msg;
        CHECK(g.guests().checkIn(g.map().tileCenter(9, 12), msg));
        for (int i = 0; i < 400; ++i) g.guests().update(0.05f);
        g.furniture().remove(2);
        for (int i = 0; i < 200; ++i) g.guests().update(0.05f);
        CHECK(g.economy().served == 0);
    }
    {   // tanpa resepsi -> tidak ada tamu
        Game g;
        g.initLogic(nullptr);
        g.furniture().remove(1);
        for (int i = 0; i < 2000; ++i) g.guests().update(0.05f);
        CHECK(g.guests().guests().empty());
    }
}

static void testAssets() {
    FileAssetIO io(kData);
    AssetLoader ld(&io);

    // PNG + inflate (blok dinamis)
    std::vector<uint8_t> bytes;
    CHECK(io.read("tex.png", bytes));
    Texture tex;
    CHECK(decodePng(bytes.data(), bytes.size(), tex) && tex.w == 2 && tex.h == 2);
    Color c = tex.sample(0.25f, 0.25f);
    CHECK(c.r > 0.99f && c.g < 0.01f);
    c = tex.sample(0.75f, 0.75f);
    CHECK(c.r > 0.99f && c.g > 0.99f && c.b < 0.01f);

    CHECK(io.read("big.png", bytes));
    Texture big;
    CHECK(decodePng(bytes.data(), bytes.size(), big) && big.w == 64);
    std::vector<uint8_t> exp;
    {
        std::vector<uint8_t> eb;
        CHECK(io.read("big.expected", eb));
        std::stringstream ss(std::string(eb.begin(), eb.end()));
        std::string tok;
        while (std::getline(ss, tok, ',')) exp.push_back(static_cast<uint8_t>(std::stoi(tok)));
    }
    bool same = exp.size() == 60;
    for (size_t i = 0; same && i < 20; ++i)
        same = big.rgba[i * 4] == exp[i * 3] && big.rgba[i * 4 + 1] == exp[i * 3 + 1] && big.rgba[i * 4 + 2] == exp[i * 3 + 2];
    CHECK(same);
    std::vector<uint8_t> junk = {1, 2, 3};
    Texture bad;
    CHECK(!decodePng(junk.data(), junk.size(), bad));

    // GLB: transformasi node + warna dari tekstur
    MeshData m;
    CHECK(ld.loadGltfFile("models/tri.glb", m));
    CHECK(m.vertices.size() == 3 && m.indices.size() == 3);
    CHECK(near(m.vertices[1].px, 11.0f) && near(m.vertices[2].py, 1.0f));
    CHECK(m.vertices[0].r > 0.99f && m.vertices[0].g < 0.01f);      // merah
    CHECK(m.vertices[1].g > 0.99f && m.vertices[1].r < 0.01f);      // hijau
    CHECK(m.vertices[2].b > 0.99f && m.vertices[2].r < 0.01f);      // biru

    MeshData m2;
    CHECK(ld.loadGltfFile("models/tri_b64.gltf", m2) && m2.vertices.size() == 3);
    CHECK(near(m2.vertices[0].r, 0.5f) && near(m2.vertices[0].g, 0.25f) && near(m2.vertices[0].b, 1.0f));

    MeshData m3;
    CHECK(ld.loadObjFile("models/quad.obj", m3) && m3.vertices.size() == 6 && m3.indices.size() == 6);
    CHECK(near(m3.vertices[0].r, 1.0f) && near(m3.vertices[0].g, 0.0f));

    MeshData m4;
    CHECK(ld.loadModel("models/tri", m4) && ld.loadModel("models/quad", m4));
    CHECK(!ld.loadModel("models/tidak_ada", m4));

    // fit: footprint & tinggi
    AssetLoader::fit(m3, 1.0f, 1.0f);
    float minx = 1e9, maxx = -1e9;
    for (auto& v : m3.vertices) { minx = std::min(minx, v.px); maxx = std::max(maxx, v.px); }
    CHECK(near(minx, -maxx) && near(maxx - minx, 0.92f));
    AssetLoader::fit(m, 1, 1, 1.35f);
    float maxy = 0;
    for (auto& v : m.vertices) maxy = std::max(maxy, v.py);
    CHECK(near(maxy, 1.35f));

    // JSON
    JsonValue j;
    const char* src = "{\"a\":[1,2.5,{\"b\":\"x\\ny\"}],\"c\":true,\"d\":null}";
    CHECK(parseJson(src, std::strlen(src), j));
    CHECK(j["a"].size() == 3 && near(static_cast<float>(j["a"][1].number()), 2.5f) && j["a"][2]["b"].string() == "x\ny");
    CHECK(j["c"].b && j["d"].isNull() && j["zzz"].isNull());
    CHECK(!parseJson("{\"a\":", 5, j));
}

static void testAudio() {
    SoundBank bank(44100);
    bank.synthesizeDefaults();
    for (int i = 0; i < kSfxCount; ++i) {
        const auto& s = bank.get(static_cast<Sfx>(i));
        CHECK(s.size() > 1000);
        float peak = 0;
        for (float v : s) peak = std::max(peak, std::fabs(v));
        CHECK(peak > 0.05f && peak <= 1.0f);
    }
    FileAssetIO io(kData);
    std::vector<uint8_t> wav;
    CHECK(io.read("sounds/coin.wav", wav));
    CHECK(bank.loadWav(Sfx::Coin, wav));
    CHECK(bank.get(Sfx::Coin).size() > 4300 && bank.get(Sfx::Coin).size() < 4500);     // di-resample ke 44.1 kHz
    std::vector<uint8_t> notwav(100, 7);
    CHECK(!bank.loadWav(Sfx::Bell, notwav));

    Mixer mx;
    mx.setBank(&bank);
    std::vector<float> buf(512);
    mx.render(buf.data(), 512);
    float silent = 0;
    for (float v : buf) silent += std::fabs(v);
    CHECK(silent == 0);
    mx.trigger(Sfx::Bell);
    mx.trigger(Sfx::Coin);
    mx.render(buf.data(), 512);
    float loud = 0;
    for (float v : buf) { loud += std::fabs(v); CHECK(std::fabs(v) <= 1.0f); }
    CHECK(loud > 1.0f);
    for (int i = 0; i < 200; ++i) mx.render(buf.data(), 512);                           // semua suara selesai
    float after = 0;
    for (float v : buf) after += std::fabs(v);
    CHECK(after == 0);
}

static void testUI() {
    TouchUI ui;
    ui.setScreenSize(1280, 720);
    UIButton b;
    b.id = 7; b.rect = {1000, 600, 200, 80}; b.label = "OK";
    ui.setLayout({b}, {UIRect{600, 50, 600, 500}});

    ui.onTouch(TouchAction::Down, 0, 1050, 640);
    ui.onTouch(TouchAction::Up, 0, 1060, 650);
    auto clicks = ui.consumeClicks();
    CHECK(clicks.size() == 1 && clicks[0] == 7);

    ui.onTouch(TouchAction::Down, 0, 1050, 640);            // geser keluar tombol lalu lepas -> tidak klik
    ui.onTouch(TouchAction::Up, 0, 100, 100);
    CHECK(ui.consumeClicks().empty());

    ui.onTouch(TouchAction::Down, 1, 800, 300);              // panel menelan sentuhan: tidak memutar kamera
    ui.onTouch(TouchAction::Move, 1, 850, 320);
    CHECK(ui.consumeCameraDelta().x == 0);
    ui.onTouch(TouchAction::Up, 1, 850, 320);

    ui.onTouch(TouchAction::Down, 2, 300, 500);              // joystick
    ui.onTouch(TouchAction::Move, 2, 300 + 200, 500);
    CHECK(near(ui.moveAxis().x, 1.0f) && near(ui.moveAxis().y, 0.0f));
    ui.onTouch(TouchAction::Move, 2, 300, 500 - 200);
    CHECK(ui.moveAxis().y > 0.99f);
    ui.onTouch(TouchAction::Up, 2, 300, 300);
    CHECK(ui.moveAxis().x == 0 && ui.moveAxis().y == 0);

    UIBatch batch;
    batch.addText(0, 0, 3, "KASUR BESAR $120", {1, 1, 1, 1});
    CHECK(batch.vertices().size() > 100 && batch.vertices().size() % 3 == 0);
    CHECK(near(Font::textWidth("AB", 2), 22));
}

static void testGameSimulation() {
    FileAssetIO io(kData);
    Game g;
    Renderer r;
    g.initLogic(&io);
    g.resize(1280, 720);
    g.createGraphics(r);
    CHECK(g.sampleRate() == 44100);

    // --- layar loading ---
    CHECK(g.phase() == LoadPhase::Loading && g.loadProgress() == 0.0f);
    g.onTouch(TouchAction::Down, 0, 300, 500);                   // sentuhan saat loading diabaikan
    g.onTouch(TouchAction::Move, 0, 500, 500);
    CHECK(g.ui().moveAxis().x == 0);
    g.onTouch(TouchAction::Up, 0, 500, 500);
    size_t guestsBefore = g.guests().guests().size();
    float last = 0;
    int frames = 0;
    bool monotonic = true, sawPartial = false, sawFade = false;
    while (!g.loadingFinished() && frames < 60 * 20) {
        g.update(1.0f / 60);
        g.render(r);
        monotonic = monotonic && g.loadProgress() >= last;
        last = g.loadProgress();
        if (g.loadProgress() > 0.1f && g.loadProgress() < 0.9f) sawPartial = true;
        if (g.phase() == LoadPhase::FadeOut) sawFade = true;
        ++frames;
    }
    CHECK(g.loadingFinished() && monotonic && sawPartial && sawFade);
    CHECK(frames >= static_cast<int>(1.6f * 60));                // layar pembuka tampil minimal ~1.6 dtk
    CHECK(frames < 60 * 4);
    CHECK(g.guests().guests().size() == guestsBefore);           // permainan belum berjalan saat loading
    CHECK(g.soundBank().get(Sfx::Coin).size() > 4300 && g.soundBank().get(Sfx::Coin).size() < 4500);   // WAV override terbaca saat loading

    // konteks GL hilang lalu pulih -> loading singkat, state permainan tetap
    int moneyBefore = g.economy().money;
    g.destroyGraphics(r);
    g.createGraphics(r);
    frames = 0;
    while (!g.loadingFinished() && frames < 60 * 20) { g.update(1.0f / 60); g.render(r); ++frames; }
    CHECK(g.loadingFinished() && frames < static_cast<int>(1.6f * 60) && g.economy().money == moneyBefore);

    // --- beli furnitur ---
    g.player().position = g.map().tileCenter(5, 6);
    g.player().heading = 0;                                  // menghadap +Z
    g.handleAction(Game::ActShop);
    CHECK(g.mode() == GameMode::Shop);
    g.economy().money = 10;
    g.handleAction(Game::ActBuyBase + 1);
    CHECK(g.mode() == GameMode::Shop && g.toast() == "UANG KURANG");
    g.economy().money = 300;
    g.handleAction(Game::ActBuyBase + 3);                    // kursi $25
    CHECK(g.mode() == GameMode::Placing);
    g.update(1.0f / 60);
    g.render(r);
    size_t before = g.furniture().items().size();
    g.handleAction(Game::ActRotate);
    g.handleAction(Game::ActConfirm);
    CHECK(g.mode() == GameMode::Play && g.furniture().items().size() == before + 1);
    CHECK(g.economy().money == 275);

    // --- jual (setengah harga) ---
    g.player().heading = 0;
    g.update(1.0f / 60);
    g.handleAction(Game::ActSell);
    CHECK(g.furniture().items().size() == before && g.economy().money == 275 + 12);

    // --- penempatan ditolak ---
    g.player().position = g.map().tileCenter(1, 1);
    g.player().heading = 3.14159f;                           // menghadap ke dinding utara
    g.handleAction(Game::ActBuyBase + 3);
    g.update(1.0f / 60);
    g.handleAction(Game::ActConfirm);
    CHECK(g.mode() == GameMode::Placing);                    // tetap di mode penempatan
    g.handleAction(Game::ActCancel);
    CHECK(g.mode() == GameMode::Shop);
    g.handleAction(Game::ActShopClose);

    // --- teks tombol harus muat di tombolnya pada berbagai ukuran layar & mode ---
    const int sizes[][2] = {{1280, 720}, {1920, 1080}, {2400, 1080}, {960, 540}, {800, 480}};
    for (auto& sz : sizes) {
        g.resize(sz[0], sz[1]);
        for (GameMode want : {GameMode::Play, GameMode::Shop, GameMode::Placing}) {
            g.handleAction(Game::ActShopClose);
            if (want == GameMode::Shop) g.handleAction(Game::ActShop);
            if (want == GameMode::Placing) g.handleAction(Game::ActBuyBase + 6);   // sofa (nama 4 huruf), $80
            g.update(1.0f / 60);
            CHECK(g.mode() == want);
            for (const auto& b : g.ui().buttons()) {
                CHECK(Font::textWidth(b.label, b.textScale) <= b.rect.w * 0.98f);
                if (!b.sub.empty()) CHECK(Font::textWidth(b.sub, b.textScale) <= b.rect.w * 0.98f);
                CHECK(b.rect.x >= 0 && b.rect.y >= 0 && b.rect.x + b.rect.w <= sz[0] && b.rect.y + b.rect.h <= sz[1]);
            }
            g.render(r);
        }
        g.handleAction(Game::ActShopClose);
    }
    g.resize(1280, 720);

    // --- permainan penuh: tamu datang, pemain check-in, dapat uang ---
    g.economy().money = 300;
    g.guests().reset();
    g.economy().served = 0;
    g.player().position = g.map().tileCenter(9, 12);
    int checkins = 0;
    for (int i = 0; i < 60 * 150; ++i) {
        g.update(1.0f / 60);
        if (g.guests().canCheckIn(g.player().position)) { g.handleAction(Game::ActCheckIn); ++checkins; }
        if (i % 600 == 0) g.render(r);
    }
    CHECK(checkins >= 1 && g.economy().served >= 1 && g.economy().money > 300);

    // --- simpan / muat ---
    std::string save = g.saveState();
    Game g2;
    g2.initLogic(nullptr);
    g2.furniture().place(3, 5, 5, 0);                        // akan diganti oleh data simpanan
    CHECK(g2.loadState(save));
    CHECK(g2.economy().money == g.economy().money && g2.economy().served == g.economy().served);
    CHECK(g2.furniture().items().size() == g.furniture().items().size());
    CHECK(!g2.loadState("sampah"));

    g.destroyGraphics(r);
}

static void testCamera() {
    CameraController cam;
    cam.snapTo({0, 0, 0});
    CHECK(cam.view().m[14] < 0);
    Mat4 vp = cam.projection(16.0f / 9.0f) * cam.view();
    Vec2 s;
    CHECK(projectToScreen({0, 0, 0}, vp, 1280, 720, s) && near(s.x, 640, 1) && near(s.y, 360, 1));
    CHECK(!projectToScreen(cam.eye() + (cam.eye() * 1.0f), vp, 1280, 720, s));   // di belakang kamera
}

int main() {
    testMapAndPaths();
    testFurniture();
    testGuestFlow();
    testGuestEdgeCases();
    testAssets();
    testAudio();
    testUI();
    testCamera();
    testGameSimulation();
    std::printf("%d pemeriksaan, %d gagal\n", g_count, g_fail);
    return g_fail ? 1 : 0;
}
