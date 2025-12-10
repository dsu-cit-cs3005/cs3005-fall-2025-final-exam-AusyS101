// Updated Robot_Teto.cpp
#include "RobotBase.h"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

class Robot_Teto : public RobotBase {
private:
    // remember last radar results (targets and obstacles)
    std::vector<RadarObj> last_scan;

    // helper: distance squared
    static int dist2(int r1, int c1, int r2, int c2) {
        int dr = r1 - r2;
        int dc = c1 - c2;
        return dr*dr + dc*dc;
    }

    // helper: map (dr,dc) to direction index 1..8 using directions[] from RobotBase.h
    static int dir_from_delta(int dr, int dc) {
        for (int i = 1; i <= 8; ++i) {
            if (directions[i].first == dr && directions[i].second == dc) return i;
        }
        return 0; // invalid
    }

    // choose nearest enemy in last_scan; returns index in last_scan or -1
    int nearest_enemy_idx(int my_r, int my_c) const {
        int best = -1;
        int bestd = std::numeric_limits<int>::max();
        for (size_t i = 0; i < last_scan.size(); ++i) {
            const RadarObj &o = last_scan[i];
            if (o.m_type == m_character) continue; // ignore self (shouldn't appear)
            // treat uppercase as robots (assignment uses chars for robots)
            if (!isalpha((unsigned char)o.m_type)) continue;
            int d = dist2(my_r, my_c, o.m_row, o.m_col);
            if (d < bestd) { bestd = d; best = (int)i; }
        }
        return best;
    }

    // check if a tile is a flame obstacle
    bool is_flame_tile(int row, int col) const {
        for (const auto &o : last_scan) {
            if (o.m_row == row && o.m_col == col && o.m_type == 'F')
                return true;
        }
        return false;
    }

    bool is_danger_cell(int row, int col) const {
        for (auto &[dr, dc] : danger_cells)
            if (dr == row && dc == col) return true;
        return false;
    }

    std::vector<std::pair<int,int>> danger_cells;


public:
    Robot_Teto()
        : RobotBase(3, 2, railgun) // sensible defaults: move=3, armor=2, default weapon=railgun
    {
        m_name = "Teto";
        m_character = '4';
        // health private: base ctor sets default health (as assignment requires)
    }

    // choose a radar direction: prefer direction towards nearest last-known enemy
    void get_radar_direction(int &radar_direction) override {
        int r, c;
        get_current_location(r, c);
        int idx = nearest_enemy_idx(r, c);
        if (idx != -1) {
            int tr = last_scan[idx].m_row;
            int tc = last_scan[idx].m_col;
            int dr = tr - r;
            int dc = tc - c;
            // normalize to -1,0,1
            if (dr > 0) dr = 1; else if (dr < 0) dr = -1; else dr = 0;
            if (dc > 0) dc = 1; else if (dc < 0) dc = -1; else dc = 0;
            int d = dir_from_delta(dr, dc);
            radar_direction = (d == 0) ? (rand() % 8 + 1) : d;
            return;
        }

        // otherwise random scan
        radar_direction = rand() % 8 + 1;
    }

    // store radar results for decision making
    void process_radar_results(const std::vector<RadarObj> &radar_results) override {
        last_scan = radar_results;
        danger_cells.clear();

        int my_r, my_c;
        get_current_location(my_r, my_c);

        for (const auto &o : last_scan) {
            // Track flames directly
            if (o.m_type == 'F') {
                danger_cells.push_back({o.m_row, o.m_col});
            }

            // Track enemies as potential threats
            if (isalpha((unsigned char)o.m_type) && o.m_type != m_character) {
                int er = o.m_row;
                int ec = o.m_col;

                // Hammer danger: adjacent cells
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        int nr = er + dr;
                        int nc = ec + dc;
                        if (nr >= 0 && nr < m_board_row_max && nc >= 0 && nc < m_board_col_max)
                            danger_cells.push_back({nr, nc});
                    }
                }

                // Railgun danger: full row and full column
                for (int i = 0; i < m_board_row_max; ++i)
                    danger_cells.push_back({i, ec});
                for (int j = 0; j < m_board_col_max; ++j)
                    danger_cells.push_back({er, j});

                // Flamethrower danger: 3x4 area in front of robot (approx)
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = 1; dc <= 4; ++dc) {
                        int nr = er + dr;
                        int nc = ec + dc; // assume robot faces “right” for simplicity
                        if (nr >= 0 && nr < m_board_row_max && nc >= 0 && nc < m_board_col_max)
                            danger_cells.push_back({nr, nc});
                    }
                }
            }
        }
    }


    // choose where to shoot this turn. Tries to pick meaningful target based on weapon and radar data.
    bool get_shot_location(int &shot_row, int &shot_col) override {
        int r, c;
        get_current_location(r, c);

        int idx = nearest_enemy_idx(r, c);
        // if we see someone, target them intelligently
        if (idx != -1) {
            const RadarObj &target = last_scan[idx];

            WeaponType w = get_weapon();

            if (w == hammer) {
                // prefer adjacent cell (one of 8 neighbors) that contains the robot
                int dr = target.m_row - r;
                int dc = target.m_col - c;
                // clamp to -1..1
                if (dr > 1) dr = 1; else if (dr < -1) dr = -1;
                if (dc > 1) dc = 1; else if (dc < -1) dc = -1;
                shot_row = r + dr;
                shot_col = c + dc;
                return true;
            }

            if (w == railgun) {
                // railgun hits along a single row or column direction in arena.
                // choose shot_row equal to target row so railgun will affect that row.
                shot_row = target.m_row;
                shot_col = target.m_col; // arena's railgun logic in apply_shot expects a row to match
                return true;
            }

            if (w == flamethrower) {
                // flamethrower affects a rectangular area; target the cell just in front of the robot towards enemy
                int dr = target.m_row - r;
                int dc = target.m_col - c;
                if (dr > 0) dr = 1; else if (dr < 0) dr = -1; else dr = 0;
                if (dc > 0) dc = 1; else if (dc < 0) dc = -1; else dc = 0;
                // pick a cell 1 step toward enemy (if enemy adjacent, target their cell)
                shot_row = r + dr;
                shot_col = c + dc;
                // ensure within bounds
                if (shot_row < 0) shot_row = 0;
                if (shot_col < 0) shot_col = 0;
                if (shot_row >= m_board_row_max) shot_row = m_board_row_max - 1;
                if (shot_col >= m_board_col_max) shot_col = m_board_col_max - 1;
                return true;
            }

            if (w == grenade) {
                // if grenades available, lob directly at the enemy coordinates to get 3x3 area
                if (get_grenades() > 0) {
                    shot_row = target.m_row;
                    shot_col = target.m_col;
                    return true;
                }
            }
        }

        // fallback: no visible enemy => random shooting chance for chaos (grenade or railgun)
        WeaponType w = get_weapon();
        if (w == grenade && get_grenades() > 0) {
            shot_row = rand() % m_board_row_max;
            shot_col = rand() % m_board_col_max;
            return true;
        }
        if (w == railgun) {
            // shoot across a random row
            shot_row = rand() % m_board_row_max;
            shot_col = rand() % m_board_col_max;
            return true;
        }

        // otherwise don't shoot
        return false;
    }

    // movement: move exactly ONE cell per turn toward nearest seen target; otherwise wander one cell
    void get_move_direction(int &move_direction, int &move_distance) override {
        int r, c;
        get_current_location(r, c);

        move_distance = 1;  // default one step

        int idx = nearest_enemy_idx(r, c);
        if (idx != -1) {
            int tr = last_scan[idx].m_row;
            int tc = last_scan[idx].m_col;

            int dr = tr - r;
            int dc = tc - c;

            if (dr > 0) dr = 1; else if (dr < 0) dr = -1; else dr = 0;
            if (dc > 0) dc = 1; else if (dc < 0) dc = -1; else dc = 0;

            int d = dir_from_delta(dr, dc);

            // Try the direct step toward enemy
            if (d != 0) {
                int nr = r + directions[d].first;
                int nc = c + directions[d].second;

                if (!is_flame_tile(nr, nc) && !is_danger_cell(nr, nc)) {
                    move_direction = d;
                    return;
                }
            }

            // Otherwise choose the safest move that gets closer
            std::vector<std::pair<int,int>> options;
            for (int d2 = 1; d2 <= 8; ++d2) {
                int nr = r + directions[d2].first;
                int nc = c + directions[d2].second;

                if (nr < 0 || nr >= m_board_row_max || nc < 0 || nc >= m_board_col_max)
                    continue;
                if (is_flame_tile(nr, nc) || is_danger_cell(nr, nc))
                    continue;

                int d2dist = dist2(nr, nc, tr, tc);
                options.push_back({d2dist, d2});
            }

            if (!options.empty()) {
                std::sort(options.begin(), options.end());
                move_direction = options[0].second;
                return;
            }

            // Surrounded or no safe move
            move_direction = 0;
            move_distance = 0;
            return;
        }

        // No target: remain in place like HammerBro
        move_direction = 0;
        move_distance = 0;


        // fallback: stay put
        move_direction = 0;
        move_distance = 0;
    }


    virtual ~Robot_Teto() {}
};

// factory
extern "C" RobotBase* create_robot() {
    return new Robot_Teto();
}
