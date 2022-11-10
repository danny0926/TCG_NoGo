/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include "board.h"
#include "action.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random player for both side
 * put a legal piece randomly
 */

class Node {
public:
        board state;
        int win_count = 0;
        int visit_count = 0;
        double UCT_value = 0x3f3f3f3f;
        Node* parent = nullptr;
        action::place last_action;
        std::vector<Node*> children;
        board::piece_type who;

	~Node(){};
};

class player : public MCTS_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (meta.find("search") != meta.end()) action_mode = (std::string)meta["search"];
		if (meta.find("timeout") != meta.end()) timeout = (clock_t)meta["timeout"];
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		// default action : random
		if (action_mode == "random" or action_mode.empty()){
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal)
					return move;
			}
			return action();
		}
		

		else if (action_mode == "MCTS"){
			clock_t start_time, end_time, total_time = 0;
			start_time = clock();
			
			Node* root = new Node;
			board::piece_type winner;
			int total_visit_count = 0;
			int empty_space = 0;
			for(int i = 0; i < 9; ++i) {
				for(int j = 0; j < 9; ++j) {
					if(state[i][j] == board::empty)
						++empty_space;
				}
			}
			
			root->state = state;
			root->who = (who == board::white ? board::black : board::white);
			expension(root);
			end_time = clock();
			total_time += (start_time - end_time);
			while(total_time < time_limit) {
				start_time = clock();
				
				Node* best_node = selection(root);
				expension(best_node);
				winner = simulation(best_node);
				
				++total_visit_count;
				backpropagation(root, best_node, winner, total_visit_count);
				
				end_time = clock();
				total_time += (end_time - start_time);
				//std::cout <<"This search cost:" << end_time - start_time << "ms" << std::endl;
			}

			action best_action = bestAaction(root);
			delete_tree(root);
			free(root);
			
			return best_action;
		}
		else if (action_mode == "alpha-beta") {
			throw std::invalid_argument("not be implemented");
		}
		else {
			throw std::invalid_argument("illegal action mode");
		}
		
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
	std::string action_mode;
	clock_t timeout = 1000;
};

class MCTS_agent : public random_agent {
public:
	MCTS_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		white_space(board::size_x * board::size_y), black_space(board::size_x * board::size_y), 
		who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < white_space.size(); ++i)
			white_space[i] = action::place(i, board::white);
		for (size_t i = 0; i < black_space.size(); ++i)
			black_space[i] = action::place(i, board::black);
	}
	
	void computeUCT(Node* node, int total_visit_count) {
		node->UCT_value = ((double)node->win_count/node->visit_count) + 0.5*sqrt(log((double)total_visit_count)/node->visit_count);
	}
	
	void expension(Node* parent_node) {
		board::piece_type child_who;
		action::place child_move;
		
		if (parent_node->who == board::black) {
			child_who = board::white;
			for(const action::place& child_move : white_space) {
				board after = parent_node->state;
				if (child_move.apply(after) == board::legal) {
					Node* child_node = new Node;
					child_node->state = after;
					child_node->parent = parent_node;
					child_node->last_action = child_move;
					child_node->who = child_who;
					
					parent_node->children.emplace_back(child_node);
				}
			}
		}
		else if (parent_node->who == board::white) {
			child_who = board::black;
			for(const action::place& child_move : black_space) {
				board after = parent_node->state;
				if (child_move.apply(after) == board::legal) {
					Node* child_node = new Node;
					child_node->state = after;
					child_node->parent = parent_node;
					child_node->last_action = child_move;
					child_node->who = child_who;
					
					parent_node->children.emplace_back(child_node);
				}
			}
		}
			
	}
	
	Node* selection(Node* node) {
		while(node->children.empty() == false) {
			double max_UCT_value = 0;
			int select_idx = 0;
			for(size_t i = 0; i < node->children.size(); ++i) {
				if(max_UCT_value < node->children[i]->UCT_value) {
					max_UCT_value = node->children[i]->UCT_value;
					select_idx = i;
				}
			}
			node = node->children[select_idx];
		}
		return node;
	}
	
	/* return the winner */
	board::piece_type simulation(Node* root) {
		bool terminal = false;
		board state = root->state;
		board::piece_type who = root->who;
		
		while(terminal == false) {
			/* there are no legal move -> terminal */
			terminal = true;
			
			/*rival's round*/
			who = (who == board::white ? board::black : board::white);
			
			if (who == board::black) {
				/* place randomly , apply the first legal move*/
				std::shuffle(black_space.begin(), black_space.end(), engine);
				for(const action::place& move : black_space) {
					board after = state;
					if (move.apply(after) == board::legal) {
						move.apply(state);
						terminal = false;
						break;
					}
				}
			}
			else if (who == board::white) {
				/* place randomly , apply the first legal move*/
				std::shuffle(white_space.begin(), white_space.end(), engine);
				for(const action::place& move : black_space) {
					board after = state;
					if (move.apply(after) == board::legal) {
						move.apply(state);
						terminal = false;
						break;
					}
				}
			}
		}
		/*I have no legal move, rival win*/
		return (who = (who == board::white ? board::black : board::white));
	}
	
	void backpropagation(Node* root, Node* node, board::piece_type winner, int total_visit_count) {
		/* e.g.
		// root state : last_action = white 
		// -> root who = black 
		*/
		bool win = true;
		if(winner == root->who)
			win = false;
		while(node != nullptr) {
			++node->visit_count;
			if(win == true)
				++node->win_count;
			computeUCT(node, total_visit_count);
			node = node->parent;
		}
	}
	
	action bestAaction(Node* node) {
		int child_idx = -1;
		int max_visit_count = 0;
		for(size_t i = 0; i < node->children.size(); ++i) {
			if(node->children[i]->visit_count > max_visit_count) {
				max_visit_count = node->children[i]->visit_count;
				child_idx = i;
			}
		}
		
		if(child_idx == -1) return action();
		return node->children[child_idx]->last_action;
	}
	
	void delete_tree(Node* node) {
		if(node->children.empty() == false) {
			for(size_t i = 0; i < node->children.size(); ++i) {
				delete_tree(node->children[i]);
				if(node->children[i] != nullptr)
					free(node->children[i]);
			}
			node->children.clear();
		}
	}
	

private:
	std::vector<action::place> white_space, black_space;
	board::piece_type who;
};
