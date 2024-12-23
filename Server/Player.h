#pragma once
#include <string>
#include <deque>

class Player
{
public:
	int id = 0;
	std::string name = "";
	int fd = -1;
	int score = 0;

	std::deque<std::string> bSend;
	std::string bRecv;
	int leftToRecv =0;
	int cmd = 0;
};

