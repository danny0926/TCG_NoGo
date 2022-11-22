/**
 * Framework for NoGo and similar games (C++ 11)
 * episode.h: Data structure for storing an episode
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <chrono>
#include <numeric>
#include "board.h"
#include "action.h"
#include "agent.h"

class episode {
public:
	episode() : ep_state(initial_state()), ep_score(0), ep_time(0) {
		ep_moves.reserve(board::size_x * board::size_y);
	}

public:
	board& state() { return ep_state; }
	const board& state() const { return ep_state; }
	board::score score() const { return ep_score; }

	void open_episode(const std::string& tag) {
		ep_open = { tag, millisec() };
	}
	void close_episode(const std::string& tag) {
		ep_close = { tag, millisec() };
	}
	bool apply_action(action move) {
		board::reward reward = move.apply(state());
		if (reward != board::legal) return false;
		ep_moves.emplace_back(move, reward, millisec() - ep_time);
		ep_score += reward;
		return true;
	}
	agent& take_turns(agent& black, agent& white) {
		ep_time = millisec();
		return (step() % 2) ? white : black;
	}
	agent& last_turns(agent& black, agent& white) {
		return take_turns(white, black);
	}

public:
	size_t step(unsigned who = -1u) const {
		int size = ep_moves.size();
		switch (who) {
		case board::black:
		case action::black::type: return (size / 2) + (size % 2);
		case board::white:
		case action::white::type: return (size / 2);
		case action::place::type:
		default:                  return size;
		}
	}

	time_t time(unsigned who = -1u) const {
		time_t time = 0;
		switch (who) {
		case board::black:
		case action::black::type:
			for (size_t i = 0; i < ep_moves.size(); i += 2) time += ep_moves[i].time;
			break;
		case board::white:
		case action::white::type:
			for (size_t i = 1; i < ep_moves.size(); i += 2) time += ep_moves[i].time;
			break;
		case action::place::type:
		default:
			time = ep_close.when - ep_open.when;
			break;
		}
		return time;
	}

	std::vector<action> actions(unsigned who = -1u) const {
		std::vector<action> res;
		switch (who) {
		case board::black:
		case action::black::type:
			for (size_t i = 0; i < ep_moves.size(); i += 2) res.push_back(ep_moves[i]);
			break;
		case board::white:
		case action::white::type:
			for (size_t i = 1; i < ep_moves.size(); i += 2) res.push_back(ep_moves[i]);
			break;
		case action::place::type:
		default:
			res.assign(ep_moves.begin(), ep_moves.end());
			break;
		}
		return res;
	}

public:

	friend std::ostream& operator <<(std::ostream& out, const episode& ep) {
		out << '(';
		out << ";FF[4]CA[UTF-8]AP[TCG-NoGo-Demo]";
		out << "SZ[" << board::size_y;
		if (board::size_x != board::size_y) out << ':' << board::size_x;
		out << "]KM[0]";
		std::string names = ep.ep_open.tag;
		out << "PB[" << names.substr(0, names.find(':')) << "]";
		out << "PW[" << names.substr(names.find(':') + 1) << "]";
		time_t date = ep.ep_open.when / 1000;
		out << "DT[" << std::put_time(std::localtime(&date), "%Y-%m-%d") << "]";
		std::string winner = ep.ep_close.tag;
		out << "RE[" << (names.find(winner) == 0 ? "B" : "W") << "+R]";
		out << "C[TCG|" << ep.ep_open << "|" << ep.ep_close << "]";
		for (const move& mv : ep.ep_moves) out << mv;
		out << ')';
		return out;
	}
	friend std::istream& operator >>(std::istream& in, episode& ep) {
		ep = {};
		std::string token;
		std::getline(in, token, ')');
		token.erase(0, token.find('(') + 1);
		if (token.find("C[TCG|") != std::string::npos) {
			std::stringstream ss(token.substr(token.find("C[TCG|")));
			ss.ignore(6); // C[TCG|
			ss >> ep.ep_open;
			ss.ignore(1); // |
			ss >> ep.ep_close;
			ss.ignore(1); // ]
			while (ss.peek() != ';' && ss.ignore(1));
			while (ss.peek() == ';') {
				ep.ep_moves.emplace_back();
				ss >> ep.ep_moves.back();
			}
			ep.ep_score = 0;
		} else {
			in.setstate(std::ios::failbit);
		}
		return in;
	}

protected:

	struct move {
		action code;
		board::reward reward;
		time_t time;
		move(action code = {}, board::reward reward = 0, time_t time = 0) : code(code), reward(reward), time(time) {}

		operator action() const { return code; }
		friend std::ostream& operator <<(std::ostream& out, const move& m) {
			out << m.code;
			if (m.time) out << "C[" << std::dec << m.time << "]";
			return out;
		}
		friend std::istream& operator >>(std::istream& in, move& m) {
			in >> m.code;
			m.reward = 0;
			m.time = 0;
			if (in.peek() == 'C') {
				in.ignore(2); // C[
				in >> std::dec >> m.time;
				in.ignore(1); // ]
			}
			return in;
		}
	};

	struct meta {
		std::string tag;
		time_t when;
		meta(const std::string& tag = "N/A", time_t when = 0) : tag(tag), when(when) {}

		friend std::ostream& operator <<(std::ostream& out, const meta& m) {
			return out << m.tag << "@" << std::dec << m.when;
		}
		friend std::istream& operator >>(std::istream& in, meta& m) {
			return std::getline(in, m.tag, '@') >> std::dec >> m.when;
		}
	};

	static board initial_state() {
		return {};
	}
	static time_t millisec() {
		auto now = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	}
	
	/******************* debug tools  **************************/

	static board endGame() {
		/* one step end game*/
		std::vector<int> put_black {2, 5, 8, 11, 12, 14, 15, 17, 24, 26, 27, 28, 30, 31, 34, 35, 40, 41, 44, 49, 50, 52, 53, 54, 56, 57, 63, 69, 75, 76, 79, 80};
		std::vector<int> put_white {0, 1, 4, 6, 7, 9, 10, 18, 20, 21, 25, 29, 32, 33, 36, 39, 46, 48, 55, 59, 60, 61, 62, 64, 65, 66, 68, 71, 72, 73, 77, 78, 100};
		
		/* four step end game*/
		//std::vector<int> put_black {2, 5, 8, 11, 12, 14, 15, 17, 24, 26, 27, 28, 30, 31, 34, 35, 40, 41, 44, 49, 50, 52, 53, 54, 56, 57, 63, 69, 75, 76, 79, 80};
		//std::vector<int> put_white {0, 1, 4, 6, 7, 9, 10, 18, 20, 21, 25, 29, 32, 33, 36, 39, 46, 48, 55, 59, 60, 61, 62, 64, 65, 66, 68, 71, 72, 73, 77, 78};
		
		/* ten step end game */
		//std::vector<int> put_black {2, 5, 8, 11, 12, 14, 15, 17, 24, 26, 27, 28, 30, 31, 34, 35, 40, 41, 44, 49, 50, 52, 53, 54, 56, 57, 63, 69, 75, 76, 79, 80};
		//std::vector<int> put_white {0, 1, 4, 6, 7, 9, 10, 18, 20, 21, 25, 29, 32, 33, 36, 39, 46, 48, 55, 59, 60, 61, 62, 64, 65, 66, 68, 71, 72, 73, 77, 78};

		if (put_black.size() == put_white.size()) std::cout << "legal end game\n";
		
		board endBoard;
		std::vector<int>::iterator it_black, it_white;
		for(it_black = put_black.begin(), it_white = put_white.begin(); (it_black != put_black.end() and it_white != put_white.end()); ++it_black, ++it_white) {
			if (it_black != put_black.end()) {
			int x = *it_black / 9;
			int y = *it_black % 9;
			int temp = endBoard.place(x, y, board::black);
			}
			//if (temp == board::legal) std::cout << "finish put\n";
			if (it_white != put_white.end()) {
			int x = *it_white / 9;
			int y = *it_white % 9;
			int temp = endBoard.place(x, y, board::white);
			}
			//if (temp == board::legal) std::cout << "finish put\n";
		}
		/*
		for(it_white = put_white.begin(); it_white != put_white.end(); ++it_white) {
			int x = *it_white / 9;
			int y = *it_white % 9;
			endBoard.place(x, y, board::white);
		}
		*/
		//std::cout << endBoard << "\n";
		return endBoard;
	}

	/******************* debug tools  *************************/
private:
	board ep_state;
	board::score ep_score;
	std::vector<move> ep_moves;
	time_t ep_time;

	meta ep_open;
	meta ep_close;
};
