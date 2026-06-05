/**
 * @file player.h
 * @author yangboyang@jisuanke.com
 * @copyright jisuanke.com
 * @date 2021-07-01
 */

 #include <string.h>
 #include "../include/playerbase.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <time.h>
 #include <stdbool.h>
 #include <limits.h>
 
 // 根据棋盘大小动态调整搜索深度
 int get_search_depth(int rows, int cols) {
	 int size = rows * cols;
	 if (size <= 64) {      // 8x8
		 return 5;
	 } else if (size <= 100) { // 10x10
		 return 3;
	 } else {               // 12x12
		 return 3;
	 }
 }
 
 // 针对不同棋盘尺寸的位置权重表
 static const int POSITION_WEIGHTS_8[8][8] = {
	 {100, -15, 10, 5, 5, 10, -15, 100},
	 {-15, -30, -5, -5, -5, -5, -30, -15},
	 {10, -5, 3, 2, 2, 3, -5, 10},
	 {50, -5, 2, 1, 1, 2, -5, 5},
	 {50, -5, 2, 1, 1, 2, -5, 5},
	 {10, -5, 3, 2, 2, 3, -5, 10},
	 {-15, -30, -5, -5, -5, -5, -30, -15},
	 {100, -15, 10, 5, 5, 10, -15, 100}
 };
 
 static const int POSITION_WEIGHTS_10[10][10] = {
	 {100, -15, 10, 5, 5, 5, 5, 10, -15, 100},
	 {-15, -30, -5, -5, -5, -5, -5, -5, -30, -15},
	 {10, -5, 3, 2, 2, 2, 2, 3, -5, 10},
	 {5, -5, 2, 2, 1, 1, 2, 2, -5, 5},
	 {5, -5, 2, 1, 1, 1, 1, 2, -5, 5},
	 {5, -5, 2, 1, 1, 1, 1, 2, -5, 5},
	 {5, -5, 2, 2, 1, 1, 2, 2, -5, 5},
	 {10, -5, 3, 2, 2, 2, 2, 3, -5, 10},
	 {-15, -30, -5, -5, -5, -5, -5, -5, -30, -15},
	 {100, -15, 10, 5, 5, 5, 5, 10, -15, 100}
 };
 
 static const int POSITION_WEIGHTS_12[12][12] = {
	 {100, -15, 10, 5, 5, 5, 5, 5, 5, 10, -15, 100},
	 {-15, -30, -5, -5, -5, -5, -5, -5, -5, -5, -30, -15},
	 {10, -5, 4, 2, 2, 2, 2, 2, 2, 4, -5, 10},
	 {5, -5, 2, 3, 1, 1, 1, 1, 3, 2, -5, 5},
	 {5, -5, 2, 1, 2, 1, 1, 2, 1, 2, -5, 5},
	 {5, -5, 2, 1, 1, 1, 1, 1, 1, 2, -5, 5},
	 {5, -5, 2, 1, 1, 1, 1, 1, 1, 2, -5, 5},
	 {5, -5, 2, 1, 2, 1, 1, 2, 1, 2, -5, 5},
	 {5, -5, 2, 3, 1, 1, 1, 1, 3, 2, -5, 5},
	 {10, -5, 4, 2, 2, 2, 2, 2, 2, 4, -5, 10},
	 {-15, -30, -5, -5, -5, -5, -5, -5, -5, -5, -30, -15},
	 {100, -15, 10, 5, 5, 5, 5, 5, 5, 10, -15, 100}
 };
 
 // 方向数组
 static const int DIRECTIONS[8][2] = {
	 {0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}
 };
 
 // 获取位置权重
 int get_position_weight(int x, int y, int rows, int cols) {
	 if (rows == 8 && cols == 8) {
		 return POSITION_WEIGHTS_8[x][y];
	 } else if (rows == 10 && cols == 10) {
		 return POSITION_WEIGHTS_10[x][y];
	 } else if (rows == 12 && cols == 12) {
		 return POSITION_WEIGHTS_12[x][y];
	 } else {
		 // 对于其他尺寸，使用12x12的权重表进行缩放
		 int scale_x = (x * 12) / rows;
		 int scale_y = (y * 12) / cols;
		 return POSITION_WEIGHTS_12[scale_x][scale_y];
	 }
 }
 
 // 使用computer.h的移动验证机制
 bool is_valid_move(char **board, int posx, int posy, int rows, int cols, char player_piece) {
	 // 基本边界检查
	 if (posx < 0 || posx >= rows || posy < 0 || posy >= cols) {
		 return false;
	 }
	 
	 // 检查位置是否已被占用
	 if (board[posx][posy] == 'o' || board[posx][posy] == 'O') {
		 return false;
	 }
	 
	 char opponent_piece = (player_piece == 'O') ? 'o' : 'O';
	 
	 // 检查八个方向
	 for (int dir = 0; dir < 8; dir++) {
		 int x = posx + DIRECTIONS[dir][0];
		 int y = posy + DIRECTIONS[dir][1];
		 
		 // 检查相邻位置是否在边界内且为对手棋子
		 if (x < 0 || x >= rows || y < 0 || y >= cols) {
			 continue;
		 }
		 if (board[x][y] != opponent_piece) {
			 continue;
		 }
		 
		 // 沿该方向继续搜索
		 while (true) {
			 x += DIRECTIONS[dir][0];
			 y += DIRECTIONS[dir][1];
			 
			 // 检查是否超出边界或遇到空位
			 if (x < 0 || x >= rows || y < 0 || y >= cols || 
				 (board[x][y] >= '1' && board[x][y] <= '9')) {
				 break;
			 }
			 
			 // 如果遇到自己的棋子，说明这是一个有效移动
			 if (board[x][y] == player_piece) {
				 return true;
			 }
		 }
	 }
	 return false;
 }
 
 // 获取所有合法移动
 int get_valid_moves(char **board, int rows, int cols, char player_piece, struct Point *moves) {
	 int count = 0;
	 for (int i = 0; i < rows; i++) {
		 for (int j = 0; j < cols; j++) {
			 if (is_valid_move(board, i, j, rows, cols, player_piece)) {
				 moves[count++] = initPoint(i, j);
			 }
		 }
	 }
	 return count;
 }
 
 // 执行移动并翻转棋子
 void make_move(char **board, int x, int y, int rows, int cols, char player_piece) {
	 if (!is_valid_move(board, x, y, rows, cols, player_piece)) {
		 return;
	 }
	 
	 char opponent_piece = (player_piece == 'O') ? 'o' : 'O';
	 board[x][y] = player_piece;
	 
	 // 在八个方向上翻转棋子
	 for (int dir = 0; dir < 8; dir++) {
		 int nx = x + DIRECTIONS[dir][0];
		 int ny = y + DIRECTIONS[dir][1];
		 
		 if (nx < 0 || nx >= rows || ny < 0 || ny >= cols || board[nx][ny] != opponent_piece) {
			 continue;
		 }
		 
		 // 检查是否可以翻转
		 bool can_flip = false;
		 int temp_x = nx;
		 int temp_y = ny;
		 
		 while (true) {
			 temp_x += DIRECTIONS[dir][0];
			 temp_y += DIRECTIONS[dir][1];
			 
			 if (temp_x < 0 || temp_x >= rows || temp_y < 0 || temp_y >= cols || 
				 (board[temp_x][temp_y] >= '1' && board[temp_x][temp_y] <= '9')) {
				 break;
			 }
			 
			 if (board[temp_x][temp_y] == player_piece) {
				 can_flip = true;
				 break;
			 }
		 }
		 
		 // 执行翻转
		 if (can_flip) {
			 temp_x = nx;
			 temp_y = ny;
			 while (board[temp_x][temp_y] == opponent_piece) {
				 board[temp_x][temp_y] = player_piece;
				 temp_x += DIRECTIONS[dir][0];
				 temp_y += DIRECTIONS[dir][1];
			 }
		 }
	 }
 }
 
 // 根据棋盘大小调整评估函数
 int evaluate_board(char **board, int rows, int cols, char player_piece) {
	 int score = 0;
	 char opponent_piece = (player_piece == 'O') ? 'o' : 'O';

	 // 1. 终局直接返回棋子数差值
	 int empty = 0, player_count = 0, opponent_count = 0;
	 for (int i = 0; i < rows; i++)
		 for (int j = 0; j < cols; j++) {
			 if (board[i][j] == player_piece) player_count++;
			 else if (board[i][j] == opponent_piece) opponent_count++;
			 else if (board[i][j] != 'O' && board[i][j] != 'o') empty++;
		 }
     if (empty == 0) return (player_count - opponent_count) * 10000;
     else if (empty > 0 && empty <= rows)
         score += (player_count - opponent_count) * 30;
     else if (empty > rows && empty <= (rows + 1) * (cols - 1))
         score += (player_count - opponent_count) * 10;
     else score += 0;

	 // 2. 位置权重
	 for (int i = 0; i < rows; i++) {
		 for (int j = 0; j < cols; j++) {
			 if (board[i][j] == player_piece) {
				 score += get_position_weight(i, j, rows, cols);
			 } else if (board[i][j] == opponent_piece) {
				 score -= get_position_weight(i, j, rows, cols);
			 }
		 }
	 }

	 // 3. 角落和危险点权重
	 const int CORNER_BONUS = 400;
	 const int DANGER_PENALTY = -80;
	 const int EDGE_BONUS = 80;
	 int corners[4][2] = {{0,0},{0,cols-1},{rows-1,0},{rows-1,cols-1}};
	 int danger[12][2] = {
		 {0,1},{1,0},{1,1},
		 {0,cols-2},{1,cols-1},{1,cols-2},
		 {rows-2,0},{rows-1,1},{rows-2,1},
		 {rows-2,cols-1},{rows-1,cols-2},{rows-2,cols-2}
	 };
	 // 角落加分
	 for (int i = 0; i < 4; i++) {
		 int x = corners[i][0], y = corners[i][1];
		 if (board[x][y] == player_piece) score += CORNER_BONUS;
		 if (board[x][y] == opponent_piece) score -= CORNER_BONUS;
	 }
	 // 危险点减分
	 for (int i = 0; i < 12; i++) {
		 int x = danger[i][0], y = danger[i][1];
		 if (x < 0 || x >= rows || y < 0 || y >= cols) continue;
		 if (board[x][y] == player_piece) score += DANGER_PENALTY;
		 if (board[x][y] == opponent_piece) score -= DANGER_PENALTY;
	 }
	 // 边缘加分
	 for (int i = 0; i < rows; i++) {
		 if (board[i][0] == player_piece) score += EDGE_BONUS;
		 if (board[i][cols-1] == player_piece) score += EDGE_BONUS;
		 if (board[i][0] == opponent_piece) score -= EDGE_BONUS;
		 if (board[i][cols-1] == opponent_piece) score -= EDGE_BONUS;
	 }
	 for (int j = 0; j < cols; j++) {
		 if (board[0][j] == player_piece) score += EDGE_BONUS;
		 if (board[rows-1][j] == player_piece) score += EDGE_BONUS;
		 if (board[0][j] == opponent_piece) score -= EDGE_BONUS;
		 if (board[rows-1][j] == opponent_piece) score -= EDGE_BONUS;
	 }

	 // 4. 行动力评估 - 动态权重
	 struct Point moves[144];
	 int player_moves = get_valid_moves(board, rows, cols, player_piece, moves);
	 int opponent_moves = get_valid_moves(board, rows, cols, opponent_piece, moves);
	 int mobility_weight = (rows * cols - empty) / 10 + 4;
	 if (rows * cols <= 64) {      // 8x8
		 mobility_weight = (64 - empty) / 8 + 8;
	 } else if (rows * cols <= 100) { // 10x10
		 mobility_weight = (100 - empty) / 10 + 6;
	 } else {                      // 12x12
		 mobility_weight = (144 - empty) / 12 + 4;
	 }
	 score += (player_moves - opponent_moves) * mobility_weight;

	 return score;
 }
 
 // 创建临时棋盘
 char** create_temp_board(char **board, int rows, int cols) {
	 if (!board || rows <= 0 || cols <= 0) {
		 return NULL;
	 }
	 
	 char **temp_board = (char**)malloc(rows * sizeof(char*));
	 if (!temp_board) return NULL;
	 
	 for (int i = 0; i < rows; i++) {
		 temp_board[i] = (char*)malloc(cols * sizeof(char));
		 if (!temp_board[i]) {
			 for (int j = 0; j < i; j++) {
				 free(temp_board[j]);
			 }
			 free(temp_board);
			 return NULL;
		 }
		 memcpy(temp_board[i], board[i], cols * sizeof(char));
	 }
	 return temp_board;
 }
 
 // 释放临时棋盘
 void free_temp_board(char **board, int rows) {
	 if (!board) return;
	 for (int i = 0; i < rows; i++) {
		 if (board[i]) free(board[i]);
	 }
	 free(board);
 }
 
 // Minimax搜索（带Alpha-Beta剪枝）
 int minimax(char **board, int depth, int alpha, int beta, bool is_maximizing, 
			 int rows, int cols, char player_piece, char current_piece) {
	 if (depth == 0) {
		 return evaluate_board(board, rows, cols, player_piece);
	 }
	 
	 struct Point moves[144];
	 int move_count = get_valid_moves(board, rows, cols, current_piece, moves);
	 
	 // 如果没有合法移动，检查是否需要让对手走子
	 if (move_count == 0) {
		 char opponent_piece = (current_piece == 'O') ? 'o' : 'O';
		 int opponent_moves = get_valid_moves(board, rows, cols, opponent_piece, moves);
		 if (opponent_moves == 0) {
			 return evaluate_board(board, rows, cols, player_piece);
		 }
		 return minimax(board, depth - 1, alpha, beta, !is_maximizing, 
					   rows, cols, player_piece, opponent_piece);
	 }
	 
	 char **temp_board = create_temp_board(board, rows, cols);
	 if (!temp_board) {
		 return evaluate_board(board, rows, cols, player_piece);
	 }
	 
	 if (is_maximizing) {
		 int max_eval = INT_MIN;
		 for (int i = 0; i < move_count && alpha < beta; i++) {
			 // 重置临时棋盘
			 for (int r = 0; r < rows; r++) {
				 memcpy(temp_board[r], board[r], cols * sizeof(char));
			 }
			 
			 make_move(temp_board, moves[i].X, moves[i].Y, rows, cols, current_piece);
			 int eval = minimax(temp_board, depth - 1, alpha, beta, false,
							  rows, cols, player_piece, 
							  (current_piece == 'O') ? 'o' : 'O');
			 
			 max_eval = (eval > max_eval) ? eval : max_eval;
			 alpha = (eval > alpha) ? eval : alpha;
		 }
		 
		 free_temp_board(temp_board, rows);
		 return max_eval;
	 } else {
		 int min_eval = INT_MAX;
		 for (int i = 0; i < move_count && alpha < beta; i++) {
			 // 重置临时棋盘
			 for (int r = 0; r < rows; r++) {
				 memcpy(temp_board[r], board[r], cols * sizeof(char));
			 }
			 
			 make_move(temp_board, moves[i].X, moves[i].Y, rows, cols, current_piece);
			 int eval = minimax(temp_board, depth - 1, alpha, beta, true,
							  rows, cols, player_piece, 
							  (current_piece == 'O') ? 'o' : 'O');
			 
			 min_eval = (eval < min_eval) ? eval : min_eval;
			 beta = (eval < beta) ? eval : beta;
		 }
		 
		 free_temp_board(temp_board, rows);
		 return min_eval;
	 }
 }
 
 // 主要的落子函数
 struct Point place(struct Player *player) {
	 // 参数验证
	 if (!player || !player->mat || player->row_cnt <= 0 || player->col_cnt <= 0 ||
		 player->row_cnt > 12 || player->col_cnt > 12) {
		 return initPoint(-1, -1);
	 }
	 
	 // 获取所有合法移动
	 struct Point moves[144];
	 int move_count = get_valid_moves(player->mat, player->row_cnt, player->col_cnt, 'O', moves);
	 
	 // 如果没有合法移动，返回(-1, -1)表示弃权
	 if (move_count == 0) {
		 // 双重检查：确保对手也没有合法移动才真正弃权
		 int opponent_moves = get_valid_moves(player->mat, player->row_cnt, player->col_cnt, 'o', moves);
		 if (opponent_moves == 0) {
			 return initPoint(-1, -1);
		 }
		 // 如果对手有合法移动，我们必须弃权
		 return initPoint(-1, -1);
	 }
	 
	 // 根据棋盘大小确定搜索深度
	 int search_depth = get_search_depth(player->row_cnt, player->col_cnt);
	 
	 // 创建临时棋盘
	 char **temp_board = create_temp_board(player->mat, player->row_cnt, player->col_cnt);
	 if (!temp_board) {
		 // 内存分配失败时，返回第一个合法移动
		 return moves[0];
	 }
	 
	 // 评估所有移动并选择最佳移动
	 int best_score = INT_MIN;
	 struct Point best_move = moves[0];
	 
	 for (int i = 0; i < move_count; i++) {
		 // 重置临时棋盘
		 for (int r = 0; r < player->row_cnt; r++) {
			 memcpy(temp_board[r], player->mat[r], player->col_cnt * sizeof(char));
		 }
		 
		 // 验证移动的合法性
		 if (!is_valid_move(temp_board, moves[i].X, moves[i].Y, 
						   player->row_cnt, player->col_cnt, 'O')) {
			 continue;
		 }
		 
		 make_move(temp_board, moves[i].X, moves[i].Y, 
				  player->row_cnt, player->col_cnt, 'O');
		 
		 int score = minimax(temp_board, search_depth - 1, INT_MIN, INT_MAX, false,
						   player->row_cnt, player->col_cnt, 'O', 'o');
		 
		 if (score > best_score) {
			 best_score = score;
			 best_move = moves[i];
		 }
	 }
	 
	 free_temp_board(temp_board, player->row_cnt);
	 
	 // 最后一次验证最佳移动的合法性
	 if (!is_valid_move(player->mat, best_move.X, best_move.Y, 
					   player->row_cnt, player->col_cnt, 'O')) {
		 // 如果最佳移动不合法（这不应该发生），返回第一个合法移动
		 return moves[0];
	 }
	 
	 return best_move;
 }
 
 void init(struct Player *player) {
	 srand(time(0));
 }
 
