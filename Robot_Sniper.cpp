#include "RobotBase.h"
#include <cstdlib>

class Sniper : public RobotBase {
public:
    // Call base constructor for move, armor, weapon
    Sniper()
        : RobotBase(2, 1, railgun) // move=2, armor=1, weapon=railgun
    {
        m_name = "Sniper";
        m_character = 'N';
        // Health is private; use default in base constructor
    }

    void get_move_direction(int &dir, int &dist) override {
        dir = rand() % 8 + 1;         // random cautious direction
        dist = 1 + rand() % get_move_speed(); // use getter
    }

    bool get_shot_location(int &r, int &c) override {
        int rr, cc;
        get_current_location(rr, cc);
        // Shoot somewhere within the board; farthest logic can be added later
        r = rr + (rand() % 5 - 2); 
        c = cc + (rand() % 5 - 2);
        return true;
    }

    // Required radar methods
    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1;
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // For now, Sniper ignores radar
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Sniper();
}
