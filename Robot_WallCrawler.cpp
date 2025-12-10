#include "RobotBase.h"
#include <vector>

class Robot_WallCrawler : public RobotBase
{
private:
    bool moving_down = true;
    int target_row = -1, target_col = -1;

public:
    Robot_WallCrawler() : RobotBase(3, 3, railgun) {
        m_name = "WallCrawler";
        m_character = 'W';
    }

    virtual void get_radar_direction(int& radar_direction) override {
        radar_direction = 5; // Always look down along the wall
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        target_row = -1;
        target_col = -1;
        for (auto& obj : radar_results) {
            if (obj.m_type == 'R') {
                target_row = obj.m_row;
                target_col = obj.m_col;
                break;
            }
        }
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override {
        if (target_row != -1) {
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

        if (moving_down) {
            if (current_row + move < m_board_row_max) {
                move_direction = 5; // Down
                move_distance = move;
            } else { moving_down = false; move_direction = 1; move_distance = move; }
        } else {
            if (current_row - move >= 0) {
                move_direction = 1; // Up
                move_distance = move;
            } else { moving_down = true; move_direction = 5; move_distance = move; }
        }
    }
};

// Factory function
extern "C" RobotBase* create_robot() {
    return new Robot_WallCrawler();
}
