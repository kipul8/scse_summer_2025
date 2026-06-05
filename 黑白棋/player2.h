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

// 棋盘评估权重矩阵
const int WEIGHTS[8][8] = {
    {100, -20, 10, 5, 5, 10, -20, 100},
    {-20, -50, -2, -2, -2, -2, -50, -20},
    {10, -2, -1, -1, -1, -1, -2, 10},
    {5, -2, -1, -1, -1, -1, -2, 5},
    {5, -2, -1, -1, -1, -1, -2, 5},
    {10, -2, -1, -1, -1, -1, -2, 10},
    {-20, -50, -2, -2, -2, -2, -50, -20},
    {100, -20, 10, 5, 5, 10, -20, 100}
};

void init(struct Player *player) {
    // This function will be executed at the begin of each game, only once.
    srand(time(0));
}

int is_valid(struct Player *player, int posx, int posy) {
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
        if (player->mat[x][y] != 'o') {
            continue;
        }
        while (true) {
            x += step[dir][0];
            y += step[dir][1];
            if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt || (player->mat[x][y] >= '1' && player->mat[x][y] <= '9')) {
                break;
            }
            if (player->mat[x][y] == 'O') {
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
void make_move(struct Player *player, int posx, int posy) {
    player->mat[posx][posy] = 'O';
    int step[8][2] = {0, 1, 0, -1, 1, 0, -1, 0, 1, 1, -1, -1, 1, -1, -1, 1};
    for (int dir = 0;  dir < 8; dir++) {
        int x = posx + step[dir][0];
        int y = posy + step[dir][1];
        if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt) {
            continue;
        }
        if (player->mat[x][y] != 'o') {
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
            if (player->mat[x][y] == 'O') {
                break;
            }
            flip++;
        }
        x = posx + step[dir][0];
        y = posy + step[dir][1];
        for (int i = 0; i < flip; i++) {
            player->mat[x][y] = 'O';
            x += step[dir][0];
            y += step[dir][1];
        }
    }
}

// 评估函数
int evaluate(struct Player *player) {
    int score = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (player->mat[i][j] == 'O') {
                score += WEIGHTS[i][j];
            } else if (player->mat[i][j] == 'o') {
                score -= WEIGHTS[i][j];
            }
        }
    }
    return score;
}

// 极小极大算法带 α-β 剪枝
int minimax(struct Player *player, int depth, int alpha, int beta, bool is_maximizing) {
    if (depth == 0) {
        return evaluate(player);
    }

    struct Point *ok_points = (struct Point *)malloc((player->row_cnt * player->col_cnt) * sizeof(struct Point));
    int ok_cnt = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (is_valid(player, i, j)) {
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
            make_move(&temp_player, ok_points[i].X, ok_points[i].Y);
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
            make_move(&temp_player, ok_points[i].X, ok_points[i].Y);
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
            if (is_valid(player, i, j)) {
                ok_points[ok_cnt++] = initPoint(i, j);
            }
        }
    }

    struct Point best_point = initPoint(-1, -1);
    int best_score = -999999;
    int depth = 4; // 搜索深度

    for (int i = 0; i < ok_cnt; i++) {
        struct Player temp_player;
        copy_board(player, &temp_player);
        make_move(&temp_player, ok_points[i].X, ok_points[i].Y);
        int score = minimax(&temp_player, depth - 1, -999999, 999999, false);
        if (score > best_score) {
            best_score = score;
            best_point = ok_points[i];
        }
    }

    free(ok_points);
    return best_point;
}
