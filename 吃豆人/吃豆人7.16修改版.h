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
#define OPPONENT_DANGER_DISTANCE 6
#define SUPER_STAR_VALUE 30     // 提高强化星价值
#define NORMAL_STAR_VALUE 5
#define GHOST_VALUE 20          // 吃鬼得分
#define EMPTY_CELL_VALUE -1
#define WALL_VALUE INT_MIN
#define MAX_RANDOM_TRIES 10

typedef struct Node {
    int x, y;
    int g_cost;         //已消耗
    int h_cost;         //预估消耗
    int f_cost;         //总代价
    struct Node* parent;
} Node;

typedef enum {
    EMPTY, WALL, NORMAL_STAR, SUPER_STAR, GHOST, OPPONENT
} CellType;

typedef enum {
    STATE_EAT_SUPER_STAR,     // 吃强化星
    STATE_HUNT_GHOST,         // 猎杀幽灵
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
        
        if (x == targetX && y == targetY) {
            return distance[x][y];
        }
        
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !visited[nx][ny] && !isWall(player, nx, ny)) {
                visited[nx][ny] = 1;
                distance[nx][ny] = distance[x][y] + 1;
                queue[tail++] = (struct Point){nx, ny};
            }
        }
    }
    
    return INT_MAX;
}

int heuristic(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 初始化地图信息
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
                            value_map[nx][ny] -= (GHOST_DANGER_DISTANCE - distance + 1) * 3;
                        }
                    }
                }
            }
        }
    }

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
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1) * 5;
                        } else {
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1);
                        }
                    }
                }
            }
        }
    }

    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == SUPER_STAR) {
                        value_map[i][j] += 20;
                    }
                }
            }
            break;
            
        case STATE_HUNT_GHOST:
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == GHOST) {
                        value_map[i][j] += GHOST_VALUE;
                    }
                }
            }
            break;
            
        case STATE_EAT_NORMAL_STAR:
            for (int i = 0; i < player->row_cnt; i++) {
                for (int j = 0; j < player->col_cnt; j++) {
                    if (cell_type_map[i][j] == NORMAL_STAR) {
                        value_map[i][j] += 5;
                    }
                }
            }
            break;
            
        case STATE_AVOID_GHOST:
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

void resetAStar() {
    open_count = 0;
    closed_count = 0;
}

bool isInClosedList(int x, int y) {
    for (int i = 0; i < closed_count; i++) {
        if (closed_list[i].x == x && closed_list[i].y == y) {
            return true;
        }
    }
    return false;
}

int isInOpenList(int x, int y) {
    for (int i = 0; i < open_count; i++) {
        if (open_list[i].x == x && open_list[i].y == y) {
            return i;
        }
    }
    return -1;
}

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

bool findPath(struct Player *player, int startX, int startY, int targetX, int targetY, struct Point *path) {
    resetAStar();
    Node startNode = {startX, startY, 0, realDistance(player, startX, startY, targetX, targetY), realDistance(player, startX, startY, targetX, targetY), NULL};
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

        for (int i = 0; i < open_count - 1; i++) {
            open_list[i] = open_list[i + 1];
        }
        open_count--;
        closed_list[closed_count++] = current;

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
            if (cell_type_map[nx][ny] == GHOST && player->your_status <= 0) {
                newG += 10000;
            } else if (cell_type_map[nx][ny] == OPPONENT && player->opponent_status > player -> your_status) {
                newG += 10000;
            } else if (cell_type_map[nx][ny] == 'o' || cell_type_map[nx][ny] == 'O'){
                newG -= 1;
            } else if (cell_type_map[nx][ny] == OPPONENT && player->your_status > player -> opponent_status && player -> opponent_score < 10){
                newG += 10000;
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

// 更新AI状态（移除堵出生点相关逻辑）
void updateAIState(struct Player *player) {
    int superStarCount = countRemainingSuperStars(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    
    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            if (player->your_status > 0) {
                currentState = STATE_HUNT_GHOST;
            } else if (superStarCount == 0) {
                currentState = STATE_EAT_NORMAL_STAR;
            }
            break;
            
        case STATE_HUNT_GHOST:
            if (player->your_status <= 0) {
                if (superStarCount > 0) {
                    currentState = STATE_EAT_SUPER_STAR;
                } else {
                    currentState = STATE_EAT_NORMAL_STAR;
                }
            } else if (player->your_status < 5 && superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 5) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
            break;
            
        case STATE_EAT_NORMAL_STAR:
            if (superStarCount > 0) {
                struct Point nextSuperStar;
                if (findNearestSuperStar(player, currentX, currentY, &nextSuperStar)) {
                    if (realDistance(player, currentX, currentY, nextSuperStar.X, nextSuperStar.Y) < 10) {
                        currentState = STATE_EAT_SUPER_STAR;
                    }
                }
            }
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

void init(struct Player *player) {
    srand(time(NULL));
    currentState = STATE_EAT_SUPER_STAR;
}

struct Point walk(struct Player *player) {
    updateAIState(player);
    initMapInfo(player);
    
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    struct Point target;
    struct Point nextMove;
    
    switch (currentState) {
        case STATE_EAT_SUPER_STAR:
            if (findNearestSuperStar(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            currentState = STATE_EAT_NORMAL_STAR;
            break;
            
        case STATE_HUNT_GHOST:
            if (findNearestGhost(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            break;
            
        case STATE_EAT_NORMAL_STAR:
            if (findNearestNormalStar(player, currentX, currentY, &target)) {
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    return nextMove;
                }
            }
            break;
            
        case STATE_AVOID_GHOST:
            int bestX = currentX;
            int bestY = currentY;
            int maxDistance = -1;
            
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};
            
            for (int i = 0; i < 4; i++) {
                int nx = currentX + dx[i];
                int ny = currentY + dy[i];
                
                if (nx >=0 && nx < player->row_cnt && ny >=0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
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
                    
                    if (minGhostDist > maxDistance) {
                        maxDistance = minGhostDist;
                        bestX = nx;
                        bestY = ny;
                    }
                }
            }
            
            if (maxDistance > 1) {
                return (struct Point){bestX, bestY};
            }
            break;
    }
    
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
    
    return (struct Point){currentX, currentY};
}
