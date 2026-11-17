#pragma once

namespace hotel {

struct Economy {
    int money = 300;
    int served = 0;     // jumlah tamu yang sudah membayar
    bool canAfford(int cost) const { return money >= cost; }
    bool spend(int cost) { if (money < cost) return false; money -= cost; return true; }
    void earn(int amount) { money += amount; }
};

} // namespace hotel
