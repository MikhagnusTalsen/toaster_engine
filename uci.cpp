#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "state_copy.h"
#include <cstdio>

// int main()
// {
//     // The ultimate Linux buffer-killer. This forces standard output to be unbuffered at the OS level.
//     setvbuf(stdout, NULL, _IONBF, 0);

//     std::string line;
//     while (std::getline(std::cin, line))
//     {
//         // Strip out the stringstream just for this test
//         if (line == "uci")
//         {
//             std::cout << "id name 1234" << std::endl;
//             std::cout << "id author toast" << std::endl;
//             std::cout << "uciok" << std::endl;
//         }
//         else if (line == "isready")
//         {
//             std::cout << "readyok" << std::endl;
//         }
//         else if (line == "quit")
//         {
//             break;
//         }
//     }
//     return 0;
// }

int main()
{
    // Turn off output buffering! (Forces text to go to the GUI instantly)
    // std::cout.setf(std::ios::unitbuf);

    // The ultimate Linux buffer-killer. This forces standard output to be unbuffered at the OS level.
    setvbuf(stdout, NULL, _IONBF, 0);

    init_all();
    std::string line;

    // std::cout << "UCI >>> ";
    while (std::getline(std::cin, line))
    {
        // put the line into a stringstream to easily split it by words
        std::stringstream ss(line);
        std::string token;

        // grab the very first word (uci, isready, position, go, quit)
        ss >> token;

        if (token == "uci")
        {
            std::cout << "id name engine_v1" << std::endl;
            std::cout << "id author toast" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (token == "isready")
        {
            std::cout << "readyok" << std::endl;
        }
        else if (token == "position")
        {
            std::string pos_type;
            ss >> pos_type;

            std::string fen = "";
            if (pos_type == "startpos")
            {
                fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            }
            else if (pos_type == "fen")
            {
                std::string fen_part;
                for (int i = 0; i < 6; i++)
                {
                    ss >> fen_part;
                    fen += fen_part + (i == 5 ? "" : " ");
                }
            }

            std::string moves_token;
            ss >> moves_token;

            std::vector<std::string> move_list;
            if (moves_token == "moves")
            {
                std::string move;
                while (ss >> move)
                {
                    move_list.push_back(move);
                }
            }
            set_position(fen, move_list);
        }
        else if (token == "go")
        {
            std::string param;
            int depth = 0, movetime = 0, wtime = 0, btime = 0, winc = 0, binc = 0;

            while (ss >> param)
            {
                if (param == "depth")
                    ss >> depth;
                else if (param == "movetime")
                    ss >> movetime;
                else if (param == "wtime")
                    ss >> wtime;
                else if (param == "btime")
                    ss >> btime;
                else if (param == "winc")
                    ss >> winc;
                else if (param == "binc")
                    ss >> binc;
            }

            if (depth > 0)
            {
                search_fixed_depth(depth);
            }
            else if (movetime > 0)
            {
                search_fixed_time(movetime);
            }
            else if (wtime > 0 || btime > 0)
            {
                search_time_control(wtime, btime, winc, binc);
            }
        }
        else if (token == "quit")
        {
            break;
        }
        // std::cout << "UCI >>> ";
    }
    return 0;
}

// int Main()
// {
//     while (true)
//     {
//         std::string in_cmd;
//         std::getline(std::cin, in_cmd);
//         // std::cin >> in_cmd;
//         int len = in_cmd.length();

//         if (in_cmd == "uci")
//         {
//             std::cout << "id name  : engine_v1\n";
//             std::cout << "id author: toast\n";
//             std::cout << "uciok\n";
//         }
//         else if (in_cmd == "isready")
//         {
//             std::cout << "readyok\n";
//         }
//         else if (in_cmd[0] == 'p')
//         {
//             std::string pos;
//             if (in_cmd[9] == 's')
//             {
//                 pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
//             }
//             else if (in_cmd[9] == 'f')
//             {
//                 size_t moves_pos = in_cmd.find("moves");
//                 if (moves_pos != std::string::npos)
//                 {
//                     pos = in_cmd.substr(13, moves_pos - 14);
//                 }
//                 else
//                 {
//                     pos = in_cmd.substr(13);
//                 }
//             }

//             std::vector<std::string> move_list;
//             size_t moves_pos = in_cmd.find("moves");
//             if (moves_pos != std::string::npos)
//             {
//                 int idx = moves_pos + 6;
//                 while (idx < len)
//                 {
//                     std::string move;
//                     while (idx < len && in_cmd[idx] != ' ')
//                     {
//                         move.append(1, in_cmd[idx++]);
//                     }
//                     idx++;
//                     move_list.push_back(move);
//                 }
//             }
//             state(pos, move_list);
//         }
//         else if (in_cmd[0] == 'g')
//         {
//             auto stoi = [&](int idx, int &num, std::string str)
//             {
//                 while (idx < len && in_cmd[idx] != ' ')
//                 {
//                     str.append(1, in_cmd[idx++]);
//                 }
//                 try
//                 {
//                     num = std::stoi(str);
//                 }
//                 catch (const std::invalid_argument &e)
//                 {
//                     std::cout << str;
//                     std::cout << " That wasn't a valid number!" << std::endl;
//                 }
//             };

//             if (in_cmd[3] == 'd')
//             {
//                 int idx = 9;
//                 std::string depth_s;
//                 int depth = 0;
//                 stoi(idx, depth, depth_s);
//                 state(depth);
//             }
//             else if (in_cmd[3] == 'm')
//             {
//                 int idx = 12;
//                 std::string time_s;
//                 int time = 0;
//                 stoi(idx, time, time_s);
//                 state(time);
//             }
//             // go wtime <W> btime <B> winc <WI> binc <BI>
//             else if (in_cmd[3] == 'w')
//             {
//                 int wtime = 0, btime = 0;
//                 int winc = 0, binc = 0;
//                 int idx = 0;
//                 std::string time_s;
//                 size_t found;

//                 found = in_cmd.find("wtime");
//                 if (found != std::string::npos)
//                 {
//                     idx = found + 6;
//                     stoi(idx, wtime, time_s);
//                 }

//                 found = in_cmd.find("btime");
//                 if (found != std::string::npos)
//                 {
//                     idx = found + 6;
//                     stoi(idx, btime, time_s);
//                 }

//                 found = in_cmd.find("winc");
//                 if (found != std::string::npos)
//                 {
//                     idx = found + 5;
//                     stoi(idx, winc, time_s);
//                 }

//                 found = in_cmd.find("binc");
//                 if (found != std::string::npos)
//                 {
//                     idx = found + 5;
//                     stoi(idx, binc, time_s);
//                 }

//                 std::string best_move = state(wtime, btime, winc, binc);

//                 std::cout << "bestmove " << best_move << "\n";
//             }
//         }
//         else if (in_cmd[0] == 'q')
//             break;
//     }
//     return 0;
// }
