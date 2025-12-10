#include "RobotBase.h"
#include <cstdlib>

class Berserker : public RobotBase {
public:
    // Use the base constructor to set move speed, armor, and weapon
    Berserker()
        : RobotBase(4, 1, flamethrower)  // move=4, armor=1, weapon=flamethrower
    {
        m_name = "Berserker";
        m_character = 'Z';
        // Health is private; can't modify, assume base constructor sets default
    }

    // Implement required virtual methods
    void get_move_direction(int &direction, int &distance) override {
        direction = rand() % 8 + 1;  // aggressive random direction
        distance = 1 + rand() % get_move_speed(); // use public getter
    }

    bool get_shot_location(int &r, int &c) override {
        r = rand() % m_board_row_max;
        c = rand() % m_board_col_max;
        return true;
    }

    void get_radar_direction(int &radar_direction) override {
        radar_direction = rand() % 8 + 1; // random radar scan
    }

    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        // ignore for now
    }
};

// Factory function for dynamic loading
extern "C" RobotBase* create_robot() {
    return new Berserker();
}
