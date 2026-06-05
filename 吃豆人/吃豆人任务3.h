#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include "../include/playerbase.h"

#define MAP_MAX_ROWS 100
#define MAP_MAX_COLS 100
#define GHOST_DANGER_DISTANCE 4  
#define OPPONENT_DANGER_DISTANCE 3
#define SUPER_STAR_VALUE 15      
#define NORMAL_STAR_VALUE 5
#define EMPTY_CELL_VALUE -1
#define WALL_VALUE INT_MIN
#define MAX_RANDOM_TRIES 10
#define GHOST_PROFIT_BASE 20     
#define GHOST_PROFIT_RANGE 2     
#define SAFE_DISTANCE_THRESHOLD 6 

typedef struct Node {
    int x, y;
    int g_cost;         // 实际代价（包含星星加成）
    int h_cost;         // 预估代价（曼哈顿距离）
    int star_count;     // 路径上的星星数量（用于优先选多星星路径）
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

// 初始化地图信息（处理对手消失逻辑）
void initMapInfo(struct Player *player) {
    // 1. 初始化基础地图
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

    // 2. 处理对手消失（被消灭）的情况
    bool opponentExists = (player->opponent_status != -1) && (player->opponent_score >= 0);
    if (!opponentExists) {
        // 清除对手位置标记
        int opp_x = player->opponent_posx;
        int opp_y = player->opponent_posy;
        if (opp_x >= 0 && opp_x < player->row_cnt && opp_y >= 0 && opp_y < player->col_cnt) {
            cell_type_map[opp_x][opp_y] = EMPTY;
        }
    }

    // 3. 幽灵危险区域
    for (int i = 0; i < 2; i++) {
        int x = player->ghost_posx[i];
        int y = player->ghost_posy[i];
        if (x >= 0 && x < player->row_cnt && y >= 0 && y < player->col_cnt) {
            cell_type_map[x][y] = GHOST;
            int dangerDistance = GHOST_DANGER_DISTANCE;
            if (player->your_status > 0 && player->your_status <= 3) {
                dangerDistance += 2;
            }
            for (int dx = -dangerDistance; dx <= dangerDistance; dx++) {
                for (int dy = -dangerDistance; dy <= dangerDistance; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt) {
                        int distance = abs(dx) + abs(dy);
                        if (distance <= dangerDistance && (player->your_status <= 0 || player->your_status <= 3)) {
                            int dangerWeight = (player->your_status <= 3) ? 3 : 2;
                            value_map[nx][ny] -= (dangerDistance - distance + 1) * dangerWeight;
                        }
                    }
                }
            }
        }
    }

    // 4. 对手危险区域（仅对手存在时处理）
    if (opponentExists) {
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
                                value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1) * 3;
                            } else {
                                value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1);
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. 强化状态价值加成
    int attackBoost = (player->your_status > 3) ? 5 : 2;
    if (player->your_status > 0) {
        for (int i = 0; i < player->row_cnt; i++) {
            for (int j = 0; j < player->col_cnt; j++) {
                if (cell_type_map[i][j] != WALL) {
                    value_map[i][j] += attackBoost;
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

// 添加节点到开放列表并排序（优先星星多、代价低的路径）
void addToOpenList(Node node) {
    open_list[open_count++] = node;
    // 排序：f_cost优先，相同则星星多的排前面
    for (int i = open_count - 1; i > 0; i--) {
        if (open_list[i].g_cost + open_list[i].h_cost < open_list[i-1].g_cost + open_list[i-1].h_cost ||
            (open_list[i].g_cost + open_list[i].h_cost == open_list[i-1].g_cost + open_list[i-1].h_cost &&
             open_list[i].star_count > open_list[i-1].star_count)) {
            Node temp = open_list[i];
            open_list[i] = open_list[i-1];
            open_list[i-1] = temp;
        }
    }
}

// 寻路优化：优先选择顺路星星多的路径
bool findPath(struct Player *player, int startX, int startY, int targetX, int targetY, struct Point *path) {
    resetAStar();
    // 初始节点：包含星星计数（起点是否有星星）
    int startStar = (cell_type_map[startX][startY] == NORMAL_STAR || cell_type_map[startX][startY] == SUPER_STAR) ? 1 : 0;
    Node startNode = {startX, startY, 0, heuristic(startX, startY, targetX, targetY), startStar, NULL};
    addToOpenList(startNode);

    while (open_count > 0) {
        Node current = open_list[0];
        if (current.x == targetX && current.y == targetY) {
            // 回溯获取第一步移动
            Node *node = &current;
            while (node->parent != NULL && node->parent->parent != NULL) {
                node = node->parent;
            }
            path->X = node->x;
            path->Y = node->y;
            return true;
        }

        // 移动当前节点到关闭列表
        for (int i = 0; i < open_count - 1; i++) {
            open_list[i] = open_list[i + 1];
        }
        open_count--;
        closed_list[closed_count++] = current;

        // 搜索四方向邻居
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

            // 计算代价：经过星星时降低代价（鼓励走星星多的路）
            int newG = current.g_cost + 1;
            int newStarCount = current.star_count;
            if (cell_type_map[nx][ny] == NORMAL_STAR) {
                newG -= 2;  // 普通星星代价降低
                newStarCount++;
            } else if (cell_type_map[nx][ny] == SUPER_STAR) {
                newG -= 5;  // 超级星星代价降低更多
                newStarCount++;
            }

            // 危险区域代价加成
            if (cell_type_map[nx][ny] == GHOST && player->your_status <= 0) {
                newG += 5;
            } else if (cell_type_map[nx][ny] == OPPONENT && player->opponent_status > 0) {
                newG += 5;
            }

            int index = isInOpenList(nx, ny);
            if (index == -1) {
                Node neighbor = {nx, ny, newG, heuristic(nx, ny, targetX, targetY), newStarCount, &closed_list[closed_count - 1]};
                addToOpenList(neighbor);
            } else {
                // 已有节点：若新路径星星更多或代价更低，则更新
                if (newG < open_list[index].g_cost || 
                    (newG == open_list[index].g_cost && newStarCount > open_list[index].star_count)) {
                    open_list[index].g_cost = newG;
                    open_list[index].star_count = newStarCount;
                    open_list[index].parent = &closed_list[closed_count - 1];
                    // 重新排序
                    for (int j = index; j > 0; j--) {
                        if (open_list[j].g_cost + open_list[j].h_cost < open_list[j-1].g_cost + open_list[j-1].h_cost ||
                            (open_list[j].g_cost + open_list[j].h_cost == open_list[j-1].g_cost + open_list[j-1].h_cost &&
                             open_list[j].star_count > open_list[j-1].star_count)) {
                            Node temp = open_list[j];
                            open_list[j] = open_list[j-1];
                            open_list[j-1] = temp;
                        }
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

// 计算击败幽灵的预期收益
int calculateGhostProfit(struct Player *player, int ghostX, int ghostY) {
    int baseProfit = GHOST_PROFIT_BASE;
    int starProfit = 0;
    for (int dx = -GHOST_PROFIT_RANGE; dx <= GHOST_PROFIT_RANGE; dx++) {
        for (int dy = -GHOST_PROFIT_RANGE; dy <= GHOST_PROFIT_RANGE; dy++) {
            int nx = ghostX + dx;
            int ny = ghostY + dy;
            if (nx >=0 && nx < player->row_cnt && ny >=0 && ny < player->col_cnt) {
                if (cell_type_map[nx][ny] == NORMAL_STAR) {
                    starProfit += NORMAL_STAR_VALUE;
                } else if (cell_type_map[nx][ny] == SUPER_STAR) {
                    starProfit += SUPER_STAR_VALUE;
                }
            }
        }
    }
    float durationFactor = (player->your_status > 5) ? 1.0 : (player->your_status > 2) ? 0.7 : 0.3;
    return (baseProfit + starProfit) * durationFactor;
}

// 检查路径是否会被幽灵围困
bool isPathTrapped(struct Player *player, struct Point nextMove, int ghostX, int ghostY) {
    int ghostCount = 0;
    int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
    for (int i = 0; i < 8; i++) {
        int nx = nextMove.X + dx[i];
        int ny = nextMove.Y + dy[i];
        if (nx >=0 && nx < player->row_cnt && ny >=0 && ny < player->col_cnt) {
            for (int g = 0; g < 2; g++) {
                if (nx == player->ghost_posx[g] && ny == player->ghost_posy[g]) {
                    ghostCount++;
                }
            }
        }
    }
    return ghostCount >= 2;
}

// 寻找最近的星星
bool findNearestStar(struct Player *player, int currentX, int currentY, struct Point *target) {
    int minDist = INT_MAX;
    bool found = false;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == NORMAL_STAR || cell_type_map[i][j] == SUPER_STAR) {
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
    return found;
}

// 寻找强化结束前的安全逃生点
struct Point findSafestEscape(struct Player *player, int currentX, int currentY) {
    struct Point bestEscape = {-1, -1};
    int maxSafety = INT_MIN;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == WALL) continue;

            // 计算与幽灵的最小距离
            int minGhostDist = INT_MAX;
            for (int g = 0; g < 2; g++) {
                int dist = heuristic(i, j, player->ghost_posx[g], player->ghost_posy[g]);
                if (dist < minGhostDist) minGhostDist = dist;
            }
            
            // 计算与对手的距离（对手不存在则忽略）
            int oppDist = (player->opponent_status == -1 || player->opponent_score < 0) ? 
                         SAFE_DISTANCE_THRESHOLD : heuristic(i, j, player->opponent_posx, player->opponent_posy);

            // 安全价值：幽灵距离权重最高
            int safetyValue = minGhostDist * 3 + oppDist * 2 + value_map[i][j];
            int distFromCurrent = heuristic(i, j, currentX, currentY);

            if (distFromCurrent <= 8 && safetyValue > maxSafety) {
                maxSafety = safetyValue;
                bestEscape.X = i;
                bestEscape.Y = j;
            }
        }
    }

    // 生成到安全点的路径
    if (bestEscape.X != -1) {
        struct Point path;
        if (findPath(player, currentX, currentY, bestEscape.X, bestEscape.Y, &path)) {
            return path;
        }
    }
    
    // 无安全路径时随机移动
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        int nx = currentX + dx[i];
        int ny = currentY + dy[i];
        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
            return (struct Point){nx, ny};
        }
    }
    
    return (struct Point){currentX, currentY};
}

// 增强目标选择：对手负分时不攻击，优先星星多的路径
bool findBestTarget(struct Player *player, struct Point *target) {
    int bestValue = INT_MIN;
    bool found = false;
    int remainingStars = countRemainingStars(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    int starScarcity = (remainingStars < 10) ? 5 : (remainingStars < 20 ? 3 : 1);

    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == WALL || value_map[i][j] < -30) continue;
            
            struct Point path;
            if (findPath(player, currentX, currentY, i, j, &path)) {
                int distance = heuristic(currentX, currentY, i, j);
                int adjustedValue = value_map[i][j] - distance / 3;
                
                // 星星价值加成
                if (cell_type_map[i][j] == SUPER_STAR) {
                    adjustedValue += 10 + (10 - distance / 2);
                } else if (cell_type_map[i][j] == NORMAL_STAR) {
                    adjustedValue += starScarcity + (5 - distance / 3);
                }
                
                // 强化状态下的幽灵目标
                if (player->your_status > 0 && cell_type_map[i][j] == GHOST) {
                    adjustedValue += calculateGhostProfit(player, i, j);
                }
                
                // 对手处理：负分/已消灭则不攻击，甚至避开
                if (cell_type_map[i][j] == OPPONENT) {
                    if (player->opponent_score < 0 || player->opponent_status == -1) {
                        // 对手负分或已消失，不攻击
                        adjustedValue -= 20;
                    } else if (player->your_status > 0 && player->opponent_status <= 0) {
                        // 对手正分且未强化，才攻击
                        adjustedValue += 20 - distance;
                    } else if (player->opponent_status > 0) {
                        // 对手强化，避开
                        adjustedValue -= 30;
                    }
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

    // 无目标时找最近空白区域
    if (!found) {
        int minDist = INT_MAX;
        for (int i = 0; i < player->row_cnt; i++) {
            for (int j = 0; j < player->col_cnt; j++) {
                if (cell_type_map[i][j] != WALL) {
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

// 主移动函数
struct Point walk(struct Player *player) {
    initMapInfo(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;

    if (player->your_status == -1) {
        return (struct Point){0, 0}; // 死亡状态不移动
    }

    // 强化状态逻辑
    if (player->your_status > 0) {
        // 评估幽灵价值
        int bestGhostProfit = -1;
        int bestGhostIndex = -1;
        for (int g = 0; g < 2; g++) {
            int ghostX = player->ghost_posx[g];
            int ghostY = player->ghost_posy[g];
            if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
                int ghostProfit = calculateGhostProfit(player, ghostX, ghostY);
                int distance = heuristic(currentX, currentY, ghostX, ghostY);
                if (distance <= 5 && ghostProfit > bestGhostProfit) {
                    bestGhostProfit = ghostProfit;
                    bestGhostIndex = g;
                }
            }
        }
        
        // 攻击高价值幽灵
        if (bestGhostIndex != -1) {
            int ghostX = player->ghost_posx[bestGhostIndex];
            int ghostY = player->ghost_posy[bestGhostIndex];
            struct Point target = {ghostX, ghostY};
            struct Point nextMove;
            if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove) && 
                !isPathTrapped(player, nextMove, ghostX, ghostY)) {
                int stepsToGhost = heuristic(currentX, currentY, ghostX, ghostY);
                if (player->your_status > stepsToGhost + 1) {
                    return nextMove;
                }
            }
        }
        
        // 攻击对手（仅对手正分且存在时）
        if (player->opponent_score >= 0 && player->opponent_status != -1) {
            int oppX = player->opponent_posx;
            int oppY = player->opponent_posy;
            if (oppX >=0 && oppX < player->row_cnt && oppY >=0 && oppY < player->col_cnt && player->opponent_status <= 0) {
                struct Point target = {oppX, oppY};
                struct Point nextMove;
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove) && 
                    !isPathTrapped(player, nextMove, oppX, oppY)) {
                    int stepsToOpp = heuristic(currentX, currentY, oppX, oppY);
                    if (player->your_status > stepsToOpp + 1) {
                        return nextMove;
                    }
                }
            }
        }
        
        // 强化快结束时避险
        if (player->your_status <= 2) { 
            struct Point safestPos = findSafestEscape(player, currentX, currentY);
            if (safestPos.X != -1) {
                return safestPos;
            }
        }
    }

    // 正常状态：优先星星多的路径
    struct Point bestTarget;
    if (findBestTarget(player, &bestTarget)) {
        struct Point nextMove;
        if (findPath(player, currentX, currentY, bestTarget.X, bestTarget.Y, &nextMove)) {
            if (nextMove.X == currentX && nextMove.Y == currentY) {
                // 目标在当前位置时随机移动
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

    // 无有效目标时随机移动
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    for (int t = 0; t < MAX_RANDOM_TRIES; t++) {
        int dir = t % 4;
        int nx = currentX + dx[dir];
        int ny = currentY + dy[dir];
        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
            return (struct Point){nx, ny};
        }
    }

    return (struct Point){currentX, currentY};
}
