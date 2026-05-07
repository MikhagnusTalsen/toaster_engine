#pragma once
#include <string>
#include <vector>

void init_all();
void set_position(std::string fen, std::vector<std::string> &moves);
void search_fixed_depth(int depth);
void search_fixed_time(int time_ms);
void search_time_control(int wtime, int btime, int winc, int binc);
