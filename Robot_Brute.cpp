#include "RobotBase.h"
#include <cstdlib>

class Brute : public RobotBase {
public:
    // Call base constructor for move, armor, weapon
    Brute()
        : RobotBase(1, 3, hammer) // move=1, armor=3, weapon=hammer
    {
        m_name = "Brute";
        m_character = 'B';
        // Cannot set m_health directly; use base constructor if available
    }

    void get_move_direction(int &dir, int &dist) override {
        dir = rand() % 8 + 1;
        dist = 1;  // Brute moves slowly
    }

    bool get_shot_location(int &r, int &c) override {
        int rr, cc;
        get_current_location(rr, cc);
        r = rr + (rand() % 3 - 1);  // shoot nearby
        c = cc + (rand() % 3 - 1);
        return true;
    }

    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1;
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // Brute ignores radar for now
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Brute();
}
