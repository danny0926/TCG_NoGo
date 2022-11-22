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
#include <unistd.h>
#include <omp.h>
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

class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y),
	        white_space(board::size_x * board::size_y),
		black_space(board::size_x * board::size_y),
		who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (meta.find("search") != meta.end()) action_mode = (std::string)meta["search"];
		if (meta.find("timeout") != meta.end()) timeout = (clock_t)meta["timeout"];
		if (meta.find("simulation") != meta.end()) simulation_count = (int)meta["simulation"];
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
		for (size_t i = 0; i < white_space.size(); ++i)
			white_space[i] = action::place(i, board::white);
		for (size_t i = 0; i < black_space.size(); ++i)
			black_space[i] = action::place(i, board::black);
	}
	/******************* begin of MCTS's tools **************************/
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
	board::piece_type simulation(Node* root, bool random_open = false) {
		bool terminal = false;
		board state = root->state;
		board::piece_type who = root->who;
		
		if(random_open == true) {
			std::default_random_engine engine2(time(0) + omp_get_thread_num());
			engine = engine2;
		}

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
				for(const action::place& move : white_space) {
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
		return (who == board::white ? board::black : board::white);
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
	
	void bestAction(Node* node, action *best_action, int *best_idx) {
		int child_idx = -1;
		int max_visit_count = 0;
		
		for(size_t i = 0; i < node->children.size(); ++i) {
			//std::cout << "id " << i << " " << node->children[i]->visit_count << "\n";	
			if(node->children[i]->visit_count > max_visit_count) {
				max_visit_count = node->children[i]->visit_count;
				child_idx = i;
			}
		}
		//std::cout << "\n";
		*best_idx = child_idx;
		if(child_idx == -1) *best_action = action();
		else *best_action = node->children[child_idx]->last_action;
		//if(child_idx == -1) return action();
		//return node->children[child_idx]->last_action;
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
	/* only can use simulation arg  */
	Node* tree_policy(Node* root, bool random_open=true) {
		int cnt = 0;
		board::piece_type winner;
		int total_visit_count = 0;
		
		while (cnt < simulation_count) {
			Node* best_node = selection(root);

			expension(best_node);
			winner = simulation(best_node, random_open);
			++total_visit_count;
			backpropagation(root, best_node, winner, total_visit_count);
			++cnt;
		}
		//#pragma omp critical
		//{
		//	std::cout << "thread " << omp_get_thread_num() << " : cnt = " << cnt << std::endl;
		//}
		return root;
	}
	/******************* end of MCTS's tools **************************/



	/****************** begin of parallel MCTS's tools ********************/
	void parallelMCTS(std::vector<Node*>& roots) {
		omp_set_num_threads(thread_num);
		//std::cout << "There are " << omp_get_num_threads() << " can be used\n";
		
		Node* temp_node = new Node;
		if (simulation_count > 0) {	
			#pragma omp parallel for private(temp_node)
			for (int i = 0; i < thread_num; ++i) {
			//	#pragma omp critical
			//	{
			//		std::cout << "before tree policy, root " << omp_get_thread_num() << std::endl;
			//		
			//		printNode(roots[i]);
			//	}
				temp_node = tree_policy(roots[i]);
				//#pragma omp critical
				//{
					roots[i] = temp_node;
				//}

			//	#pragma omp critical
			//	{
			//		std::cout << "thread " << omp_get_thread_num() << "   done\n";
			//		std::cout << "after tree policy, root " << omp_get_thread_num() << std::endl;

			//		printNode(roots[i]);
			//	}
			}
		}
	
		else {
			//#pragma omp parallel for	
			for (int i = 0; i < thread_num; ++i) {
				clock_t start_time, end_time, total_time = 0;
				start_time = clock();
				int total_visit_count = 0;
				board::piece_type winner;
				while(total_time < timeout) {
					roots[i] = tree_policy(roots[i]);
					end_time = clock();
					total_time = (end_time - start_time);
				}
			}
		}
		
	}
	/****************** end of parallel MCTS's tools ********************/


	


	void printNode(Node* node) {
		std::cout << "##########################\n" << "win_count : " << node->win_count << std::endl << "visit_count : " << node->visit_count << std::endl << "UCT_value : " << node->UCT_value << std::endl << "piece_type : " << node->who << std::endl << "##########################\n";
	}

	virtual action take_action(const board& state) {
		
		//board state_ = endGame();
		//std::cout<<state_;
		//sleep(5);
		

		// default action : random
		if (action_mode == "random" or action_mode.empty()){
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal) {
					//std::cout << state << "\n";
					return move;
				}
			}
			//std::cout << state << "\n";
			return action();
		}
		

		else if (action_mode == "MCTS"){
			clock_t start_time, end_time, total_time = 0;
			start_time = clock();
			
			Node* root = new Node;
			board::piece_type winner;
			int total_visit_count = 0;
			
			root->state = state;
			std::cout << root->state << "\n";
			root->who = (who == board::white ? board::black : board::white);
			expension(root);
			
			
			// default time limit = 1s //
			if (simulation_count == 0) { 
				while(total_time < timeout) {
									
					Node* best_node = selection(root);
					expension(best_node);
					winner = simulation(best_node);
				
					++total_visit_count;
					backpropagation(root, best_node, winner, total_visit_count);
					end_time = clock();

					total_time = (double)(end_time-start_time);
				}	
			}
			else {
				int cnt = 0;
				
				while (cnt < simulation_count) {
					
					 Node* best_node = selection(root);

					expension(best_node);
					winner = simulation(best_node);

					++total_visit_count;
					backpropagation(root, best_node, winner, total_visit_count);
					
					//tree_policy(root, winner, total_visit_count);
					++cnt;
				}
				
			}
			action best_action;
			int best_idx;
			bestAction(root, &best_action, &best_idx);
			//action best_action = bestAction(root);
			//std::cout << "take action : " << best_action << std::endl;
			delete_tree(root);
			free(root);
			return best_action;
		}
		else if (action_mode == "MCTS-parallel") {
			//std::cout << state << std::endl;
			std::vector<Node*> roots(thread_num);

			for (int i = 0; i < thread_num; ++i) {
				roots[i] = new Node;
				roots[i]->state = state;
				roots[i]->who = (who == board::white ? board::black : board::white);
				expension(roots[i]);
			}


			if (simulation_count == 0) {
				
			}
			else {
				//int cnt = 0, total_visit_count = 0;
				//board::piece_type winner;
				//#pragma omp parallel for
				for(int i = 0; i < thread_num; ++i) {
					int cnt=0, total_visit_count=0;
					board::piece_type winner;
					while (cnt < simulation_count) {
						Node* best_node = selection(roots[i]);
						

						//std::cout << "thread " << i << " choose :\n";
						//printNode(best_node);


						expension(best_node);
						winner = simulation(best_node, false);
						++total_visit_count;
						backpropagation(roots[i], best_node, winner, total_visit_count);
						++cnt;

					}
				}
			}

			
			//parallelMCTS(roots);
			//for(int i = 0; i < thread_num; ++i) {
			//	std::cout << "root " << i << std::endl;
			//	printNode(roots[i]);
			//	std::cout << "\n";
			//}

			int bound = roots[0]->children.size();
			

			//for(int i = 0; i < thread_num; ++i) {
			//	std::cout << "thread " << i << ": \n";
			//	std::cout << roots[i]->visit_count;
			//	std::cout << "\n";
			//}
	
			//std::cout << "root0...\n";
			//for(int i = 0 ; i < bound; ++i) {
				//std::cout << "id " << i <<"   " << roots[0]->children[i]->visit_count << "\n";
				//total += roots[0]->children[i]->visit_count;
			//}
			//std::cout << "thread 0 : total visit in children : " << total << "\n";

			// aggregate count result
			for (int thread_idx = 1; thread_idx < thread_num; ++thread_idx) {
				Node* root_temp = roots[thread_idx];
				//total = 0;
				//std::cout << "root" << thread_idx << "...\n";
				//std::vector<Node*>::iterator it;
				for(int i = 0; i < bound ;++i) {
					//std::cout << "id " << i <<"   " << roots[thread_idx]->children[i]->visit_count << "\n";
					//roots[0]->children[i]->visit_count += roots[thread_idx]->children[i]->visit_count;
					//total += roots[thread_idx]->children[i]->visit_count;
				}
				//std::cout << "thread " << thread_idx << " : total visit in children : " << total << "\n";
			}

			//std::cout << "root0"  << "...\n";
			//for(int i = 0; i < bound; ++i) {
				//std::cout << "id " << i <<"   " << roots[0]->children[i]->visit_count << "\n";std::cout << "id " << i <<"   " << roots[0]->children[i]->visit_count << "\n";
			//}

			int best_index;
			action best_action;
			bestAction(roots[1], &best_action, &best_index);
			std::cout << "best action : " << best_action << "\n";
			//sleep(1);
			for(int i = 0; i < thread_num; ++i) {
				delete_tree(roots[i]);
				free(roots[i]);
			}
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
	std::vector<action::place> space, white_space, black_space;
	board::piece_type who;
	std::string action_mode;
	int simulation_count = 0;
	clock_t timeout = 1000;
	int thread_num = 2;   /* default thread number = 4  */
};
