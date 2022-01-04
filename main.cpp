
#include <iostream>
#include <string>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <thread>
#include <sstream>
#include <vector>
#include <chrono>
#include <set>
#include <utility>

enum class Command
{
	START,
	MOVE,
	STOP,
	ERR
};

enum class Player
{
	NONE,
	BLACK,
	WHITE,
};

struct State
{
	bool myTurn;
	uint8_t iteration;
	std::set<std::pair<int8_t, std::string>> moves{};
};

const std::vector<std::string> MOVES_STRINGS{ "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8",
"B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8",
"C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8",
"D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8",
"E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8",
"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
"G1", "G2", "G3", "G4", "G5", "G6", "G7", "G8",
"H1", "H2", "H3", "H4", "H5", "H6", "H7", "H8"};
const char BLACK_CHIP = 'X';
const char WHITE_CHIP = 'O';
const char EMPTY_FIELD = '-';
const size_t BOARD_SIZE = 8;
const int8_t NEIGHBOURS[] = { 1, 7, 8, 9, -1,  7, -8, -9 };
const std::unordered_map<char, std::set<int8_t>> MOVE_CLASSES{
	{'A', { 1, 7, 8, 9, -1, 7, -8, -9 }},
	{'B', { 1, 8, 9,}},
	{'C', {1, 7, 8, 9, -1}},
	{'D', {7, 8, -1}},
	{'E', {-8, -9, -1, 7, 8}},
	{'F', {-1, -9, -8}},
	{'G', {-1, -9, -8, -7, 1}},
	{'H', {-8, -7, 1}},
	{'I', {-8, -7, 1, 8, 9}}
};
const std::vector<char> ALLOWED_MOVES{'B', 'C','C','C','C','C','C','D',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'H','G','G','G','G','G','G','F'
};
const std::vector<int8_t> FIELD_VALUES{15,0,2,2,2,2,0,15,
0,-1,0,0,0,0,-1,0,
2,0,3,2,2,3,0,2,
2,0,2,3,3,2,0,2,
2,0,2,3,3,2,0,2,
2,0,3,2,2,3,0,2,
0,-1,0,0,0,0,-1,0,
15,0,2,2,2,2,0,15,
};

size_t gameTime = 0;
bool gameStarted = false;
Player appPlayer;
std::atomic_bool running = false, initialized = false, error = false, stop = false;
std::condition_variable waitCv, initCv;
std::mutex waitMutex, initMutex;

std::vector<std::string> splitString(const std::string& line)
{
	std::stringstream stream(line);
	std::vector<std::string> words;
	std::string word;
	while (stream >> word)
	{
		words.push_back(word);
	}
	return words;
}

Command parseCommand(const std::string& command)
{
	std::stringstream stream(command);
	std::string operation;
	stream >> operation;
	
	if (operation == "START" && (!gameStarted))
	{
		return Command::START;
	}
	else if (operation == "MOVE" && gameStarted)
	{
		return Command::MOVE;
	}
	else if (operation == "STOP")
	{
		return Command::STOP;
	}

	return Command::ERR;
}


void initGame(const std::string& command)
{
	try
	{
		if (!gameStarted)
		{
			auto args = splitString(command);
			if (args.size() % 3)
			{
				std::clog << "Wrong command arguments" << std::endl;
			}
			gameTime = std::stoi(args.at(2));
			if (args.at(1) == "B")
			{
				appPlayer = Player::BLACK;
			}
			else if (args.at(1) == "W")
			{
				appPlayer = Player::WHITE;
			}
			else
			{
				appPlayer = Player::NONE;
				std::clog << "Wrong player parameter" << std::endl;
			}
			if (appPlayer != Player::NONE)
			{
				std::lock_guard<std::mutex> lk(initMutex);
				initialized.store(true);
			}
			gameStarted = true;
		}
		else
		{
			std::lock_guard<std::mutex> lk(initMutex);
			error.store(true);
			std::clog << "Game has already been started" << std::endl;
		}
		initCv.notify_one();
	}
	catch (std::exception& err)
	{
		std::clog << err.what() << std::endl;
		appPlayer = Player::NONE;
		error.store(false);
		std::exit(1);
	}
}

void moveOption(Player current,char& cur, char& enemy)
{
	if (current == Player::BLACK)
	{
		cur = 'X';
		enemy = 'O';
	}
	else
	{
		cur = 'O';
		enemy = 'X';
	}
}

auto findAvailableMoves(const std::string& board, Player player, std::pair<uint8_t, uint8_t>& tokens)
{
	char token = 0, enemyToken = 0;
	moveOption(player, token, enemyToken);
	std::pair<std::vector<uint8_t>, std::unordered_map<int8_t, int8_t >[64]> moves;

	for (uint8_t position = 0; position < board.length(); ++position)
	{
		if (board.at(position) == BLACK_CHIP)
		{
			tokens.first++;
		}
		else if (board.at(position) == WHITE_CHIP)
		{
			tokens.second++;
		}

		if (board.at(position) == EMPTY_FIELD)
		{
			for (const auto& direction : MOVE_CLASSES.at(ALLOWED_MOVES.at(position)))
			{
				size_t tempPosition = static_cast<size_t>(position) + direction;
				if (board.at(position + direction) == enemyToken && !moves.second[position].contains(direction))
				{
					int8_t length = 0;
					while (board.at(tempPosition) == enemyToken &&
						MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition)).find(direction) != std::end(MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition))))
					{
						length++;
						tempPosition += direction;
					}
					if (length > 0 && board.at(tempPosition) == token)
					{
						moves.second[position][direction] = length;
						moves.first.push_back(position);
					}
					else if (length > 0 && board.at(tempPosition) == EMPTY_FIELD)
					{
						//to avoid searching same line in reversed direction
						moves.second[tempPosition][-direction] = 0;
					}
				}
			}
		}
	}
	return moves;
}

auto analyzeMove(const std::string& board, uint8_t move, Player player, std::pair<uint8_t, uint8_t>& tokens)
{
	char token = 0, enemyToken = 0;
	moveOption(player, token, enemyToken);
	std::unordered_map<int8_t, int8_t> moves;

	for (uint8_t position = 0; position < board.length(); ++position)
	{
		if (board.at(position) == BLACK_CHIP)
		{
			tokens.first++;
		}
		else if (board.at(position) == WHITE_CHIP)
		{
			tokens.second++;
		}

		if (board.at(position) == EMPTY_FIELD && position == move)
		{
			for (const auto& direction : MOVE_CLASSES.at(ALLOWED_MOVES.at(position)))
			{
				size_t tempPosition = static_cast<size_t>(position) + direction;
				if (board.at(position + direction) == enemyToken && !moves.contains(direction))
				{
					int8_t length = 0;
					while (board.at(tempPosition) == enemyToken &&
						MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition)).find(direction) != std::end(MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition))))
					{
						length++;
						tempPosition += direction;
					}
					if (length > 0 && board.at(tempPosition) == token)
					{
						moves[direction] = length;
					}
				}
			}
		}
	}
	return moves;
}

std::pair<int8_t, std::string> flip(const std::string& board, int8_t availableMoves, int8_t move, const std::unordered_map<int8_t, int8_t>& directions)
{
	int8_t value = (FIELD_VALUES.at(move)) + availableMoves;
	std::string newBoard = board;
	char curToken = 0, enemyToken = 0;
	moveOption(appPlayer, curToken, enemyToken);

	for (auto& entry : directions)
	{
		for (int8_t i = 0; i <= entry.second; ++i)
		{
			newBoard.at(move + (i * entry.first)) = curToken;
		}
		//TODO : uprav vypocet metriky
	}
	return { value, newBoard };
}


int8_t evaluate(const std::string& board)
{
	double mySpots = 0, enemySpots = 0;
	char myToken = 0, enemyToken = 0;
	moveOption(appPlayer, myToken, enemyToken);
	for (size_t i = 0; i < board.length(); ++i)
	{
		if (board.at(i) == myToken)
		{
			mySpots += FIELD_VALUES.at(i);
		}
		else if(board.at(i) == enemyToken)
		{
			enemySpots += FIELD_VALUES.at(i);
		}
	}
	double ratio = ((mySpots - enemySpots) / (mySpots + enemySpots)) * 100;
	return static_cast<int8_t>(ratio);
	
}

//best-firs alpha/beta pruning
int32_t findBestMove(const std::string& state, Player currentPlayer, int32_t depth, uint8_t iteration, int32_t alpha, int32_t beta, std::unordered_map<std::string, State>& transpositionTable)
{
	std::pair<uint8_t, uint8_t> tokens(static_cast<uint8_t>(0), static_cast<uint8_t>(0)); //player token and enemy token count

	//ends if leaf node or the state was already discovered
	if (depth == 0 || (transpositionTable.contains(state) && transpositionTable[state].iteration == iteration))// || (!running.load()))
	{
		return evaluate(state);
	}

	State newState;
	newState.iteration = iteration;
	if (!transpositionTable.contains(state) ||
	(transpositionTable.contains(state) && ((transpositionTable[state].myTurn && currentPlayer != appPlayer) || (!transpositionTable[state].myTurn && currentPlayer == appPlayer))))
	{
		auto moves = findAvailableMoves(state, currentPlayer, tokens);
		for (const auto& move : moves.first)
		{
			//nodes.insert(evaluate(state, currentPlayer, move, moves.second[move], tokens));
			auto flipped = (flip(state, static_cast<int8_t>(moves.first.size()), move, moves.second[move]));
			newState.moves.insert(flipped);
		}
	}

	Player enemy = Player::BLACK;
	if (currentPlayer == Player::BLACK)
	{
		enemy = Player::WHITE;
	}

	if (currentPlayer == appPlayer)
	{
		newState.myTurn = true;
		transpositionTable[state] = newState;
		int32_t value = INT_MIN;
		for (auto reversedIt = transpositionTable[state].moves.rbegin(); reversedIt!= transpositionTable[state].moves.rend(); ++reversedIt)
		{
			value = std::max(value, findBestMove(reversedIt->second, enemy, depth - 1, iteration, alpha, beta, transpositionTable));
			alpha = std::max(alpha, value);
			if (value >= beta)
			{
				break;
			}
		}
		return value;
	}
	else
	{
		newState.myTurn = false;
		transpositionTable[state] = newState;
		int32_t value = INT_MAX;
		for (const auto& child : transpositionTable[state].moves)
		{
			value = std::min(value, findBestMove(child.second, enemy, depth - 1, iteration, alpha, beta, transpositionTable));
			beta = std::min(beta, value);
			if(value <= alpha)
			{
				break;
			}
		}
		return value;
	}
}

std::string makeMove(const std::string& command)
{
	auto args = splitString(command);
	if (args.size() % 2)
	{
		std::clog << "Insufficient arguments" << std::endl;
		std::exit(1);
	}
	
	if (args.at(1).length() % 64 || args.at(1).find_first_not_of("-OX") != std::string::npos)
	{
		std::clog << "Wrong game state parameter" << std::endl;
		std::exit(1);
	}
	std::unordered_map<std::string, State> transpositionTable;
	std::pair<uint8_t, uint8_t> tokens(static_cast<uint8_t>(0), static_cast<uint8_t>(0)); //player token and enemy token count
	int32_t best = -INT_MAX;
	uint8_t nextMove = 0;
	auto availableMoves = findAvailableMoves(args.at(1), appPlayer, tokens);
	
	if (availableMoves.first.empty())
	{
		std::clog << "No available moves" << std::endl;
		std::exit(1);
	}

	Player enemy = Player::BLACK;
	if (enemy == appPlayer)
	{
		enemy = Player::WHITE;
	}

	std::clog << running.load() << std::endl;
	for (uint8_t depth = 7, iteration = 0; depth < 8; depth += 4, iteration++)
	{
		std::clog << availableMoves.first.size() << std::endl;
		for (const auto& move : availableMoves.first)
		{
			auto nextStep = flip(args.at(1), static_cast<int8_t>(availableMoves.first.size()), move, availableMoves.second[move]);
			std::clog << nextStep.second << std::endl;
			auto value = findBestMove(nextStep.second, enemy, depth, iteration, INT_MIN, INT_MAX, transpositionTable);
			if (value > best)
			{
				best = value;
				nextMove = move;
			}
		}
	}
	return MOVES_STRINGS.at(nextMove);
}

void timerThread()
{
	std::chrono::seconds playTime(gameTime);
	std::unique_lock<std::mutex> lock(initMutex);
	//waits until the timer is initialized during START operation
	initCv.wait(lock, [] {return error.load() || initialized.load() || stop.load(); });
	std::unique_lock<std::mutex> waitLock(waitMutex);
	if (!error.load() && initialized.load())
	{
		
		waitCv.wait_for(waitLock, playTime, []
		{
			running.store(true);
			return (error.load() || stop.load());
		});
		running.store(false);
	}
	waitLock.unlock();
}

int main()
{
	std::string command;

	while (std::getline(std::cin, command))
	{
		try
		{
			error.store(false);
			std::thread timer(timerThread);
			std::string result;
			switch (parseCommand(command))
			{
				case Command::START:
					initGame(command);
					if (appPlayer != Player::NONE)
					{
						result = "1";
					}
					else
					{
						error.store(true);
					}
					break;
				case Command::MOVE:

				//{
				//	//wait until the timer is set
				//	std::unique_lock<std::mutex> lock(waitMutex);
				//	waitCv.wait(lock, [] {return running.load(); });
				//}

					result = makeMove(command);
					break;
				case Command::ERR:
					error = true;
					std::clog << "Command error" << std::endl;
					return 1;
					break;
				case Command::STOP:
					waitCv.notify_all();
					if (timer.joinable())
					{
						timer.join();
					}
					return 0;
			}

			stop.store(true);
			if (timer.joinable())
			{
				waitCv.notify_one();
				timer.join();
			}
			std::cout << result << std::endl;
		}
		catch(std::exception& err)
		{
			std::clog << err.what() << std::endl;
			return 1;
		}
	}
	return 0;
}
