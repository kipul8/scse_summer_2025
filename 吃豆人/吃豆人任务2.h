#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include "../include/playerbase.h"

#define MAP_MAX_ROWS 100
#define MAP_MAX_COLS 100
#define GHOST_DANGER_DISTANCE 6
#define GHOST_SPAWN_DISTANCE 3  // 鬼出生点附近距离
#define OPPONENT_DANGER_DISTANCE 8
#define SUPER_STAR_VALUE 30     // 提高强化星价值
#define NORMAL_STAR_VALUE 5
#define GHOST_VALUE 20          // 吃鬼得分
#define GHOST_SPAWN_WAIT 15     // 在出生点等待时间
#define EMPTY_CELL_VALUE -1
#define WALL_VALUE INT_MIN
#define MAX_RANDOM_TRIES 10

typedef struct Node {
    int x, y;
    int g_cost;
    int h_cost;
    int f_cost;
    struct Node* parent;
} Node;

typedef enum {
    EMPTY, WALL, NORMAL_STAR, SUPER_STAR, GHOST, OPPONENT, GHOST_SPAWN
} CellType;

typedef enum {
    STATE_EAT_SUPER_STAR,     // 吃强化星
    STATE_HUNT_GHOST,         // 猎杀幽灵
    STATE_BLOCK_SPAWN,        // 堵幽灵出生点
    STATE_EAT_NORMAL_STAR,    // 吃普通星
    STATE_AVOID_GHOST         // 躲避幽灵
} AIState;

Node open_list[MAP_MAX_ROWS * MAP_MAX_COLS];
Node closed_list[MAP_MAX_ROWS * MAP_MAX_COLS];
int open_count = 0;
int closed_count = 0;
CellType cell_type_map[MAP_MAX_ROWS][MAP_MAX_COLS];
int value_map[MAP_MAX_ROWS][MAP_MAX_COLS];
AIState currentState = STATE_EAT_SUPER_STAR;  // 初始状态
int spawnBlockTime = 0;                       // 记录堵出生点的时间

bool isWall(struct Player *player, int x, int y) {
    return player->mat[x][y] == '#';
}

// 考虑墙体的真实距离计算（使用简化的BFS）
int realDistance(struct Player *player, int startX, int startY, int targetX, int targetY) {
    if (startX == targetX && startY == targetY) return 0;
    if (isWall(player, startX, startY) || isWall(player, targetX, targetY)) return INT_MAX;
    
    int visited[MAP_MAX_ROWS][MAP_MAX_COLS] = {0};
    int distance[MAP_MAX_ROWS][MAP_MAX_COLS] = {0};
    struct Point queue[MAP_MAX_ROWS * MAP_MAX_COLS];
    int head = 0, tail = 0;
    
    queue[tail++] = (struct Point){startX, startY};
    visited[startX][startY] = 1;
    
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    while (head < tail) {
        struct Point current = queue[head++];
        int x = current.X;
        int y = current.Y;
        
        // 找到目标点，返回距离
        if (x == targetX && y == targetY) {
            return distance[x][y];
        }
        
        // 检查四个方向
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt &&
                !visited[nx][ny] && !isWall(player, nx, ny)) {
                visited[nx][ny] = 1;
                distance[nx][ny] = distance[x][y] + 1;
                queue[tail++] = (struct Point){nx, ny};
            }
        }
    }
    
    // 无法到达目标点
    return INT_MAX;
}

// 曼哈顿距离（仅用于启发式，不考虑墙体）
int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 初始化地图信息
void initMapInfo(struct Player *player) {
    // 初始化地图基础值
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

    // 标记幽灵出生点（假设出生点为固定位置）
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            // 假设幽灵出生点是地图上特定位置，这里简化为幽灵初始位置
            if (player->ghost_posx[0] == i && player->ghost_posy[0] == j) {
                cell_type_map[i][j] = GHOST_SPAWN;
                value_map[i][j] = 0;  // 初始值为0，后续根据状态调整
            }
        }
    }

    // 幽灵危险区域（非强化状态下有效）
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
                        if (distance <= GHOST_DANGER_DISTANCE && player->your_status <= 0) {
                            // 非强化状态下，幽灵附近价值降低
                            value_map[nx][ny] -= (GHOST_DANGER_DISTANCE - distance + 1) * 3;
                        }
                    }
                }
            }
        }
    }

    // 对手危险区域
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
                            // 对手强化状态，危险更高
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1) * 5;
                        } else {
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1);
                        }
                    }
                }
            }
        }
    }

    // 根据AI状态调整地图价值
    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            // 提高所有超级星价值
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == SUPER_STAR) {
                        value_map[i][j] += 20;
                    }
                }
            }
            break;
            
        case STATE_HUNT_GHOST:
            // 强化状态下，幽灵变为高价值目标
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == GHOST) {
                        value_map[i][j] += GHOST_VALUE;
                    }
                }
            }
            break;
            
        case STATE_BLOCK_SPAWN:
            // 提高幽灵出生点附近价值
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == GHOST_SPAWN) {
                        // 出生点周围区域价值提升
                        for (int dx = -GHOST_SPAWN_DISTANCE; dx <= GHOST_SPAWN_DISTANCE; dx++) {
                            for (int dy = -GHOST_SPAWN_DISTANCE; dy <= GHOST_SPAWN_DISTANCE; dy++) {
                                int nx = i + dx;
                                int ny = j + dy;
                                if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt) {
                                    value_map[nx][ny] += 15;
                                }
                            }
                        }
                    }
                }
            }
            break;
            
        case STATE_EAT_NORMAL_STAR:
            // 提高普通星价值
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == NORMAL_STAR) {
                        value_map[i][j] += 5;
                    }
                }
            }
            break;
            
        case STATE_AVOID_GHOST:
            // 非强化状态下，进一步降低幽灵附近价值
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == GHOST) {
                        for (int dx = -GHOST_DANGER_DISTANCE; dx <= GHOST_DANGER_DISTANCE; dx++) {
                            for (int dy = -GHOST_DANGER_DISTANCE; dy <= GHOST_DANGER_DISTANCE; dy++) {
                                int nx = i + dx;
                                int ny = j + dy;
                                if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt) {
                                    int distance = abs(dx) + abs(dy);
                                    if (distance <= GHOST_DANGER_DISTANCE) {
                                        value_map[nx][ny] -= (GHOST_DANGER_DISTANCE - distance + 1) * 2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
    }
}

// 重置A*寻路
void resetAStar() {
    open_count = 0;
    closed_count = 0;
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

        // 搜索四个方向邻居
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
            
            // 计算新的G值，考虑危险区域额外成本
            int newG = current.g_cost + 1;
            if (cell_type_map[nx][ny] == GHOST && player->your_status <= 0) {
                newG += 10; // 非强化状态下，避开幽灵
            } else if (cell_type_map[nx][ny] == OPPONENT && player->opponent_status > 0) {
                newG += 10; // 避开强化状态的对手
            }
            
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

// 统计剩余强化星
int countRemainingSuperStars(struct Player *player) {
    int count = 0;
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (player->mat[i][j] == 'O') {
                count++;
            }
        }
    }
    return count;
}

// 寻找最近的强化星
bool findNearestSuperStar(struct Player *player, int currentX, int currentY, struct Point *target) {
    int minDist = INT_MAX;
    bool found = false;
    
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == SUPER_STAR) {
                int dist = realDistance(player, currentX, currentY, i, j);
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

// 寻找最近的幽灵
bool findNearestGhost(struct Player *player, int currentX, int currentY, struct Point *target) {
    int minDist = INT_MAX;
    bool found = false;
    
    for (int g = 0; g < 2; g++) {
        int ghostX = player->ghost_posx[g];
        int ghostY = player->ghost_posy[g];
        
        if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
            int dist = realDistance(player, currentX, currentY, ghostX, ghostY);
            if (dist < minDist) {
                minDist = dist;
                target->X = ghostX;
                target->Y = ghostY;
                found = true;
            }
        }
    }
    
    return found;
}

// 寻找最近的幽灵出生点
bool findNearestGhostSpawn(struct Player *player, int currentX, int currentY, struct Point *target) {
    int minDist = INT_MAX;
    bool found = false;
    
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == GHOST_SPAWN) {
                int dist = realDistance(player, currentX, currentY, i, j);
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

// 寻找最近的普通星
bool findNearestNormalStar(struct Player *player, int currentX, int currentY, struct Point *target) {
    int minDist = INT_MAX;
    bool found = false;
    
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == NORMAL_STAR) {
                int dist = realDistance(player, currentX, currentY, i, j);
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

// 状态机逻辑：根据游戏状态决定下一步策略
void updateAIState(struct Player *player) {
    int superStarCount = countRemainingSuperStars(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    
    // 幽灵出生点位置（假设已知）
    struct Point spawnPoint;
    bool hasSpawnPoint = findNearestGhostSpawn(player, currentX, currentY, &spawnPoint);
    
    // 根据当前状态进行状态转换
    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            // 吃到强化星后，切换到猎杀幽灵状态
            if (player->your_status > 0) {
                currentState = STATE_HUNT_GHOST;
                spawnBlockTime = 0;
            }
            // 如果没有强化星了，切换到吃普通星状态
            else if (superStarCount == 0) {
                currentState = STATE_EAT_NORMAL_STAR;
            }
            break;
            
        case STATE_HUNT_GHOST:
            // 强化状态结束，寻找下一个强化星
            if (player->your_status <= 0) {
                if (superStarCount > 0) {
                    currentState = STATE_EAT_SUPER_STAR;
                } else {
                    currentState = STATE_EAT_NORMAL_STAR;
                }
            }
            // 强化时间快结束（剩余时间小于5），且附近有强化星，提前去吃
            else if (player->your_status < 5 && superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 5) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
            // 如果没有幽灵可吃了（或距离太远），切换到堵出生点状态
            else {
                struct Point nearestGhost;
                if (!findNearestGhost(player, currentX, currentY, &nearestGhost) || 
                    realDistance(player, currentX, currentY, nearestGhost.X, nearestGhost.Y) > 8) {
                    if (hasSpawnPoint) {
                        currentState = STATE_BLOCK_SPAWN;
                        spawnBlockTime = 0;
                    }
                }
            }
            break;
            
        case STATE_BLOCK_SPAWN:
            // 强化状态结束，寻找下一个强化星
            if (player->your_status <= 0) {
                if (superStarCount > 0) {
                    currentState = STATE_EAT_SUPER_STAR;
                } else {
                    currentState = STATE_EAT_NORMAL_STAR;
                }
            }
            // 在出生点等待足够长时间后，切换回猎杀幽灵状态
            else if (spawnBlockTime >= GHOST_SPAWN_WAIT) {
                currentState = STATE_HUNT_GHOST;
                spawnBlockTime = 0;
            }
            // 强化时间快结束，且附近有强化星，提前去吃
            else if (player->your_status < 5 && superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 5) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
            else {
                spawnBlockTime++;  // 增加堵出生点的时间计数
            }
            break;
            
        case STATE_EAT_NORMAL_STAR:
            // 遇到强化星，切换到吃强化星状态
            if (superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 10) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
            // 被幽灵追赶，切换到躲避状态
            for (int g = 0; g < 2; g++) {
                int ghostX = player->ghost_posx[g];
                int ghostY = player->ghost_posy[g];
                if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
                    if (realDistance(player, currentX, currentY, ghostX, ghostY) < 4) {
                        currentState = STATE_AVOID_GHOST;
                        break;
                    }
                }
            }
            break;
            
        case STATE_AVOID_GHOST:
            // 成功避开幽灵，切换回吃普通星状态
            bool ghostNearby = false;
            for (int g = 0; g < 2; g++) {
                int ghostX = player->ghost_posx[g];
                int ghostY = player->ghost_posy[g];
                if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
                    if (realDistance(player, currentX, currentY, ghostX, ghostY) < 5) {
                        ghostNearby = true;
                        break;
                    }
                }
            }
            if (!ghostNearby) {
                currentState = STATE_EAT_NORMAL_STAR;
            }
            // 遇到强化星，切换到吃强化星状态
            if (superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 10) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
            break;
    }
}

// 初始化
void init(struct Player *player) {
    srand(time(NULL));
    currentState = STATE_EAT_SUPER_STAR;  // 初始状态：寻找强化星
    spawnBlockTime = 0;
}

// 主移动函数
struct Point walk(struct Player *player) {
    // 更新AI状态
    updateAIState(player);
    
    // 初始化地图信息（根据当前状态设置价值地图）
    initMapInfo(player);
    
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    struct Point target;
    struct Point nextMove;
    
    // 根据当前状态决定目标
    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            if (findNearestSuperStar(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            // 如果找不到强化星，退回到吃普通星状态
            currentState = STATE_EAT_NORMAL_STAR;
            break;
            
        case STATE_HUNT_GHOST:
            if (findNearestGhost(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            // 如果找不到幽灵，切换到堵出生点状态
            if (findNearestGhostSpawn(player, currentX, currentY, &target)) {
                currentState = STATE_BLOCK_SPAWN;
                spawnBlockTime = 0;
            }
            break;
            
        case STATE_BLOCK_SPAWN:
            if (findNearestGhostSpawn(player, currentX, currentY, &target)) {
                // 找到出生点附近的最佳位置（不是直接站在出生点上）
                int bestX = target.X;
                int bestY = target.Y;
                int bestValue = INT_MIN;
                
                // 检查出生点周围的位置，找最佳堵点
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int nx = target.X + dx;
                        int ny = target.Y + dy;
                        if (nx >=0 && nx < player->row_cnt && ny >=0 && ny < player->col_cnt && 
                            !isWall(player, nx, ny)) {
                            int value = value_map[nx][ny] - realDistance(player, currentX, currentY, nx, ny);
                            if (value > bestValue) {
                                bestValue = value;
                                bestX = nx;
                                bestY = ny;
                            }
                        }
                    }
                }
                
                target.X = bestX;
                target.Y = bestY;
                
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            // 如果找不到出生点，切换回猎杀幽灵状态
            currentState = STATE_HUNT_GHOST;
            break;
            
        case STATE_EAT_NORMAL_STAR:
            if (findNearestNormalStar(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            // 如果找不到普通星，尝试随机移动
            break;
            
        case STATE_AVOID_GHOST:
            // 计算与所有幽灵的距离，选择远离幽灵的方向
            int bestX = currentX;
            int bestY = currentY;
            int maxDistance = -1;
            
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};
            
            for (int i = 0; i < 4; i++) {
                int nx = currentX + dx[i];
                int ny = currentY + dy[i];
                
                if (nx >=0 && nx < player->row_cnt && ny >=0 && ny < player->col_cnt && 
                    !isWall(player, nx, ny)) {
                    // 计算与所有幽灵的最小距离
                    int minGhostDist = INT_MAX;
                    for (int g = 0; g < 2; g++) {
                        int ghostX = player->ghost_posx[g];
                        int ghostY = player->ghost_posy[g];
                        if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
                            int dist = realDistance(player, nx, ny, ghostX, ghostY);
                            if (dist < minGhostDist) {
                                minGhostDist = dist;
                            }
                        }
                    }
                    
                    // 选择距离幽灵最远的方向
                    if (minGhostDist > maxDistance) {
                        maxDistance = minGhostDist;
                        bestX = nx;
                        bestY = ny;
                    }
                }
            }
            
            // 如果找到了更好的位置，移动过去
            if (maxDistance > 1) {
                return (struct Point){bestX, bestY};
            }
            break;
    }
    
    // 如果上面的逻辑都没有找到合适的移动位置，尝试随机移动
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    for (int i = 0; i < MAX_RANDOM_TRIES; i++) {
        int randomIndex = rand() % 4;
        int nx = currentX + dx[randomIndex];
        int ny = currentY + dy[randomIndex];
        
        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && 
            !isWall(player, nx, ny)) {
            return (struct Point){nx, ny};
        }
    }
    
    // 最终兜底：返回当前位置
    return (struct Point){currentX, currentY};
}
