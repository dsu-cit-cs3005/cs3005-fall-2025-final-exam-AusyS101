#include "RobotBase.h"
#include <cstdlib>
#include <ctime>

class Robot_Grenadier : public RobotBase
{
public:
    Robot_Grenadier() : RobotBase(2, 4, grenade) {
        m_name = "Grenadier";
        m_character = 'G';
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    virtual void get_radar_direction(int& radar_direction) override {
        radar_direction = (std::rand() % 8) + 1; // Random radar
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        // Ignore radar results
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override {
        if (get_grenades() > 0) {
            shot_row = std::rand() % m_board_row_max;
            shot_col = std::rand() % m_board_col_max;
            decrement_grenades();
            return true;
        }
        return false;
    }

    virtual void get_move_direction(int& move_direction, int& move_distance) override {
        move_direction = (std::rand() % 8) + 1; 
        move_distance = 1;
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Robot_Grenadier();
}
