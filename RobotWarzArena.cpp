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

struct LoadedRobot {
    string cpp_file;
    string so_file;
    void* handle = nullptr;
    RobotBase* robot_instance = nullptr;
    bool alive = false;
};

//Helper: compile a robot implementation (Robot_X.cpp) into a libRobot_X.so
bool compile_robot(const string& cpp, string& out_so) {
    string base = cpp.substr(0, cpp.find(".cpp"));
    out_so = "./lib" + base + ".so";
    string cmd = "g++ -shared -fPIC -o " + out_so + " " + cpp + " RobotBase.o -I. -std=c++20";
    cerr << "Compiling: " << cmd << "\n";
    return system(cmd.c_str()) == 0;
}

//Place robots on the board at random free cells
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
        cout << "Loaded robot: " << lr.robot_instance->m_name << " at (" << r << "," << c << ")\n";
    }
}

//Print board nicely
void print_board(const vector<vector<char>>& board) {
    cout << "\n=========== board ===========\n";
    cout << "    ";
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

//Helper to find robot index at a position
int find_robot_at(const vector<LoadedRobot>& robots, int row, int col) {
    for (size_t i = 0; i < robots.size(); ++i) {
        if (!robots[i].alive || !robots[i].robot_instance) continue;
        int rr, cc;
        robots[i].robot_instance->get_current_location(rr, cc);
        if (rr == row && cc == col) return (int)i;
    }
    return -1;
}

//Build radar results for a robot scanning in given direction
vector<RadarObj> do_radar_scan(RobotBase* robot, int direction,
                               const vector<vector<char>>& board,
                               const vector<LoadedRobot>& robots)
{
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
        r += dr; c += dc;
    }
    return res;
}

//Apply an attack originating from shooter index. The shooter already determined
// a shot row / col (via get_shot_location()). We apply damage according to weapon.
void apply_shot(vector<LoadedRobot>& robots, int shooter_idx, int shot_r, int shot_c) {
    RobotBase* shooter = robots[shooter_idx].robot_instance;
    WeaponType wt = shooter->get_weapon();

    auto damage_hit = [&](int target_idx, int dmg) {
        if (target_idx < 0) return;
        RobotBase* target = robots[target_idx].robot_instance;
        if (!robots[target_idx].alive) return;
        //Reduce armor by 1 (if > 0) and apply damage reduced by remaining armor
        target->reduce_armor(1);
        int newh = target->take_damage(dmg);
        cout << "Shooting: " << shooter->m_name << " hits " << target->m_name << " for " << dmg << " damage. Health: " << newh << "\n";
        if (newh <= 0) {
            cout << target->m_name << " is dead.\n";
            robots[target_idx].alive = false;
        }
    };

    switch (wt) {
        case railgun: {
            //damage every robot on the same row as shot across whole area
            for (size_t i = 0; i < robots.size(); ++i) {
                if (!robots[i].alive) continue;
                int rr, cc; robots[i].robot_instance->get_current_location(rr, cc);
                if (rr == shot_r) damage_hit((int)i, 12);
            }
            break;
        }
        case flamethrower: {
            //4x3 box centered on shot (clamped)
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
            //3x3 centered on shot
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
            // Single cell
            int idx = find_robot_at(robots, shot_r, shot_c);
            if (idx != -1) damage_hit(idx, 20);
            break;
        }
    }
}

int main(int argc, char** argv) {
    cout << "RobotWarz arena starting...\n";

    //Discover Robot_*.cpp files in current directory
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
        //Load .so
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

    //Simple board: '.' empty
    vector<vector<char>> board(BOARD_ROWS, vector<char>(BOARD_COLS, '.'));

    //Place robots
    place_robots_random(robots, board);

    // Main loop
    int round = 0;
    const int MAX_ROUNDS = 500;
    while (true) {
        cout << "\n=========== starting round " << round << " ===========\n";
        print_board(board);

        // Each robot gets a turn (in order)
        for (size_t i = 0; i < robots.size(); ++i) {
            if (!robots[i].alive || !robots[i].robot_instance) continue;
            RobotBase* r = robots[i].robot_instance;
            cout << "\n" << r->m_name << " " << r->m_character << " begins turn.\n";
            cout << r->print_stats() << "\n";

            // --- Radar scanning ---
            vector<RadarObj> radar_results;
            for (int dir = 1; dir <= 8; ++dir) {
                auto partial = do_radar_scan(r, dir, board, robots);
                radar_results.insert(radar_results.end(), partial.begin(), partial.end());
            }

            cout << " checking radar ...";
            if (radar_results.empty()) cout << "found nothing.\n";
            else {
                for (auto &ro : radar_results)
                    cout << " found '" << ro.m_type << "' at (" << ro.m_row << "," << ro.m_col << ") ";
                cout << "\n";
            }
            r->process_radar_results(radar_results);

            // --- Shooting ---
            int shot_r = -1, shot_c = -1;
            if (r->get_shot_location(shot_r, shot_c)) {
                cout << "Shooting: " << r->m_name << " aims at (" << shot_r << "," << shot_c << ")\n";
                apply_shot(robots, (int)i, shot_r, shot_c);
            }

            // --- Movement ---
            int cur_r, cur_c;
            r->get_current_location(cur_r, cur_c);

            // Clear the old cell only if robot moves
            board[cur_r][cur_c] = '.';

            int move_dir = 0, move_dist = 0;
            r->get_move_direction(move_dir, move_dist);
            if (move_dir != 0 && move_dist != 0) {
                int dr = directions[move_dir].first;
                int dc = directions[move_dir].second;

                int new_r = std::clamp(cur_r + dr * move_dist, 0, r->m_board_row_max - 1);
                int new_c = std::clamp(cur_c + dc * move_dist, 0, r->m_board_col_max - 1);

                // Block movement if target cell is occupied
                if (board[new_r][new_c] != '.') {
                    cout << "Moving: " << r->m_name << " blocked at (" << new_r << "," << new_c << ").\n";
                    board[cur_r][cur_c] = r->m_character; // stay in place
                } else {
                    r->move_to(new_r, new_c);
                    board[new_r][new_c] = r->m_character;
                    cout << "Moving: " << r->m_name << " moves to (" << new_r << "," << new_c << ").\n";
                }
            } else {
                // Stay put
                cout << "Moving: " << r->m_name << " stays put.\n";
                board[cur_r][cur_c] = r->m_character;
            }

            // --- Mark dead robots ---
            int vr, vc;
            r->get_current_location(vr, vc);
            if (r->get_health() <= 0) {
                board[vr][vc] = 'X'; // Dead robot marker
            } else {
                board[vr][vc] = r->m_character;
            }
        }

        // --- Check win condition ---
        int alive_count = 0;
        std::string winner;
        for (auto &lr : robots) if (lr.alive) { alive_count++; winner = lr.robot_instance->m_name; }

        if (alive_count <= 1) {
            cout << "\n=== Game over. ";
            if (alive_count == 1) cout << "Winner: " << winner << "\n";
            else cout << "No winners.\n";
            print_board(board);
            break; // End game
        }

        round++;
        if (round >= MAX_ROUNDS) {
            cout << "\n=== Maximum rounds reached (" << MAX_ROUNDS << "). Ending simulation ===\n";
            break;
        }

        if (round % 50 == 0) {
            cout << "\n--- Still running: round " << round << " ---\n";
        }

        std::cout << "Press Enter to continue to the next round...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }


    //cleanup
    for (auto &lr : robots) {
        if (lr.robot_instance) delete lr.robot_instance;
        if (lr.handle) dlclose(lr.handle);
    }

    return 0;
}