// Updated RobotWarzArena.cpp
#include <bits/stdc++.h>
#include <dlfcn.h>
#include <filesystem>
#include "RobotBase.h"
#include "RadarObj.h"
#include <algorithm>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

static const int BOARD_ROWS = 20;
static const int BOARD_COLS = 20;
static const char FLAME_OBS = 'F';
static const char PIT_OBS = 'P';
static const char MOUND_OBS = 'M';

struct LoadedRobot {
    string cpp_file;
    string so_file;
    void* handle = nullptr;
    RobotBase* robot_instance = nullptr;
    bool alive = false;
    bool is_dead = false;
    std::set<int> blocked_dirs;
    bool can_move_flag = true;
};

// Helper: compile a robot implementation (Robot_X.cpp) into a libRobot_X.so
bool compile_robot(const string& cpp, string& out_so) {
    string base = cpp.substr(0, cpp.find(".cpp"));
    out_so = "./lib" + base + ".so";
    string cmd = "g++ -shared -fPIC -o " + out_so + " " + cpp + " RobotBase.o -I. -std=c++20";
    cerr << "Compiling: " << cmd << "\n";
    return system(cmd.c_str()) == 0;
}

// Helper: Obstacles
void place_obstacles(vector<vector<char>>& board) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> rdist(0, BOARD_ROWS - 1);
    uniform_int_distribution<> cdist(0, BOARD_COLS - 1);

    int num_flames = 10;
    int num_pits = 5;
    int num_mounds = 10;

    auto place = [&](char ch, int count) {
        while (count--) {
            int r, c;
            do {
                r = rdist(gen);
                c = cdist(gen);
            } while (board[r][c] != '.');
            board[r][c] = ch;
        }
    };

    place(FLAME_OBS, num_flames);
    place(PIT_OBS, num_pits);
    place(MOUND_OBS, num_mounds);
}

// Place robots on the board at random free cells
void place_robots_random(vector<LoadedRobot>& robots, vector<vector<char>>& board) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> rdist(0, BOARD_ROWS - 1);
    uniform_int_distribution<> cdist(0, BOARD_COLS -1);

    for (auto &lr : robots) {
        if (!lr.robot_instance) continue;
        int r, c;
        do {
            r = rdist(gen);
            c = cdist(gen);
        } while (board[r][c] != '.');

        lr.robot_instance->move_to(r,c);
        lr.robot_instance->set_boundaries(BOARD_ROWS, BOARD_COLS);
        board[r][c] = lr.robot_instance->m_character;
        lr.alive = true;

        cout << "Loaded robot: " << lr.robot_instance->m_name
             << " at (" << r << "," << c << ")\n";
    }
}

// Print board nicely
void print_board(const vector<vector<char>>& board) {
    cout << "\n=========== board ===========\n";
    cout << " ";
    for (int c = 0; c < BOARD_COLS; ++c) cout << setw(3) << c;
    cout << "\n\n";
    for (int r = 0; r < BOARD_ROWS; ++r) {
        cout << setw(3) << r << " ";
        for (int c = 0; c < BOARD_COLS; ++c) {
            cout << setw(3) << board[r][c];
        }
        cout << "\n\n";
    }
}

// Helper to find robot index at a position
int find_robot_at(const vector<LoadedRobot>& robots, int row, int col, bool include_dead = false) {
    for (size_t i = 0; i < robots.size(); ++i) {
        if (!robots[i].robot_instance) continue;
        if (!include_dead && !robots[i].alive) continue;
        int rr, cc;
        robots[i].robot_instance->get_current_location(rr, cc);
        if (rr == row && cc == col) return (int)i;
    }
    return -1;
}


void mark_robot_dead(LoadedRobot& lr, vector<vector<char>>& board) {
    if (!lr.robot_instance) return;
    int r, c;
    lr.robot_instance->get_current_location(r, c);
    board[r][c] = 'X';
    lr.alive = false;
    lr.is_dead = true;
}


// Build radar results for a robot scanning in a given direction
vector<RadarObj> do_radar_scan(RobotBase* robot, int direction, const vector<vector<char>>& board, const vector<LoadedRobot>& robots) {
    vector<RadarObj> res;
    if (direction <= 0 || direction > 8) return res;

    int r0, c0;
    robot->get_current_location(r0, c0);
    auto [dr, dc] = directions[direction];

    int r = r0 + dr, c = c0 + dc;
    while (r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS) {
        int idx = find_robot_at(robots, r, c);
        if (idx != -1) {
            RobotBase* target = robots[idx].robot_instance;
            res.emplace_back(target->m_character, r, c);
        }
        r += dr;
        c += dc;
    }
    return res;
}

// Apply an attack originating from shooter index
void apply_shot(vector<LoadedRobot>& robots, int shooter_idx, int shot_r, int shot_c, vector<vector<char>>& board) {
    RobotBase* shooter = robots[shooter_idx].robot_instance;
    WeaponType wt = shooter->get_weapon();

    if (shot_r < 0 || shot_r >= BOARD_ROWS || shot_c < 0 || shot_c >= BOARD_COLS) {
        cout << shooter->m_name << " fired an invalid shot.\n";
        return;
    }

    auto damage_hit = [&](int target_idx, int dmg) {
        if (target_idx < 0) return;
        RobotBase* target = robots[target_idx].robot_instance;
        if (!robots[target_idx].alive) return;

        target->reduce_armor(1);
        int newh = target->take_damage(dmg);

        cout << "Shooting: " << shooter->m_name
             << " hits " << target->m_name
             << " for " << dmg << " damage. Health: " << newh << "\n";

        if (newh <= 0) {
            mark_robot_dead(robots[target_idx], board);
            cout << target->m_name << " is dead.\n";
        }
    };

    switch (wt) {
        case railgun: {
            for (size_t i = 0; i < robots.size(); ++i) {
                if (!robots[i].alive) continue;
                int rr, cc;
                robots[i].robot_instance->get_current_location(rr, cc);
                if (rr == shot_r) damage_hit((int)i, 12);
            }
            break;
        }
        case flamethrower: {
            for (int r = shot_r - 2; r <= shot_r + 1; ++r) {
                for (int c = shot_c - 1; c <= shot_c + 1; ++c) {
                    if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS) continue;
                    int idx = find_robot_at(robots, r, c);
                    if (idx != -1) damage_hit(idx, 8);
                }
            }
            break;
        }
        case grenade: {
            if (shooter->get_grenades() <= 0) {
                cout << shooter->m_name << " has no grenades left!\n";
                break;
            }
            for (int r = shot_r - 1; r <= shot_r + 1; ++r) {
                for (int c = shot_c - 1; c <= shot_c + 1; ++c) {
                    if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS) continue;
                    int idx = find_robot_at(robots, r, c);
                    if (idx != -1) damage_hit(idx, 18);
                }
            }
            shooter->decrement_grenades();
            break;
        }
        case hammer: {
            int idx = find_robot_at(robots, shot_r, shot_c);
            if (idx != -1) damage_hit(idx, 20);
            break;
        }
    }
}

char get_under_cell(const vector<vector<char>>& board, int r, int c) {
    char ch = board[r][c];
    if (ch == FLAME_OBS || ch == PIT_OBS || ch == MOUND_OBS || ch == 'X') return ch;
    return '.';
}

int main(int argc, char** argv) {
    cout << "RobotWarz arena starting...\n";

    // Discover Robot_*.cpp files
    vector<string> robot_cpp_files;
    for (auto &p : fs::directory_iterator(fs::current_path())) {
        if (!p.is_regular_file()) continue;
        string name = p.path().filename().string();
        if (name.rfind("Robot_", 0) == 0 && name.size() > 6 && name.find(".cpp") != string::npos) {
            robot_cpp_files.push_back(name);
        }
    }

    if (robot_cpp_files.empty()) {
        cerr << "No Robot_*.cpp files found in current directory.\n";
        return 1;
    }

    vector<LoadedRobot> robots;
    for (auto &cpp : robot_cpp_files) {
        LoadedRobot lr;
        lr.cpp_file = cpp;
        if (!compile_robot(cpp, lr.so_file)) {
            cerr << "Failed compiling " << cpp << " - skipping\n";
            continue;
        }

        lr.handle = dlopen(lr.so_file.c_str(), RTLD_LAZY);
        if (!lr.handle) {
            cerr << "dlopen failed for " << lr.so_file << ": " << dlerror() << "\n";
            continue;
        }

        RobotFactory create_robot = (RobotFactory)dlsym(lr.handle, "create_robot");
        if (!create_robot) {
            cerr << "dlsym create_robot failed in " << lr.so_file << ": " << dlerror() << "\n";
            dlclose(lr.handle);
            continue;
        }

        lr.robot_instance = create_robot();
        if (!lr.robot_instance) {
            cerr << "create_robot returned null for " << lr.so_file << "\n";
            dlclose(lr.handle);
            continue;
        }

        robots.push_back(move(lr));
    }

    if (robots.empty()) {
        cerr << "No robots loaded successfully.\n";
        return 1;
    }

    // Initialize board and obstacles
    vector<vector<char>> board(BOARD_ROWS, vector<char>(BOARD_COLS, '.'));
    place_obstacles(board);
    place_robots_random(robots, board);

    int round = 0;
    const int MAX_ROUNDS = 50000;

    while (true) {
        cout << "\n=========== starting round " << round << " ===========\n";
        print_board(board);

        // --- Each robot takes a turn ---
        for (size_t i = 0; i < robots.size(); ++i) {
            if (!robots[i].alive || !robots[i].robot_instance) continue;

            RobotBase* r = robots[i].robot_instance;
            cout << "\n" << r->m_name << " " << r->m_character << " begins turn.\n";
            cout << r->print_stats() << "\n";

            // Radar scanning
            vector<RadarObj> radar_results;
            int radar_dir = 0;
            r->get_radar_direction(radar_dir);

            if (radar_dir == 0) {
                int rr, rc;
                r->get_current_location(rr, rc);
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = rr + dr, nc = rc + dc;
                        if (nr < 0 || nr >= BOARD_ROWS || nc < 0 || nc >= BOARD_COLS) continue;
                        int idx = find_robot_at(robots, nr, nc);
                        if (idx != -1) radar_results.emplace_back(robots[idx].robot_instance->m_character, nr, nc);
                    }
                }
            } else {
                radar_results = do_radar_scan(r, radar_dir, board, robots);
            }

            r->process_radar_results(radar_results);

            // Shooting
            int shot_r = -1, shot_c = -1;
            if (r->get_shot_location(shot_r, shot_c)) {
                apply_shot(robots, (int)i, shot_r, shot_c, board);
                if (!robots[i].alive) {
                    int vr, vc;
                    r->get_current_location(vr, vc);
                    cout << r->m_name << " died from shooting damage. Skipping turn.\n";
                    continue;
                }
            }

            // --- Movement ---
            if (!robots[i].can_move_flag) {
                cout << r->m_name << " is trapped in a pit and cannot move.\n";
                continue;
            }

            robots[i].blocked_dirs.clear();
            int move_dir = 0, move_dist = 0;
            r->get_move_direction(move_dir, move_dist);

            if (move_dir < 0 || move_dir > 8 || move_dist <= 0 || move_dist > r->get_move_speed()) {
                cout << r->m_name << " chose invalid move (" << move_dir << "," << move_dist << "). Staying put.\n";
                continue;
            }

            int cur_r, cur_c;
            r->get_current_location(cur_r, cur_c);
            int dr = directions[move_dir].first;
            int dc = directions[move_dir].second;
            int new_r = cur_r, new_c = cur_c;
            bool blocked = false;

            for (int step = 0; step < move_dist; ++step) {
                int tr = new_r + dr;
                int tc = new_c + dc;
                if (tr < 0 || tr >= BOARD_ROWS || tc < 0 || tc >= BOARD_COLS
                    || board[tr][tc] == MOUND_OBS || board[tr][tc] == 'X'
                    || find_robot_at(robots, tr, tc) != -1) {
                    blocked = true;
                    break;
                }
                new_r = tr;
                new_c = tc;
            }

            if (blocked) {
                cout << "Moving: " << r->m_name << " blocked at (" << new_r << "," << new_c << "). Staying put.\n";
                if (robots[i].alive) board[cur_r][cur_c] = r->m_character;
            } else {
                char landed_cell = board[new_r][new_c];
                board[cur_r][cur_c] = get_under_cell(board, cur_r, cur_c);
                r->move_to(new_r, new_c);
                cout << "Moving: " << r->m_name << " moves to (" << new_r << "," << new_c << ").\n";

                if (landed_cell == FLAME_OBS) {
                    r->reduce_armor(1);
                    int health = r->take_damage(8);
                    if (health <= 0) {
                        mark_robot_dead(robots[i], board);
                        cout << r->m_name << " died in flames. Skipping turn.\n";
                        continue;
                    }
                }

                if (robots[i].alive) {
                    board[new_r][new_c] = r->m_character;
                }

                if (landed_cell == PIT_OBS) {
                    robots[i].can_move_flag = false;
                    r->disable_movement();
                    cout << r->m_name << " fell into a pit and cannot move for the rest of the game!\n";
                }
            }

            // Final alive check
            int vr, vc;
            r->get_current_location(vr, vc);
            if (r->get_health() <= 0) {
                mark_robot_dead(robots[i], board);
                cout << r->m_name << " has died.\n";
                continue;
            } if (robots[i].alive) {
                board[vr][vc] = r->m_character;
            }
        } // end robot loop

        // --- End-of-round check ---
        int alive_count = 0;
        LoadedRobot* last_alive = nullptr;
        for (auto &lr : robots) {
            if (lr.alive) {
                alive_count++;
                last_alive = &lr; // keep track of last alive robot
            }
        }

        if (alive_count <= 1 || round >= MAX_ROUNDS) {
            cout << "\nGame over after " << round << " rounds.\n";

            if (alive_count == 1 && last_alive && last_alive->robot_instance) {
                cout << "Winner: " << last_alive->robot_instance->m_name
                    << " (" << last_alive->robot_instance->m_character << ")!\n";
            } else {
                cout << "No winner.\n";
            }

            for (auto &lr : robots) {
                int rr, cc;
                if (lr.robot_instance) lr.robot_instance->get_current_location(rr, cc);
                cout << lr.robot_instance->m_name << " (" << lr.robot_instance->m_character << ") "
                    << (lr.alive ? "alive" : "dead") 
                    << " at (" << rr << "," << cc << ")\n";
            }
            break;
        }

        round++;
    } // end while

    // Cleanup
    for (auto &lr : robots) {
        if (lr.robot_instance) delete lr.robot_instance;
        if (lr.handle) dlclose(lr.handle);
    }

    cout << "Exiting RobotWarzArena.\n";
    return 0;
}
