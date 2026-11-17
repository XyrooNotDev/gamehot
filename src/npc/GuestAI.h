#pragma once
#include <string>
#include <vector>
#include "core/Events.h"
#include "furniture/FurnitureManager.h"
#include "game/Economy.h"
#include "world/HotelMap.h"

namespace hotel {

enum class GuestState {
    Arriving,   // jalan dari pintu masuk ke antrean resepsionis
    Waiting,    // menunggu pemain menekan CHECK-IN (kesabaran menurun)
    ToRoom,     // jalan ke kasur yang ditugaskan
    Staying,    // tidur di kamar (tidak digambar)
    Leaving,    // jalan keluar
    Gone
};

struct Guest {
    int id = 0;
    GuestState state = GuestState::Arriving;
    Vec3 pos;
    float heading = 0;
    std::vector<IVec2> path;
    size_t pathIdx = 0;
    IVec2 target;
    int slot = -1;
    int bedUid = -1;
    int variant = 0;
    float patience = 0, patienceMax = 1;
    float timer = 0;
    bool paysOnExit = false;
    int fare = 0;
};

// Mengelola tamu: muncul -> antre -> check-in (oleh pemain) -> kamar -> bayar -> pulang.
class GuestManager {
public:
    static constexpr int kMaxGuests = 6;
    static constexpr int kQueueSlots = 4;

    void init(HotelMap* map, FurnitureManager* furn, Economy* eco, unsigned seed = 12345);
    void reset();
    void update(float dt);

    // Pemain cukup dekat dengan resepsionis dan ada tamu yang menunggu?
    bool canCheckIn(Vec3 playerPos) const;
    // Check-in tamu yang menunggu paling lama. false (+msg) jika gagal.
    bool checkIn(Vec3 playerPos, std::string& msg);

    const std::vector<Guest>& guests() const { return guests_; }
    std::vector<GameEvent> takeEvents();

    // Pendapatan per tamu untuk kasur ini (tarif + bonus dekorasi kamar + tip lobi).
    int fareFor(const PlacedFurniture& bed) const;

private:
    std::vector<IVec2> queueSlots() const;
    bool nearReception(Vec3 p) const;
    bool setPath(Guest& g, IVec2 goal);
    void follow(Guest& g, float dt);
    void beginLeaving(Guest& g, bool pay);
    float rnd(float a, float b);

    HotelMap* map_ = nullptr;
    FurnitureManager* furn_ = nullptr;
    Economy* eco_ = nullptr;
    std::vector<Guest> guests_;
    std::vector<GameEvent> events_;
    float spawnTimer_ = 4.0f;
    int nextId_ = 1;
    unsigned rng_ = 1;
};

} // namespace hotel
