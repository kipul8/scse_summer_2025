#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include "../include/playerbase.h"

#define MAP_MAX_ROWS 100
#define MAP_MAX_COLS 100
#define GHOST_DANGER_DISTANCE 4  // 降低危险距离，避免过度规避
#define OPPONENT_DANGER_DISTANCE 3
#define SUPER_STAR_VALUE 15      // 提高超级星价值
#define NORMAL_STAR_VALUE 5
#define EMPTY_CELL_VALUE -1
#define WALL_VALUE INT_MIN
#define MAX_RANDOM_TRIES 10
#define GHOST_PROFIT_BASE 20     // 击败幽灵基础分
#define GHOST_PROFIT_RANGE 2     // 幽灵周围星星的检测范围

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
                            value_map[nx][ny] -= (GHOST_DANGER_DISTANCE - distance + 1) * 2;
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
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1) * 3;
                        } else {
                            value_map[nx][ny] -= (OPPONENT_DANGER_DISTANCE - distance + 1);
                        }
                    }
                }
            }
        }
    }

    // 强化状态下，提升所有可移动区域价值，鼓励主动进攻
    if (player->your_status > 0) {
        for (int i = 0; i < player->row_cnt; i++) {
            for (int j = 0; j < player->col_cnt; j++) {
                if (cell_type_map[i][j] != WALL) {
                    value_map[i][j] += 5;
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
                newG += 5; // 非强化状态下，避开幽灵
            } else if (cell_type_map[nx][ny] == OPPONENT && player->opponent_status > 0) {
                newG += 5; // 避开强化状态的对手
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

// 计算击败幽灵的预期收益
int calculateGhostProfit(struct Player *player, int ghostX, int ghostY) {
    int baseProfit = GHOST_PROFIT_BASE; // 击败幽灵基础分
    
    // 额外收益：幽灵周围的星星价值
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
    
    // 强化状态持续时间折扣：剩余时间越少，价值越低
    float durationFactor = (player->your_status > 5) ? 1.0 : 0.5; 
    return (baseProfit + starProfit) * durationFactor;
}

// 检查路径是否会被幽灵围困
bool isPathTrapped(struct Player *player, struct Point nextMove, int ghostX, int ghostY) {
    int ghostCount = 0;
    int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
    
    // 检查下一步周围的幽灵数量
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
    
    return ghostCount >= 2; // 周围有2个以上幽灵，视为可能被困
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

// 增强目标选择：综合考虑星星、幽灵和对手
bool findBestTarget(struct Player *player, struct Point *target) {
    int bestValue = INT_MIN;
    bool found = false;
    int remainingStars = countRemainingStars(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;
    
    // 星星稀缺度加成：剩余星星越少，单星价值越高
    int starScarcity = (remainingStars < 10) ? 5 : (remainingStars < 20 ? 3 : 1); 

    // 优先找高价值目标
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (cell_type_map[i][j] == WALL || value_map[i][j] < -30) {
                continue;
            }
            
            struct Point path;
            if (findPath(player, currentX, currentY, i, j, &path)) {
                int distance = heuristic(currentX, currentY, i, j);
                int adjustedValue = value_map[i][j] - distance / 3;
                
                // 星星价值调整
                if (cell_type_map[i][j] == SUPER_STAR) {
                    adjustedValue += 10 + (10 - distance / 2); // 近的超级星价值更高
                } else if (cell_type_map[i][j] == NORMAL_STAR) {
                    adjustedValue += starScarcity + (5 - distance / 3);
                }
                
                // 强化状态下，幽灵变为高价值目标
                if (player->your_status > 0 && cell_type_map[i][j] == GHOST) {
                    adjustedValue += calculateGhostProfit(player, i, j);
                }
                
                // 对手处理：强化状态下攻击对手，非强化状态避开
                if (cell_type_map[i][j] == OPPONENT) {
                    if (player->your_status > 0 && player->opponent_status <= 0) {
                        // 我方强化，对手未强化，攻击对手
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

    // 如果没找到目标，找最近的空白区域（强制有目标）
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

// 主移动函数：强化移动尝试，避免不动
struct Point walk(struct Player *player) {
    initMapInfo(player);
    int currentX = player->your_posx;
    int currentY = player->your_posy;

    // 强化状态逻辑：优先攻击幽灵或对手
    if (player->your_status > 0) {
        // 评估幽灵的价值
        int bestGhostProfit = -1;
        int bestGhostIndex = -1;
        
        for (int g = 0; g < 2; g++) {
            int ghostX = player->ghost_posx[g];
            int ghostY = player->ghost_posy[g];
            
            if (ghostX >=0 && ghostX < player->row_cnt && ghostY >=0 && ghostY < player->col_cnt) {
                int ghostProfit = calculateGhostProfit(player, ghostX, ghostY);
                int distance = heuristic(currentX, currentY, ghostX, ghostY);
                
                // 距离太远的幽灵不考虑
                if (distance <= 5 && ghostProfit > bestGhostProfit) {
                    bestGhostProfit = ghostProfit;
                    bestGhostIndex = g;
                }
            }
        }
        
        // 如果幽灵价值足够高，尝试攻击
        if (bestGhostIndex != -1) {
            int ghostX = player->ghost_posx[bestGhostIndex];
            int ghostY = player->ghost_posy[bestGhostIndex];
            struct Point target = {ghostX, ghostY};
            struct Point nextMove;
            
            if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                // 检查路径安全性
                if (!isPathTrapped(player, nextMove, ghostX, ghostY)) {
                    return nextMove;
                }
            }
        }
        
        // 再考虑攻击对手
        int oppX = player->opponent_posx;
        int oppY = player->opponent_posy;
        if (oppX >=0 && oppX < player->row_cnt && oppY >=0 && oppY < player->col_cnt) {
            if (player->opponent_status <= 0) { // 对手未强化
                struct Point target = {oppX, oppY};
                struct Point nextMove;
                
                if (findPath(player, currentX, currentY, target.X, target.Y, &nextMove)) {
                    // 检查路径安全性
                    if (!isPathTrapped(player, nextMove, oppX, oppY)) {
                        return nextMove;
                    }
                }
            }
        }
    }

    // 正常状态：找最优目标移动（星星、超级星）
    struct Point bestTarget;
    if (findBestTarget(player, &bestTarget)) {
        struct Point nextMove;
        if (findPath(player, currentX, currentY, bestTarget.X, bestTarget.Y, &nextMove)) {
            // 确保移动
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

    // 尝试按方向优先级移动
    int dx[] = {-1, 1, 0, 0};  // 优先上下，再左右
    int dy[] = {0, 0, -1, 1};
    for (int t = 0; t < MAX_RANDOM_TRIES; t++) {
        int dir = t % 4;
        int nx = currentX + dx[dir];
        int ny = currentY + dy[dir];
        if (nx >= 0 && nx < player->row_cnt && ny >= 0 && ny < player->col_cnt && !isWall(player, nx, ny)) {
            return (struct Point){nx, ny};
        }
    }

    // 极端情况：向任意非墙区域移动
    for (int i = 0; i < player->row_cnt; i++) {
        for (int j = 0; j < player->col_cnt; j++) {
            if (!isWall(player, i, j)) {
                return (struct Point){i, j};
            }
        }
    }

    // 最终兜底：返回当前位置
    return (struct Point){currentX, currentY};
}
