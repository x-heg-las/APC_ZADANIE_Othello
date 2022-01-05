
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
const int32_t INFI = 400;
const int8_t NEIGHBOURS[] = { 1, 7, 8, 9, -1, -7, -8, -9 };
const std::unordered_map<char, std::set<int8_t>> MOVE_CLASSES{
	{'A', { 1, 7, 8, 9, -1,-7, -8, -9 }},
	{'B', { 1, 8, 9,}},
	{'C', {1, 7, 8, 9, -1}},
	{'D', {7, 8, -1}},
	{'E', {-8, -9, -1, 7, 8}},
	{'F', {-1, -9, -8}},
	{'G', {-1, -9, -8, -7, 1}},
	{'H', {-8, -7, 1}},
	{'I', {-8, -7, 1, 8, 9}}
};
const std::vector<char> ALLOWED_MOVES{
'B','C','C','C','C','C','C','D',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'I','A','A','A','A','A','A','E',
'H','G','G','G','G','G','G','F'
};
const std::vector<int8_t> FIELD_VALUES{
99,-8,8,6,6,8,-8,99,
-8,-24,-4,-3,-3,-4,-24,-8,
8,-4,7,4,4,7,-4,8,
6,-3,4,0,0,4,-3,6,
6,-4,4,0,0,4,-3,6,
8,-3,7,4,4,7,-4,8,
-8,-24,-4,-3,-4,-3,-24,-8,
99,-8,8,6,6,8,-8,99,
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
	std::pair<std::set<uint8_t>, std::unordered_map<int8_t, int8_t >[64]> moves;

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
						moves.first.insert(position);
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

auto analyzekMove(const std::string& board, uint8_t move, Player player, std::pair<uint8_t, uint8_t>& tokens)
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

char getToken(Player player)
{
	if (player == Player::BLACK)
	{
		return BLACK_CHIP;
	}
	else if (player == Player::WHITE)
	{
		return WHITE_CHIP;
	}
	return EMPTY_FIELD;
}

std::pair< std::pair<std::set<size_t>, std::set<size_t>>, int32_t> mobility(const std::string& board)
{
	//move options for WHITE and BLACK respectively
	std::pair<std::set<size_t>, std::set<size_t>> positions;
	int8_t stability[64] = { 1 };
	int32_t stabilityRate = 0;
	for (size_t i = 0; i < board.length(); ++i)
	{
		for (const auto& direction : MOVE_CLASSES.at(ALLOWED_MOVES.at(i)))
		{
			if (board.at(i) != getToken(appPlayer) && stability[i] == 1)
			{
				stability[i] = 0;
			}
			if (board.at(i) == EMPTY_FIELD)
			{
				char token = 0;
				int32_t tempPosition = static_cast<int32_t>(i) + direction;
				token = board.at(tempPosition);
				if (token == EMPTY_FIELD)
				{
					continue;
				}
				int8_t length = 0;
				while (board.at(tempPosition) != EMPTY_FIELD && board.at(tempPosition) == token &&
					MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition)).find(direction) != std::end(MOVE_CLASSES.at(ALLOWED_MOVES.at(tempPosition))))
				{
					++length;
					tempPosition += direction;
				}
				if (length > 0 && board.at(tempPosition) != token && board.at(tempPosition) != EMPTY_FIELD)
				{
					if (token == getToken(appPlayer))
					{
						for (int j = 1; j <= length; ++j)
						{
							stability[i + (direction * j)] = -1;
						}
					}
					if (token == BLACK_CHIP)
					{
						positions.first.insert(i);
					}
					else
					{
						positions.second.insert(i);
					}
				}
			}
		}
	}
	for (const auto field : stability)
	{
		stabilityRate += field;
	}
	return {positions, stabilityRate};
}

std::pair<int8_t, std::string> flip(const std::string& board, Player player, int8_t move, const std::unordered_map<int8_t, int8_t>& directions)
{
	std::string newBoard = board;
	char curToken = 0, enemyToken = 0;
	moveOption(player, curToken, enemyToken);
	
	//board after move is applied
	for (auto& entry : directions)
	{
		for (int8_t i = 0; i <= entry.second; ++i)
		{
			newBoard.at(move + (i * entry.first)) = curToken;
		}
	}
	return { FIELD_VALUES.at(move), newBoard };
}

int32_t evaluate(const std::string& board)
{

	char myToken = 0, enemyToken = 0;
	moveOption(appPlayer, myToken, enemyToken);
	/*for (size_t i = 0; i < board.length(); ++i)
	{
		if (board.at(i) == myToken)
		{
			mySpots += FIELD_VALUES.at(i);
		}
		else if(board.at(i) == enemyToken)
		{
			enemySpots += FIELD_VALUES.at(i);
		}
	}*/
	auto stabilityMetric = mobility(board);
	int32_t mobilityMetric = 0;
	if (appPlayer == Player::BLACK)
	{
		mobilityMetric = static_cast<int32_t>(stabilityMetric.first.first.size() - stabilityMetric.first.second.size());
	}
	else
	{
		mobilityMetric = static_cast<int32_t>(stabilityMetric.first.second.size() - stabilityMetric.first.first.size());
	}
	return stabilityMetric.second + mobilityMetric;
	
}

//best-firs alpha/beta pruning
int32_t findBestMove(const std::string& state, Player currentPlayer, int32_t depth, uint8_t iteration, int32_t alpha, int32_t beta, std::unordered_map<std::string, State>& transpositionTable)
{
	std::pair<uint8_t, uint8_t> tokens(static_cast<uint8_t>(0), static_cast<uint8_t>(0)); //player token and enemy token count

	//ends if leaf node or the state was already discovered
	if (depth == 0 ||
		(transpositionTable.contains(state) && transpositionTable[state].iteration == iteration && ((transpositionTable[state].myTurn && currentPlayer == appPlayer) || (!transpositionTable[state].myTurn && currentPlayer != appPlayer))))// || (!running.load()))
	{
		return evaluate(state) + depth;
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
			auto flipped = (flip(state, currentPlayer, move, moves.second[move]));
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
		int32_t value = -INFI - depth;
		for (auto reversedIt = transpositionTable[state].moves.rbegin(); reversedIt!= transpositionTable[state].moves.rend(); ++reversedIt)
		{
			value = std::max(value, reversedIt->first + findBestMove(reversedIt->second, enemy, depth - 1, iteration, alpha, beta, transpositionTable));
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
		int32_t value = INFI + depth;
		for (const auto& child : transpositionTable[state].moves)
		{
			value = std::min(value, child.first + findBestMove(child.second, enemy, depth - 1, iteration, alpha, beta, transpositionTable));
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

	for (auto a : availableMoves.first)
	{
		std::cout << std::to_string(a) << ", ";
	}
	for (uint8_t depth = 10, iteration = 0; depth < 11 && running.load(); depth += 4, iteration++)
	{
		std::clog << availableMoves.first.size() << std::endl;
		for (const auto& move : availableMoves.first)
		{
			auto nextStep = flip(args.at(1), appPlayer, move, availableMoves.second[move]);
			std::clog << nextStep.second << std::endl;
			auto value = findBestMove(nextStep.second, enemy, depth, iteration, -INFI, INFI, transpositionTable);
			if (value > best)
			{
				best = value;
				nextMove = move;
			}
		}
	}
	std::clog << "done" << std::endl;
	return MOVES_STRINGS.at(nextMove);
}



void timerThread()
{
	std::chrono::milliseconds playTime((gameTime * 1000) - 100);
	std::unique_lock<std::mutex> lock(initMutex);
	//waits until the timer is initialized during START operation
	initCv.wait(lock, [] {return error.load() || initialized.load() || stop.load(); });
	std::unique_lock<std::mutex> waitLock(waitMutex);
	if (!error.load() && initialized.load())
	{
		running.store(true);
		waitCv.notify_one();
		waitCv.wait_for(waitLock, playTime, []
		{
			return (error.load() || stop.load());
		});
		running.store(false);
		waitLock.unlock();
		waitCv.notify_one();
	}
}

int main()
{
	std::string command;
	while (std::getline(std::cin, command))
	{
		try
		{
			error.store(false);
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
				{
					std::thread timer(timerThread);

					{
						//wait until the timer is set
						std::unique_lock<std::mutex> lock(waitMutex);
						waitCv.wait(lock, [] {return running.load(); });
					}

					result = makeMove(command);
					stop.store(true);
					waitCv.notify_all();
					if (timer.joinable())
					{
						timer.join();
					}
					break;
				}
				case Command::ERR:
					error = true;
					std::clog << "Command error" << std::endl;
					return 1;
					break;
				case Command::STOP:
					return 0;
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
