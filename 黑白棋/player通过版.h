#include <string.h>
#include "../include/playerbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

// 动态生成评估矩阵
int** generate_weights(int row_cnt, int col_cnt) {
    int** weights = (int**)malloc(row_cnt * sizeof(int*));
    for (int i = 0; i < row_cnt; i++) {
        weights[i] = (int*)malloc(col_cnt * sizeof(int));
    }

    for (int i = 0; i < row_cnt; i++) {
        for (int j = 0; j < col_cnt; j++) {
            if ((i == 0 || i == row_cnt - 1) && (j == 0 || j == col_cnt - 1)) {
                // 角上位置
                weights[i][j] = 100;
            } else if ((i == 0 || i == 1 || i == row_cnt - 2 || i == row_cnt - 1) && (j == 0 || j == 1 || j == col_cnt - 2 || j == col_cnt - 2)) {
                // 靠近角的内部位置
                weights[i][j] = -50;
            } else if (i == 0 || i == row_cnt - 1 || j == 0 || j == col_cnt - 1) {
                // 边上位置
            weights[i][j] = 20;
            } else if (i == 1 || i == row_cnt - 2 || j == 1 || j == col_cnt - 2) {
                // 靠近边的内部位置
                weights[i][j] = -20;
            } else if (i == j || i + j == row_cnt || i + j == col_cnt)
            {
                weights[i][j] = 10;
            }
            else {
                // 内部位置
                weights[i][j] = 5;
            }
        }
    }
    return weights;
}

// 释放评估矩阵内存
void free_weights(int** weights, int row_cnt) {
    for (int i = 0; i < row_cnt; i++) {
        free(weights[i]);
    }
    free(weights);
}

void init(struct Player *player) {
    // This function will be executed at the begin of each game, only once.
    srand(time(0));
}

int is_valid(struct Player *player, int posx, int posy,char piece) {
    char opponent = (piece == 'O') ? 'o' : 'O';
    if (posx < 0 || posx >= player->row_cnt || posy < 0 || posy >= player->col_cnt) {
        return false;
    }
    if (player->mat[posx][posy] == 'o' || player->mat[posx][posy] == 'O') {
        return false;
    }
    int step[8][2] = {0, 1, 0, -1, 1, 0, -1, 0, 1, 1, -1, -1, 1, -1, -1, 1};
    for (int dir = 0;  dir < 8; dir++) {
        int x = posx + step[dir][0];
        int y = posy + step[dir][1];
        if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt) {
            continue;
        }
        if (player->mat[x][y] != opponent) {
            continue;
        }
        while (true) {
            x += step[dir][0];
            y += step[dir][1];
            if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt || (player->mat[x][y] >= '1' && player->mat[x][y] <= '9')) {
                break;
            }
            if (player->mat[x][y] == piece) {
                return true;
            }
        }
    }
    return false;
}

// 复制棋盘状态
void copy_board(struct Player *src, struct Player *dst) {
    memcpy(dst, src, sizeof(struct Player));
}

// 模拟落子
void make_move(struct Player *player, int posx, int posy, char piece) {
    player->mat[posx][posy] = piece;
    char opponent = (piece == 'O') ? 'o' : 'O';
    int step[8][2] = {0, 1, 0, -1, 1, 0, -1, 0, 1, 1, -1, -1, 1, -1, -1, 1};
    for (int dir = 0;  dir < 8; dir++) {
        int x = posx + step[dir][0];
        int y = posy + step[dir][1];
        if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt) {
            continue;
        }
        if (player->mat[x][y] != opponent) {
            continue;
        }
        int flip = 0;
        while (true) {
            x += step[dir][0];
            y += step[dir][1];
            if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt || (player->mat[x][y] >= '1' && player->mat[x][y] <= '9')) {
                flip = 0;
                break;
            }
            if (player->mat[x][y] == piece) {
                break;
            }
            flip++;
        }
        x = posx + step[dir][0];
        y = posy + step[dir][1];
        for (int i = 0; i < flip; i++) {
            player->mat[x][y] = piece;
            x += step[dir][0];
            y += step[dir][1];
        }
    }
}

// 评估函数
int evaluate(struct Player *player) {
    int** weights = generate_weights(player->row_cnt, player->col_cnt);
    int score = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (player->mat[i][j] == 'O') {
                score += weights[i][j];
            } else if (player->mat[i][j] == 'o') {
                score -= weights[i][j];
            }
        }
    }
    free_weights(weights, player->row_cnt);
    return score;
}

// 极小极大算法带 α-β 剪枝
int minimax(struct Player *player, int depth, int alpha, int beta, bool is_maximizing) {
    if (depth == 0) {
        return evaluate(player);
    }
    
    char piece = is_maximizing ? 'O' : 'o';
    struct Point *ok_points = (struct Point *)malloc((player->row_cnt * player->col_cnt) * sizeof(struct Point));
    int ok_cnt = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (is_valid(player, i, j, piece)) {
                ok_points[ok_cnt++] = initPoint(i, j);
            }
        }
    }

    if (ok_cnt == 0) {
        free(ok_points);
        return evaluate(player);
    }

    if (is_maximizing) {
        int max_eval = -999999;
        for (int i = 0; i < ok_cnt; i++) {
            struct Player temp_player;
            copy_board(player, &temp_player);
            make_move(&temp_player, ok_points[i].X, ok_points[i].Y, 'O');
            int eval = minimax(&temp_player, depth - 1, alpha, beta, false);
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (eval > alpha) ? eval : alpha;
            if (beta <= alpha) {
                break;
            }
        }
        free(ok_points);
        return max_eval;
    } else {
        int min_eval = 999999;
        for (int i = 0; i < ok_cnt; i++) {
            struct Player temp_player;
            copy_board(player, &temp_player);
            make_move(&temp_player, ok_points[i].X, ok_points[i].Y, 'o');
            int eval = minimax(&temp_player, depth - 1, alpha, beta, true);
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (eval < beta) ? eval : beta;
            if (beta <= alpha) {
                break;
            }
        }
        free(ok_points);
        return min_eval;
    }
}

struct Point place(struct Player *player) {
    struct Point *ok_points = (struct Point *)malloc((player->row_cnt * player->col_cnt) * sizeof(struct Point));
    int ok_cnt = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (is_valid(player, i, j, 'O')) {
                ok_points[ok_cnt++] = initPoint(i, j);
            }
        }
    }

    struct Point best_point = initPoint(-1, -1);
    int best_score = -999999;
    int depth = 4; // 搜索深度

    for (int i = 0; i < ok_cnt; i++) {
        if((ok_points[i].X == 0 || ok_points[i].X == player->row_cnt - 1) && (ok_points[i].Y == player->col_cnt - 1 || ok_points[i].Y == 0))
        {
            return ok_points[i];
        }
        struct Player temp_player;
        copy_board(player, &temp_player);
        make_move(&temp_player, ok_points[i].X, ok_points[i].Y, 'O');
        int score = minimax(&temp_player, depth - 1, -999999, 999999, false);
        if (score > best_score) {
            best_score = score;
            best_point = ok_points[i];
        }
    }

    free(ok_points);
    return best_point;
}
