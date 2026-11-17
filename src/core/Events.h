#pragma once
#include "core/Math.h"

namespace hotel {

// Kejadian dari logika game -> dipakai untuk suara & pesan di layar.
struct GameEvent {
    enum Type { GuestArrived, CheckedIn, NoRoom, Paid, GuestLeftAngry } type;
    int amount = 0;      // untuk Paid
    Vec3 pos;
};

} // namespace hotel
