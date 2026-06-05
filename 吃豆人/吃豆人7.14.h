#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include "../include/playerbase.h"

#define MAP_MAX_ROWS 100
#define MAP_MAX_COLS 100
#define GHOST_DANGER_DISTANCE 3  // 降低幽灵危险范围，减少过度规避
#define OPPONENT_DANGER_DISTANCE 2  // 降低对手危险范围
#define SUPER_STAR_VALUE 10
#define NORMAL_STAR_VALUE 3
#define EMPTY_CELL_VALUE -1
#define WALL_VALUE INT_MIN
#define MAX_RANDOM_TRIES 10  // 增加随机移动尝试次数

typedef struct Node {
    int x, y;
    int g_cost;
    int h_cost;
    int f_cost;
    struct Node* parent;
} Node;

typedef enum {
    EMPTY, WALL, NORMAL_STAR, SUPER_STAR, GHOST, OPPONENT
} CellType;

Node open_list[MAP_MAX_ROWS * MAP_MAX_COLS];
Node closed_list[MAP_MAX_ROWS * MAP_MAX_COLS];
int open_count = 0;
int closed_count = 0;
CellType cell_type_map[MAP_MAX_ROWS][MAP_MAX_COLS];
int value_map[MAP_MAX_ROWS][MAP_MAX_COLS];

// 初始化地图信息：降低危险区域权重，避免过度限制移动
void initMapInfo(struct Player *player) {
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (player->mat[i][j] == '#') {
                cell_type_map[i][j] = WALL;
                value_map[i][j] = WALL_VALUE;
            } else if (player->mat[i][j] == 'o') {
                cell_type_map[i][j] = NORMAL_STAR;
                value_map[i][j] = NORMAL_STAR_VALUE;
            } else if (player->mat[i][j] == 'O') {
                cell_type_map[i][j] = SUPER_STAR;
                value_map[i][j] = SUPER_STAR_VALUE;
            } else {
                cell_type_map[i][j] = EMPTY;
                value_map[i][j] = EMPTY_CELL_VALUE;
            }
        }
    }

    // 幽灵危险区域：降低权重，避免过度规避
    for (int i = 0; i < 2; i++) {
        int x = player->ghost_posx[i];
        int y = player->ghost_posy[i];
        if (x >= 0 && x < player->row_cnt && y >= 0 && y < player->col_cnt) {
            cell_type_map[x][y] = GHOST;
            for (int dx = -GHOST_DANGER_DISTANCE; dx <= GHOST_DANGER_DISTANCE; dx++) {
                for (int dy = -GHOST_DANGER_DISTANCE; dy <= GHOST_DANGER_DISTANCE; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt) {
                        int distance = abs(dx) + abs(dy);
                        if (distance <= GHOST_DANGER_DISTANCE) {
                            value_map[nx][ny] -= (GHOST_DANGER_DISTANCE - distance + 1);  // 降低危险惩罚
                        }
                    }
                }
            }
        }
    }

    // 对手危险区域：降低权重
    int opp_x = player->opponent_posx;
    int opp_y = player->opponent_posy;
    if (opp_x >= 0 && opp_x < player->row_cnt && opp_y >= 0 && opp_y < player->col_cnt) {
        cell_type_map[opp_x][opp_y] = OPPONENT;
        for (int dx = -OPPONENT_DANGER_DISTANCE; dx <= OPPONENT_DANGER_DISTANCE; dx++) {
            for (int dy = -OPPONENT_DANGER_DISTANCE; dy <= OPPONENT_DANGER_DISTANCE; dy++) {
                int nx = opp_x + dx;
                int ny = opp_y + dy;
                if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt) {
                    int distance = abs(dx) + abs(dy);
                    if (distance <= OPPONENT_DANGER_DISTANCE) {
                        if (player->opponent_status > 0) {
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1) * 2;
                        } else {
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1);  // 降低惩罚
                        }
                    }
                }
            }
        }
    }

    // 强化状态下：提升所有可移动区域的价值，避免无目标
    if (player->your_status > 0) {
        for (int i = 0; i < player->row_cnt; i++) {
            for (int j = 0; j < player->col_cnt; j++) {
                if (cell_type_map[i][j] != WALL) {
                    value_map[i][j] += 3;  // 确保有足够价值的移动目标
                }
            }
        }
    }
}

// 重置A*寻路
void resetAStar() {
    open_count = 0;
    closed_count = 0;
}

// 曼哈顿距离
int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 判断是否为墙
bool isWall(struct Player *player, int x, int y) {
    return player->mat[x][y] == '#';
}

// 判断节点是否在关闭列表
bool isInClosedList(int x, int y) {
    for (int i = 0; i < closed_count; i++) {
        if (closed_list[i].x == x && closed_list[i].y == y) {
            return true;
        }
    }
    return false;
}

// 判断节点是否在开放列表
int isInOpenList(int x, int y) {
    for (int i = 0; i < open_count; i++) {
        if (open_list[i].x == x && open_list[i].y == y) {
            return i;
        }
    }
    return -1;
}

// 添加节点到开放列表并排序
void addToOpenList(Node node) {
    open_list[open_count++] = node;
    for (int i = open_count - 1; i > 0; i--) {
        if (open_list[i].f_cost < open_list[i - 1].f_cost) {
            Node temp = open_list[i];
            open_list[i] = open_list[i - 1];
            open_list[i - 1] = temp;
        }
    }
}

// 优化寻路：允许向低价值区域移动，增加路径容错性
bool findPath(struct Player *player, int startX, int startY, int targetX, int targetY, struct Point *path) {
    resetAStar();
    Node startNode = {startX, startY, 0, heuristic(startX, startY, targetX, targetY), 
                      heuristic(startX, startY, targetX, targetY), NULL};
    addToOpenList(startNode);

    while (open_count > 0) {
        Node current = open_list[0];
        if (current.x == targetX && current.y == targetY) {
            int pathLength = 0;
            Node *node = &current;
            while (node != NULL) {
                pathLength++;
                node = node->parent;
            }
            if (pathLength > 1) {
                node = &current;
                while (node->parent->parent != NULL) {
                    node = node->parent;
                }
                path->X = node->x;
                path->Y = node->y;
            } else {
                path->X = current.x;
                path->Y = current.y;
            }
            return true;
        }

        // 移动当前节点到关闭列表
        for (int i = 0; i < open_count - 1; i++) {
            open_list[i] = open_list[i + 1];
        }
        open_count--;
        closed_list[closed_count++] = current;

        // 搜索四个方向邻居（优先尝试移动，放宽限制）
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            if (nx < 0 || nx >= player->row_cnt || ny < 0 || ny >= player->col_cnt || 
                cell_type_map[nx][ny] == WALL) {
                continue;
            }
            if (isInClosedList(nx, ny)) {
                continue;
            }
            int newG = current.g_cost + 1;
            int index = isInOpenList(nx, ny);
            if (index == -1) {
                Node neighbor = {nx, ny, newG, heuristic(nx, ny, targetX, targetY), 
                                 newG + heuristic(nx, ny, targetX, targetY), &closed_list[closed_count - 1]};
                addToOpenList(neighbor);
            } else if (newG < open_list[index].g_cost) {
                open_list[index].g_cost = newG;
                open_list[index].f_cost = newG + open_list[index].h_cost;
                open_list[index].parent = &closed_list[closed_count - 1];
                for (int j = index; j > 0; j--) {
                    if (open_list[j].f_cost < open_list[j - 1].f_cost) {
                        Node temp = open_list[j];
                        open_list[j] = open_list[j - 1];
                        open_list[j - 1] = temp;
                    }
                }
            }
        }
    }
    return false;
}

// 统计剩余星星
int countRemainingStars(struct Player *player) {
    int count = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (player->mat[i][j] == 'o' || player->mat[i][j] == 'O') {
                count++;
            }
        }
    }
    return count;
}

// 增强目标选择：即使没有高价值目标，也选择最近的可达位置
bool findBestTarget(struct Player *player, struct Point *target) {
    int bestValue = INT_MIN;
    bool found = false;
    int remainingStars = countRemainingStars(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;

    // 优先找高价值目标
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == WALL || value_map[i][j] < -20) {  // 放宽低价值限制
                continue;
            }
            struct Point path;
            if (findPath(player, currentX, currentY, i, j, &path)) {
                int distance = heuristic(currentX, currentY, i, j);
                int adjustedValue = value_map[i][j] - distance / 3;  // 降低距离惩罚，鼓励移动

                if (cell_type_map[i][j] == SUPER_STAR) adjustedValue += 8;
                else if (cell_type_map[i][j] == NORMAL_STAR) adjustedValue += (10 - remainingStars) / 2;
                if (player->your_status <= 0 && cell_type_map[i][j] == SUPER_STAR) adjustedValue += 10;

                // 对手负分处理
                if (cell_type_map[i][j] == OPPONENT && player->opponent_score < 0) {
                    adjustedValue -= 10;
                }

                if (adjustedValue > bestValue) {
                    bestValue = adjustedValue;
                    target->X = i;
                    target->Y = j;
                    found = true;
                }
            }
        }
    }

    // 如果没找到目标，找最近的空白区域（强制有目标）
    if (!found) {
        int minDist = INT_MAX;
        for (int i = 0; i < player->row_cnt; i++) {
            for (int j = 0; j < player->col_cnt; j++) {
                if (cell_type_map[i][j] != WALL && (cell_type_map[i][j] == EMPTY || 
                    cell_type_map[i][j] == NORMAL_STAR || cell_type_map[i][j] == SUPER_STAR)) {
                    int dist = heuristic(currentX, currentY, i, j);
                    if (dist < minDist) {
                        minDist = dist;
                        target->X = i;
                        target->Y = j;
                        found = true;
                    }
                }
            }
        }
    }

    return found;
}

// 初始化
void init(struct Player *player) {
    srand(time(NULL));
}

// 主移动函数：强化移动尝试，避免不动
struct Point walk(struct Player *player) {
    initMapInfo(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;

    // 强化状态逻辑：优先移动，避免停滞
    if (player->your_status > 0) {
        struct Point starTarget;
        if (findBestTarget(player, &starTarget)) {
            struct Point nextMove;
            if (findPath(player, currentX, currentY, starTarget.X, starTarget.Y, &nextMove)) {
                // 确保移动（即使目标不变，也尝试微调）
                if (nextMove.X == currentX && nextMove.Y == currentY) {
                    // 目标在当前位置，尝试向任意方向移动一步
                    int dx[] = {-1, 1, 0, 0};
                    int dy[] = {0, 0, -1, 1};
                    for (int i = 0; i < 4; i++) {
                        int nx = currentX + dx[i];
                        int ny = currentY + dy[i];
                        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
                            return (struct Point){nx, ny};
                        }
                    }
                }
                return nextMove;
            }
        }
    }

    // 正常状态：找最优目标移动
    struct Point bestTarget;
    if (findBestTarget(player, &bestTarget)) {
        struct Point nextMove;
        if (findPath(player, currentX, currentY, bestTarget.X, bestTarget.Y, &nextMove)) {
            // 确保移动（同上）
            if (nextMove.X == currentX && nextMove.Y == currentY) {
                int dx[] = {-1, 1, 0, 0};
                int dy[] = {0, 0, -1, 1};
                for (int i = 0; i < 4; i++) {
                    int nx = currentX + dx[i];
                    int ny = currentY + dy[i];
                    if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
                        return (struct Point){nx, ny};
                    }
                }
            }
            return nextMove;
        }
    }

    //尝试10次，按方向优先级（上下左右）
    int dx[4] = {-1, 1, 0, 0};  // 优先上下，再左右
    int dy[4] = {0, 0, -1, 1};
    for(int t = 0; t < MAX_RANDOM_TRIES; t++) {
        int dir = t % 4;  // 循环尝试四个方向，而非随机
        int nx = currentX + dx[dir];
        int ny = currentY + dy[dir];
        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
            return (struct Point){nx, ny};
        }
    }

    // 极端情况：向任意非墙区域移动（即使危险）
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (!isWall(player, i, j)) {
                return (struct Point){i, j};
            }
        }
    }

    // 最终兜底：返回当前位置（理论上不会走到这步）
    return (struct Point){currentX, currentY};
}
