#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include "Player.h"
#include <sstream>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>



#define DEFAULT_BUFLEN 8192

//0 - 8 BLANK_COVERED adjacent to bombs
#define BLANK_COVERED_END 9
//10 - 18 BLANK_UNCOVERED adjacent to bombs
//20 - 28 BLANK MISFLAG

#define BOMB_COVERED 31
#define BOMB_UNCOVERED_OFF 32
#define BOMB_UNCOVERED_FLAG 33

#define GAME_STATE_IDLE 0
#define GAME_STATE_WAITING 1
#define GAME_STATE_GAME 2
#define GAME_STATE_OVER 3

#define SEND_LOGIN 49
#define SEND_LEFTCLICK 50
#define SEND_RIGHTCLICK 51
#define SEND_GAME_STATE 80
#define SEND_BOARD_DATA 81
#define SEND_SCORE_DATA 82
#define SEND_PLAYER_DATA 83
#define SEND_WRONG_NAME 90

typedef unsigned char byte;

int port = 3000;
int bomb_density = 4; //statistically one in BOMB_DENSITY is a bomb
int board_edge = 60;
int board_size = 60*60;

int blank_hit = 1;
int blank_flag = -10;
int bomb_hit = -10;
int bomb_flag = 10;
int blank_empty = 3;

int waiting_timeout = 5;
int game_timeout = 300;
int restart_timeout = 3;


int next_player_id = 11;
int unnamed_players = 0;
int covered_fields = board_size;
int* board = nullptr;

int LSock;

std::vector<pollfd> SocketsPollVect;
std::vector<Player*> PlayersVect;
std::vector<Player*> NewPlayersVect;
std::vector<Player*> DeletedPlayersVect;
std::vector<Player*> CombinedPlayersVect;
std::vector<Player*> ScoresUpdateVect;
std::vector<int> BoardUpdateVect;

char send_buffer[DEFAULT_BUFLEN];
int gameTimer = 0;
int gameState = GAME_STATE_IDLE;
int pollTimeout = 200; 

//wyszukuje playera po deskryptorze
Player* GetPlayer(int desc)
{
	for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
		if ((*iter)->fd == desc) return (*iter);
	}
	return nullptr;
}


//usuwa deskryptor z SocketsPollVect przed zamknięciem
void RemoveConnection(int desc)
{
	for (auto iter = SocketsPollVect.begin(); iter != SocketsPollVect.end(); ++iter) {
		if (iter->fd == desc)
		{
			iter = SocketsPollVect.erase(iter);
			return;
		}
	}
}

//usuwa z PlayersVect gracza, który sie rozłączył
void RemovePlayer(int desc)
{
	for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
		if ((*iter)->fd == desc) {
			Player* pl = (*iter);
			PlayersVect.erase(iter);

			if(pl->id == 0){
				for(auto it = CombinedPlayersVect.begin(); it != CombinedPlayersVect.end(); ++it) {
					if (((*it)->id == 0) && ((*it)->fd = pl->fd)) {
						CombinedPlayersVect.erase(it);
						break;
					}
				}
				unnamed_players--;
				delete pl;
			} else {	
				DeletedPlayersVect.push_back(pl);
			}

			return;
		}
	}
}


//odłącza wszystkich graczy po skończonej rozgrywce
void DisconnectAllPlayers()
{
	for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter)
	{
		Player* pl = (*iter);
		if(pl != nullptr){
			RemoveConnection(pl->fd);
			close(pl->fd);
			delete pl; 
		}

	}
	for (auto pl: DeletedPlayersVect){
		if(pl != nullptr)
			delete pl; 
	}
	PlayersVect.clear();
	DeletedPlayersVect.clear();
	CombinedPlayersVect.clear();
}


void SendWithPoll(Player* pl, int pos){

	std::string msg(send_buffer, pos);
	pl->bSend.push_back(msg);

	if(pl->bSend.size() <= 1){
		int PSock = pl->fd;
		for(auto iter = SocketsPollVect.begin(); iter != SocketsPollVect.end(); ++iter) {
			if(iter->fd == PSock){
				iter->events |= POLLOUT;
			}
		}
	}

}



bool IsNameAvailable(std::string name){
	for(auto iter = CombinedPlayersVect.begin(); iter != CombinedPlayersVect.end(); ++iter){
		Player* pl = (*iter);
		if (pl->id != 0){
			if(pl->name == name){
				return false;
			}
		}
	}
	return true;
}


void OnPlayer(int desc){
	Player* pl = new Player();
	pl->fd = desc;
	PlayersVect.push_back(pl);
	CombinedPlayersVect.push_back(pl);
	unnamed_players++;
}

//dodaje nowego gracza do NewPlayersVect
void OnNewPlayer(Player* pl, std::string name)
{
	if(IsNameAvailable(name)){
		pl->id = next_player_id++;
		pl->name = name;
		NewPlayersVect.push_back(pl);
		unnamed_players--;
	} else {
		send_buffer[0] = SEND_WRONG_NAME;
		std::fill(send_buffer+1, send_buffer+4,0);
		SendWithPoll(pl,4);
	}
}


void ChainedUncover(int field, int id){
	
	if(board[field] == 0){
		board[field] = id * 100 + 10; // add players id
		BoardUpdateVect.push_back(field);

		//sprawdzaj sąsiadów
		int fieldX = field % board_edge;
		int fieldY = field / board_edge;
		
		int row[8] = {-1,1,0,0,-1,-1,1,1};
		int col[8] = {0,0,-1,1,-1,1,-1,1};

		for(int i =0; i< 8; i++){
			int newX = fieldX+col[i];
			int newY = fieldY+row[i];

			if((newX >= 0 && newX < board_edge) 
				&& (newY >= 0 && newY < board_edge)){
				int newField = newX + newY*board_edge;
				if(board[newField] < BLANK_COVERED_END) ChainedUncover(newField, id);
			
			}

		}

	} else if (board[field] < BLANK_COVERED_END){
		//odkryj ale nie idź dalej
		board[field] = id * 100 + board[field] + 10; // add players id
		BoardUpdateVect.push_back(field);
		return;

	} 
	return;
}

//obsługuje nadesłane kliknięcia graczy, zmienione pola dodaje do BoardUpdateVect, wyniki do ScoresUpdateVect
void OnPlayersClick(Player* pl, int num, int cmd)
{
	int val = 0;
	if(board[num] == 0 && cmd == SEND_LEFTCLICK){
		val = blank_empty;
	} else if (board[num] < BLANK_COVERED_END)
	{
		if (cmd == SEND_LEFTCLICK)
		{
			val = blank_hit;
			board[num] += 10;
		}
		else
		{
			val = blank_flag;
			board[num] += 20;
		}
	}
	else if (board[num] == BOMB_COVERED)
	{
		if (cmd == SEND_LEFTCLICK)
		{
			val = bomb_hit;
			board[num] = BOMB_UNCOVERED_OFF;
		}
		else
		{
			val = bomb_flag;
			board[num] = BOMB_UNCOVERED_FLAG;
		}
	} else {
		return;
	}

	if(board[num] == 0){
		ChainedUncover(num, pl->id);
	} else {
		board[num] = pl->id * 100 + board[num]; // add players id
		BoardUpdateVect.push_back(num);
	}
	pl->score += val;
	if (pl->score < 0) pl->score = 0;
	ScoresUpdateVect.push_back(pl);

}


//obsługa komunikacji TCP IP 

bool MakeNonBlocking(int fd) {

	if(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0){
		return 0;
	}
	return 1;
}


bool StartServer()
{

	LSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (LSock <= 0) {
		perror("Unable to create listening socket");
		return false;
	}
	if (!MakeNonBlocking(LSock)) {
		perror("Unable to set listening socket to non-blocking");
		return false;
	}

	const int one = 1;
    setsockopt(LSock,SOL_SOCKET,SO_REUSEADDR,&one, sizeof(one));

	sockaddr_in LSockAddr {};
	LSockAddr.sin_family = AF_INET;
	LSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	LSockAddr.sin_port = htons(port);
	if (bind(LSock, (sockaddr*)&LSockAddr, sizeof(LSockAddr)) < 0) {
		perror("Unable to bind listening socket");
		return false;
	}

	if (listen(LSock, 10) < 0) {
		perror("Unable to open listening socket");
		return false;
	}

	pollfd listen_fd {};
	listen_fd.fd = LSock;
	listen_fd.events = POLLIN;
	SocketsPollVect.push_back(listen_fd);

	printf("Server is running...\n");
	return true;
}
bool PollSockets()
{
	int result = poll(SocketsPollVect.data(), SocketsPollVect.size(),pollTimeout);
	if (result < 0) {
		perror("Unable to poll");
		return false;
	}

	if (result == 0) 
		return true;


	for (auto iter = SocketsPollVect.begin(); iter != SocketsPollVect.end();) {
		Player* p;
		if(iter->revents & POLLOUT){
			p = GetPlayer(iter->fd);
			if(p != nullptr){
				
				auto& qS = p->bSend;

				while(!qS.empty()){

					int size = qS[0].size();
					int bytes_sent = send(iter->fd, qS.front().c_str(), size, 0);
					if(bytes_sent > 0){
						if(bytes_sent == size){
							qS.pop_front();
							if(qS.empty()) iter->events &= ~POLLOUT;
						} else {
							qS[0] = qS[0].substr(bytes_sent, size - 1);
							break;
						}

					}
					else if (bytes_sent == 0) {
						iter->revents |= POLLHUP;
						break;
					}
					else if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
						iter->revents |= POLLERR;
						break;
					} else {
						//when EWOULDBLOCK or EAGAIN
						break;
					}
				}
			}

		}



		if (iter->revents & POLLIN) {
			if (iter->fd == LSock) {
				// accept new client connection
				sockaddr_in CSockAddr {};
				socklen_t CSize = sizeof(CSockAddr);

				int CSock = accept(LSock, (sockaddr*)&CSockAddr, &CSize);
				if (CSock < 0) {
					perror("Unable to accept a client");
				}
				else if (!MakeNonBlocking(CSock)) {
					perror("Unable to set client socket to non-blocking");
					close(CSock);
				}
				else {
					printf("Client IP addr: %s:%i\n", inet_ntoa(CSockAddr.sin_addr), ntohs(CSockAddr.sin_port));


					pollfd client_fd {};
					client_fd.fd = CSock;
					client_fd.events=POLLIN;
					auto indeks = std::distance(iter, SocketsPollVect.begin()); 
					SocketsPollVect.push_back(client_fd); 
					iter = SocketsPollVect.begin() + indeks;

					OnPlayer(CSock);

					//printf("\tSize of fds after insert: %i, client %i\n\n", (int)SocketsPollVect.size(), CSock);
				}
			}
			else {
				// read from client connection
				p = GetPlayer(iter->fd);
				if(p != nullptr){
					if(p->leftToRecv > 0){ 
						char buffer[32];
						int bytes_read = recv(iter->fd, buffer, p->leftToRecv, 0);
						if (bytes_read == p->leftToRecv) {
							if (p->cmd == SEND_LOGIN)
							{
								std::string nameRest(buffer, bytes_read);
								std::string name = p->bRecv;
								name.append(nameRest);
								OnNewPlayer(p, name);
							}
							else if ((p->cmd == SEND_LEFTCLICK) || (p->cmd == SEND_RIGHTCLICK))
							{
								std::string valRest(buffer,bytes_read);
								std::string val = p->bRecv;
								const char* valCpy = val.append(valRest).c_str();
								char msg[3];
								std::copy(valCpy, valCpy + 3, msg);
								byte data1 = msg[0];
								byte data2 = msg[1];
								byte data3 = msg[2];
								int num = ((int)data1) | ((int)data2 << 8) | ((int)data3 << 16);
								OnPlayersClick(p, num, p->cmd);
							}
							p->cmd =0;
							p->leftToRecv =0;
							p->bRecv.clear();

						} else if (bytes_read > 0){
							std::string data(buffer,bytes_read);
							p->bRecv.append(data);
							p->leftToRecv = p->leftToRecv-bytes_read;
						} else if(bytes_read ==0) {
									iter->revents |= POLLHUP;
						}
						else if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) 
						{
							iter->revents |= POLLERR;
						}

					} 
					else 
					{
						//printf("Data received from client: %i\n\n", iter->fd);
						char buffer[1]; 
						int bytes_read = recv(iter->fd, buffer, sizeof(buffer), 0);
						if (bytes_read > 0) {

							int cmd = buffer[0];
							char* msg = nullptr;
							int msgSize =0;
							bytes_read = 0;
							if (cmd == SEND_LOGIN){
								msg = new char[32];
								msgSize = 32;
							} else if ((cmd == SEND_LEFTCLICK) || (cmd == SEND_RIGHTCLICK)){
								msg = new char[3];
								msgSize = 3;
							} 
							if(msg != nullptr) bytes_read = recv(iter->fd, msg, msgSize, 0);
								
								//printf("bytes read: %i\n",bytes_read);
								if (bytes_read > 0){
									if(bytes_read == msgSize){
										if (cmd == SEND_LOGIN) 
										{
											std::string name(msg, bytes_read);
											name = name.substr(0, name.length() - 1);

											OnNewPlayer(p, name);
										}
										else if ((cmd == SEND_LEFTCLICK) || (cmd == SEND_RIGHTCLICK))
										{
											byte data1 = msg[0];
											byte data2 = msg[1];
											byte data3 = msg[2];
											int num = ((int)data1) | ((int)data2 << 8) | ((int)data3 << 16);
											OnPlayersClick(p, num, cmd);
										}

										delete[] msg;
									} else {
										std::string data(msg,bytes_read);
										p->bRecv.append(data);
										p->leftToRecv = msgSize-bytes_read;
										p->cmd = cmd;
									}
								} 
								else if(bytes_read ==0) {
									iter->revents |= POLLHUP;
								}
								else if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) 
								{
									iter->revents |= POLLERR;
							
								}
						}
						else if (bytes_read == 0) {
							iter->revents |= POLLHUP;
						}
						else if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
							iter->revents |= POLLERR;
						}
					
					}
				} else {
					printf("No player with such file descriptor: %i\n", iter->fd);
				}
			}
		}
		if (iter->revents & (POLLERR | POLLHUP)) {
			if (iter->fd != LSock) {
				if (iter->revents & POLLHUP) {
					printf("Client disconnected: %i\n", iter->fd);
				}
				else {
					printf("Unable to read from client: %i\n", iter->fd);
				}
				int PSock = iter->fd;
				
				iter = SocketsPollVect.erase(iter);
				RemovePlayer(PSock);
				close(PSock);
				//printf("\tSize of fds after remove: %i, client %i\n\n", (int)SocketsPollVect.size(), PSock);
				continue;
			}
		}
		++iter;
	}
	return true;
}




//po zalogowaniu odsyła graczowi przyznany ID i rozmiar planszy
void SendPlayersID(Player* pl)
{
	char resp[9] = { SEND_LOGIN,5,0,0,(char)(pl->id), (char)(pl->id >> 8), (char)(pl->id >> 16),
	(char) board_edge, (char) (board_edge >> 8) };
	std::copy(resp, resp + 9, send_buffer);
	SendWithPoll(pl, 9);
	
}

//wysyła nowemu graczowi kompletny stan planszy
void SendBoard(Player* pl)
{
	send_buffer[0] = SEND_BOARD_DATA;// board setup
	int pos = 4;// [0] cmd [1,2,3] count
	int cnt = 0;
	//1364 to one buffer
	for (int i = 0; i < board_size; i++)
	{
		if (board[i] > BLANK_COVERED_END && board[i] != BOMB_COVERED) //all but covered 
		{
			send_buffer[pos++] = (byte)(i);
			send_buffer[pos++] = (byte)(i >> 8);
			send_buffer[pos++] = (byte)(i >> 16);
			send_buffer[pos++] = (byte)(board[i]);
			send_buffer[pos++] = (byte)(board[i] >> 8);
			send_buffer[pos++] = (byte)(board[i] >> 16);
			cnt++;
		}
		if (cnt > 1363)//limit reached
		{
			send_buffer[1] = (byte)(pos - 4);
			send_buffer[2] = (byte)((pos - 4) >> 8);
			send_buffer[3] = (byte)((pos - 4) >> 16);

			SendWithPoll(pl, pos);
			pos = 4;
			cnt = 0;
			
		}
	}
	if (cnt > 0)
	{
		send_buffer[1] = (byte)(pos - 4);
		send_buffer[2] = (byte)((pos - 4) >> 8);
		send_buffer[3] = (byte)((pos - 4) >> 16);

		SendWithPoll(pl, pos);
	}
}

//wysyła istniejącym graczom dane nowego gracza
void SendNewPlayerData(Player* pl)
{
	char buff[42];// 4b header, 3b id, 3b score, 32b name
	buff[0] = SEND_PLAYER_DATA;// players update
	int pos = 1;// [0] cmd [1,2,3] count
	buff[pos++] = 38;
	buff[pos++] = 0;
	buff[pos++] = 0;
	buff[pos++] = (byte)(pl->id);
	buff[pos++] = (byte)(pl->id >> 8);
	buff[pos++] = (byte)(pl->id >> 16);
	buff[pos++] = (byte)(pl->score);
	buff[pos++] = (byte)(pl->score >> 8);
	buff[pos++] = (byte)(pl->score >> 16);
	int len = pl->name.length() + 1;
	const char* name = pl->name.c_str();
	std::copy(name, name + len, buff + pos);
	std::copy(buff, buff + 42, send_buffer);
	for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter)
	{

		Player* plex = (*iter);
		if (plex == pl) continue;//dont send to itself
		if (plex->id == 0)continue;
		SendWithPoll(plex, 42);

	}
	
}

//wysyła nowemu graczowi dane wszystkich istn. graczy
void SendPlayersData(Player* pl)
{
	//200 to one buffer, one record = 38b; 3b id, 3b score, 32b name + 4b header
	send_buffer[0] = SEND_PLAYER_DATA;// players update
	int pos = 4;// [0] cmd [1,2,3] count 
	int cnt = 0;
	for (auto iter = CombinedPlayersVect.begin(); iter != CombinedPlayersVect.end(); ++iter)
	{
		Player* plex = (*iter);
		if (plex == pl) continue;//dont send to itself
		if (plex->id == 0)continue;
		send_buffer[pos++] = (byte)(plex->id);
		send_buffer[pos++] = (byte)(plex->id >> 8);
		send_buffer[pos++] = (byte)(plex->id >> 16);
		send_buffer[pos++] = (byte)(plex->score);
		send_buffer[pos++] = (byte)(plex->score >> 8);
		send_buffer[pos++] = (byte)(plex->score >> 16);
		int len = plex->name.length() + 1;
		const char* name = plex->name.c_str();
		std::copy(name, name + len, send_buffer + pos);
		pos += 32;
		cnt++;
		if (cnt > 200)//limit reached
		{
			send_buffer[1] = (byte)(pos-4);
			send_buffer[2] = (byte)((pos - 4) >> 8);
			send_buffer[3] = (byte)((pos - 4) >> 16);

			SendWithPoll(pl, pos);
			pos = 4;
			cnt = 0;
			
		}
	}
	if (cnt > 0)//remaining
	{
		send_buffer[1] = (byte)(pos - 4);
		send_buffer[2] = (byte)((pos - 4) >> 8);
		send_buffer[3] = (byte)((pos - 4) >> 16);

		SendWithPoll(pl, pos);
	}
}

//co sekundę wysyła graczom stan i czas bieżącej fazy rozgrywki
void SendGameState()
{
	char buff[7];// 4b header, 1b state, 2b time 
	buff[0] = SEND_GAME_STATE;// game state
	int pos = 1;// [0] cmd [1,2,3] count
	buff[pos++] = 3;
	buff[pos++] = 0;
	buff[pos++] = 0;
	buff[pos++] = (byte)(gameState);
	buff[pos++] = (byte)(gameTimer);
	buff[pos++] = (byte)(gameTimer >> 8);
	std::copy(buff, buff + 7, send_buffer);
	for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter)
	{
		Player* pl = (*iter);
		if (pl->id == 0)continue;
		
		SendWithPoll(pl, pos);
	}
	
}

//wysyła graczom aktualizację wyników (tylko zminione)
void SendScoresUpdate()
{
	send_buffer[0] = SEND_SCORE_DATA;// scores update
	int pos = 4;// [0] cmd [1,2,3] count
	int cnt = 0;
	//1364 to one buffer

	while (!ScoresUpdateVect.empty())
	{
		Player* pl = ScoresUpdateVect.back();
		send_buffer[pos++] = (byte)(pl->id);
		send_buffer[pos++] = (byte)(pl->id >> 8);
		send_buffer[pos++] = (byte)(pl->id >> 16);
		send_buffer[pos++] = (byte)(pl->score);
		send_buffer[pos++] = (byte)(pl->score >> 8);
		send_buffer[pos++] = (byte)(pl->score >> 16);
		ScoresUpdateVect.pop_back();
		cnt++;
		if (cnt > 1363)//limit reached
		{
			send_buffer[1] = (byte)(pos - 4);
			send_buffer[2] = (byte)((pos - 4) >> 8);
			send_buffer[3] = (byte)((pos - 4) >> 16);
			for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
				if ((*iter)->id == 0)continue;

				SendWithPoll((*iter), pos);
			}
			pos = 4;
			cnt = 0;
			
		}
	}
	if (cnt > 0)//remaining
	{
		send_buffer[1] = (byte)(pos - 4);
		send_buffer[2] = (byte)((pos - 4) >> 8);
		send_buffer[3] = (byte)((pos - 4) >> 16);
		for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
			if ((*iter)->id == 0)continue;

			SendWithPoll((*iter), pos);

		}
		
	}
}

//wysyła graczom dane zmienionych pól planszy
void SendBoardUpdate()
{
	send_buffer[0] = SEND_BOARD_DATA;// board update
	int pos = 4;// [0] cmd [1,2,3] count
	int cnt = 0;
	//1364 to one buffer

	while (!BoardUpdateVect.empty())
	{
		covered_fields--;
		int num = BoardUpdateVect.back();
		send_buffer[pos++] = (byte)(num);
		send_buffer[pos++] = (byte)(num >> 8);
		send_buffer[pos++] = (byte)(num >> 16);
		send_buffer[pos++] = (byte)(board[num]);
		send_buffer[pos++] = (byte)(board[num] >> 8);
		send_buffer[pos++] = (byte)(board[num] >> 16);
		BoardUpdateVect.pop_back();
		cnt++;
		if (cnt > 1363)//limit reached
		{
			send_buffer[1] = (byte)(pos - 4);
			send_buffer[2] = (byte)((pos - 4) >> 8);
			send_buffer[3] = (byte)((pos - 4) >> 16);
			for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
				if ((*iter)->id == 0)continue;

				SendWithPoll((*iter), pos);

			}
			pos = 4;
			cnt = 0;
			
		}
	}
	if (cnt > 0)//remaining
	{
		send_buffer[1] = (byte)(pos - 4);
		send_buffer[2] = (byte)((pos - 4) >> 8);
		send_buffer[3] = (byte)((pos - 4) >> 16);
		for (auto iter = PlayersVect.begin(); iter != PlayersVect.end(); ++iter) {
			if ((*iter)->id == 0)continue;

			SendWithPoll((*iter), pos);
			
		}
	}

	if (covered_fields <= 0 && gameState == GAME_STATE_GAME){
			gameState = GAME_STATE_OVER;
			gameTimer = restart_timeout;
			std::cout << "Game state: GAME_STATE_OVER" << std::endl;
	}
}


void UpdateAdjacentBombs(int field){
	int fieldX = field % board_edge;
	int fieldY = field / board_edge;
	
	int row[8] = {-1,1,0,0,-1,-1,1,1};
	int col[8] = {0,0,-1,1,-1,1,-1,1};

	for(int i =0; i< 8; i++){
		int newX = fieldX+col[i];
		int newY = fieldY+row[i];

		if((newX >= 0 && newX < board_edge) 
			&& (newY >= 0 && newY < board_edge)){
			int newField = newX + newY*board_edge;
			if(board[newField] != BOMB_COVERED) board[newField]++;
		
		}
	}
}



//obsługuje aktualizacje stanu gry co 1s
void OneSecondTimer()
{
	if (gameState == GAME_STATE_WAITING) //waitin for more players
	{
		gameTimer--;
		if (gameTimer < 0)
		{
			gameState = GAME_STATE_GAME;
			gameTimer = game_timeout;
			std::cout << "Game state: GAME_STATE_GAME" << std::endl;
		}
	}
	else if (gameState == GAME_STATE_GAME)
	{
		gameTimer--;
		if (gameTimer < 0)
		{
			gameState = GAME_STATE_OVER;
			gameTimer = restart_timeout;
			std::cout << "Game state: GAME_STATE_OVER" << std::endl;
		}
	}
	else if (gameState == GAME_STATE_OVER)
	{
		gameTimer--;
		if (gameTimer < 0)
		{
			gameState = GAME_STATE_IDLE;
			SendGameState();//has connections still
			DisconnectAllPlayers();
			std::cout << "Game state: GAME_STATE_IDLE"<< std::endl;
		}
	}
	SendGameState();
}

//obsługuje aktualizacje stanu gry
void OneTickTimer()
{
	if (gameState == GAME_STATE_IDLE) //zero players
	{
		if (PlayersVect.size()-unnamed_players > 0)
		{
			gameState = GAME_STATE_WAITING;
			gameTimer = waiting_timeout;
			//prepare the board
			covered_fields=board_size;
			std::fill(board,board + board_size, 0);
			for (int i = 0; i < board_size; i++)
			{
				int liczba_losowa = rand() % bomb_density;
				if (liczba_losowa == 1) {

					board[i] = BOMB_COVERED;
					UpdateAdjacentBombs(i);
				}
								
			}	

			std::cout << "Game state: GAME_STATE_WAITING"<< std::endl;
		}
	}	
	else if (PlayersVect.size()-unnamed_players == 0)//no players connected
	{
		gameState = GAME_STATE_IDLE;
		std::cout << "Game state: GAME_STATE_IDLE"<< std::endl;
		
		DeletedPlayersVect.clear();
		CombinedPlayersVect.clear();
	}

	while (!NewPlayersVect.empty())
	{
		Player* pl = NewPlayersVect.back();
		SendPlayersID(pl);
		SendBoard(pl);
		SendNewPlayerData(pl);
		SendPlayersData(pl);
		NewPlayersVect.pop_back();
	}

	if (!BoardUpdateVect.empty())
	{
		SendBoardUpdate();
	}

	if (!ScoresUpdateVect.empty())
	{
		SendScoresUpdate();
	}

}



bool loadConfig(){

	std::string line;
	std::ifstream file("config.txt");
    if (file.is_open()) {
		while(std::getline(file, line) ){
			
			line.erase(0, line.find_first_not_of(" \t")); 
			line.erase(line.find_last_not_of(" \t") + 1);
			if (line.empty() || line.substr(0,2) == "//") continue;

			std::istringstream is_line(line);
			std::string key;


			if(std::getline(is_line,key,'=')){
				std::string value;

				if(std::getline(is_line, value)){

					key.erase(0, key.find_first_not_of(" \t")); 
					key.erase(key.find_last_not_of(" \t") + 1); 
					value.erase(0, value.find_first_not_of(" \t")); 
					value.erase(value.find_last_not_of(" \t") + 1); 


					try {
						if (key == "port"){
						port= std::stoi(value);
						if(port > 65535 || port < 1024){
							port = 3000;
						}
							
						} else if (key == "bomb_density"){
							bomb_density= std::stoi(value);
							if(bomb_density > 10 || bomb_density < 2) bomb_density = 3;

						} else if (key == "board_edge"){
							board_edge= std::stoi(value);
							if(board_edge > 512 || board_edge < 20) board_edge = 60;
							board_size = board_edge*board_edge;
							covered_fields = board_size;
							board = new int[board_size];
						} else if (key == "blank_hit"){
							blank_hit= std::stoi(value);
						} else if (key == "blank_flag"){
							blank_flag= std::stoi(value);
						} else if (key == "bomb_hit"){
							bomb_hit= std::stoi(value);
						} else if (key == "bomb_flag"){
							bomb_flag= std::stoi(value);
						} else if (key == "blank_empty"){
							blank_empty= std::stoi(value);
						} else if (key == "waiting_timeout"){
							waiting_timeout= std::stoi(value);
							if(waiting_timeout < 0) waiting_timeout = 5;
						} else if (key == "game_timeout"){
							game_timeout= std::stoi(value);
							if(game_timeout < 0) game_timeout = 300;
						} else if (key == "restart_timeout"){
							restart_timeout= std::stoi(value);
							if(restart_timeout < 0) restart_timeout = 3;

						} else {
							std::cout << "Unknown configuration key" << key << std::endl;
						}
					} catch(const std::invalid_argument& e){
						std::cerr << "Invalid config argument: " << e.what() <<std::endl;
					} catch (const std::exception& e) {
						std::cerr << "Config error: " << e.what() <<std::endl;
					}

				}
			}
		}

	} else{
		std::cout << "Could not open the configuration file\n";
		return false;
	}

	return true;
}




int main()
{
	std::cout << "MinerServer on\n";
	srand(time(0));
	
	loadConfig();
	if (!StartServer())
	{
		std::cout << "Unable to start server" <<std::endl;
		return -1;
	}

	auto currentMillis = std::chrono::steady_clock::now();
	auto prevMillisSec = currentMillis;

	while (1) {
		if(!PollSockets()) return -1;

		currentMillis = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentMillis - prevMillisSec);
		pollTimeout = ((1000 - elapsed.count()) < 200) ? (1000 - elapsed.count()) : 200;
		if (elapsed.count() >= 1000)
		{
			OneSecondTimer();
			prevMillisSec += std::chrono::milliseconds(1000)*(elapsed.count()/1000);
		}
		
		OneTickTimer();

	}
	close(LSock);
	delete[] board;

	return 0;
}

