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
#include <stdbool.h>
#include <time.h>

void init(struct Player *player) {
    srand(time(0));
}

int in_corner(int i, int j, Player *player)
{
    return (i == 0 || i == player->row_cnt - 1) && (j == 0 || j == player->col_cnt - 1);
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
        int index = 0;
        while (true) {
            x += step[dir][0];
            y += step[dir][1];
            if (x < 0 || x >= player->row_cnt || y < 0 || y >= player->col_cnt || (player->mat[x][y] >= '1' && player->mat[x][y] <= '9')) {
                break;
            }
            if (player->mat[x][y] == 'O') {
                return index;
            }
            index++;
        }
    }
    return false;
}


struct Point place(struct Player *player) {
    struct Point good_place = initPoint(-1, -1);
    int grade = 0, temp;
    bool corner_found = false;
    struct Point corner_place = initPoint(-1, -1);

    for(int i = 0; i < player->row_cnt; i++) {
        for(int j = 0; j < player->col_cnt; j++) {
            temp = is_valid(player, i, j);
            if(temp > 0) { 
                if(temp > grade) {
                    good_place.X = i;
                    good_place.Y = j;
                    grade = temp;
                }
                if(in_corner(i, j, player)) {
                    corner_found = true;
                    corner_place.X = i;
                    corner_place.Y = j;
                }
            }
        }
    }
    if(corner_found) {
        return corner_place;
    }
    return good_place;
}
