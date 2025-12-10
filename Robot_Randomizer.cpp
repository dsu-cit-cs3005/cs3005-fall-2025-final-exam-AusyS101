#include "RobotBase.h"
#include <cstdlib>

class Randomizer : public RobotBase {
public:
    // Call base constructor for move, armor, weapon
    Randomizer()
        : RobotBase(3, 1, static_cast<WeaponType>(rand() % 4)) // move=3, armor=1, random weapon
    {
        m_name = "Randomizer";
        m_character = '#';
        // Health is private; cannot set directly
    }

    void get_move_direction(int &dir, int &dist) override {
        dir = rand() % 8 + 1;
        dist = 1 + rand() % get_move_speed(); // use getter
    }

    bool get_shot_location(int &r, int &c) override {
        int rr, cc;
        get_current_location(rr, cc);
        r = rr + (rand() % 5 - 2); // random 5x5 area around current location
        c = cc + (rand() % 5 - 2);
        return true;
    }

    // Implement required radar functions
    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1;
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // Randomizer ignores radar for now
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Randomizer();
}
