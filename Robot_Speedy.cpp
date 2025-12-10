#include "RobotBase.h"
#include <cstdlib>

class Speedy : public RobotBase {
public:
    // Call base constructor: move=4, armor=2, weapon=railgun
    Speedy()
        : RobotBase(4, 2, railgun)
    {
        m_name = "Speedy";
        m_character = 'S';
        // Cannot set m_health directly; use base constructor if needed
    }

    void get_move_direction(int &dir, int &dist) override {
        dir = rand() % 8 + 1;           // random direction 1-8
        dist = 1 + rand() % get_move_speed(); // use getter
    }

    bool get_shot_location(int &r, int &c) override {
        int rr, cc;
        get_current_location(rr, cc);
        r = rr + (rand() % 3 - 1); // shoots near current location
        c = cc + (rand() % 3 - 1);
        return true;
    }

    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1; // random radar
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // Speedy ignores radar for now
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Speedy();
}
