#include "RobotBase.h"
#include <vector>
#include <algorithm>

class Robot_HammerBro : public RobotBase
{
private:
    int target_row = -1;
    int target_col = -1;

public:
    Robot_HammerBro() : RobotBase(3, 3, hammer) {
        m_name = "HammerBro";
        m_character = 'H';
    }

    virtual void get_radar_direction(int& radar_direction) override {
        // Always scan to the right first, then down
        radar_direction = 3; 
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        target_row = -1;
        target_col = -1;
        for (auto& obj : radar_results) {
            if (obj.m_type == 'R') { // First robot found
                target_row = obj.m_row;
                target_col = obj.m_col;
                break;
            }
        }
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override {
        if (target_row != -1 && target_col != -1) {
            shot_row = target_row;
            shot_col = target_col;
            return true;
        }
        return false;
    }

    virtual void get_move_direction(int& move_direction, int& move_distance) override {
        int current_row, current_col;
        get_current_location(current_row, current_col);
        int move = get_move_speed();

        if (target_row != -1 && target_col != -1) {
            // Simple chase logic
            if (target_row > current_row) { move_direction = 5; move_distance = std::min(move, target_row - current_row); }
            else if (target_row < current_row) { move_direction = 1; move_distance = std::min(move, current_row - target_row); }
            else if (target_col > current_col) { move_direction = 3; move_distance = std::min(move, target_col - current_col); }
            else { move_direction = 7; move_distance = std::min(move, current_col - target_col); }
        } else {
            move_direction = 0; // No movement
            move_distance = 0;
        }
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Robot_HammerBro();
}
