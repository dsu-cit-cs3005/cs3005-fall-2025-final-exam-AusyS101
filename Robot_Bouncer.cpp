#include "RobotBase.h"
#include <cstdlib>

class Bouncer : public RobotBase {
public:
    // Use base constructor to set move, armor, weapon
    Bouncer()
        : RobotBase(3, 2, hammer)  // move=3, armor=2, weapon=hammer
    {
        m_name = "Bouncer";
        m_character = 'O';
        // Can't set health directly; use base defaults
    }

    // Required pure virtual methods
    void get_move_direction(int &dir, int &dist) override {
        dir = (rand() % 8 + 1); // random direction
        dist = (rand() % get_move_speed()) + 1; // random distance using public getter
    }

    bool get_shot_location(int &r, int &c) override {
        int rr, cc;
        get_current_location(rr, cc);
        r = rr + (rand() % 5 - 2); // shoot within 5x5 area
        c = cc + (rand() % 5 - 2);
        return true;
    }

    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1; // random radar scan
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // Bouncer ignores radar for now
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Bouncer();
}
