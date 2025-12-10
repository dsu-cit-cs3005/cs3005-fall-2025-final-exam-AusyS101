#include "RobotBase.h"
#include <vector>
#include <iostream>
#include <algorithm> // For std::find_if

class Robot_Ratboy : public RobotBase 
{
private:
    bool m_moving_down = true;
    int to_shoot_row = -1;
    int to_shoot_col = -1;
    std::vector<RadarObj> known_obstacles;

    bool is_obstacle(int row, int col) const 
    {
        return std::any_of(known_obstacles.begin(), known_obstacles.end(), 
                           [&](const RadarObj& obj) {
                               return obj.m_row == row && obj.m_col == col;
                           });
    }

    void clear_target() 
    {
        to_shoot_row = -1;
        to_shoot_col = -1;
    }

    void add_obstacle(const RadarObj& obj) 
    {
        if ((obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F') &&
            !is_obstacle(obj.m_row, obj.m_col)) 
        {
            known_obstacles.push_back(obj);
        }
    }

public:
    Robot_Ratboy() : RobotBase(3, 4, railgun) 
    {
        m_name = "Ratboy";
        m_character = 'R';
    }

    virtual void get_radar_direction(int& radar_direction) override 
    {
        int current_row, current_col;
        get_current_location(current_row, current_col);
        radar_direction = (current_col > 0) ? 7 : 3;
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override 
    {
        clear_target();
        for (const auto& obj : radar_results) 
        {
            add_obstacle(obj);
            if (obj.m_type == 'R' && to_shoot_row == -1 && to_shoot_col == -1) 
            {
                to_shoot_row = obj.m_row;
                to_shoot_col = obj.m_col;
            }
        }
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override 
    {
        if (to_shoot_row != -1 && to_shoot_col != -1) 
        {
            shot_row = to_shoot_row;
            shot_col = to_shoot_col;
            clear_target();
            return true;
        }
        return false;
    }

    void get_move_direction(int& move_direction, int& move_distance) override 
    {
        int current_row, current_col;
        get_current_location(current_row, current_col);

        if (current_col > 0) 
        {
            move_direction = 7;
            move_distance = 1;
            return;
        }

        if (m_moving_down) 
        {
            if (current_row < m_board_row_max - 1) 
            {
                move_direction = 5;
                move_distance = 1;
            } 
            else 
            {
                m_moving_down = false;
                move_direction = 1;
                move_distance = 1;
            }
        } 
        else 
        {
            if (current_row > 0) 
            {
                move_direction = 1;
                move_distance = 1;
            } 
            else 
            {
                m_moving_down = true;
                move_direction = 5;
                move_distance = 1;
            }
        }
    }
}; // <-- Close the class here

// Factory function must be outside the class
extern "C" RobotBase* create_robot() 
{
    return new Robot_Ratboy();
}
