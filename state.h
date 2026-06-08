#pragma once
#include <string>
#include <vector>

void init_all();
void set_position(std::string fen, std::vector<std::string> &moves);
void search(int depth, int movetime, int wtime, int btime, int winc, int binc);
void clear_TT();
