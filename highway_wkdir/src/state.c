#include "state.h"
#define INF 1000000000

int n;
// 定义方向数组
int dirX[6]= {8, 4, -4, -8, -4, 4};
int dirY[6]= {0, -8, -8, 0, 8, 8};

///根据坐标得到编号
int node(int x,int y,struct PNG p){
    if(x >= p.width || y >= p.height || x <= 1 || y <= 1) return -1;

    int clos = (p.width - 1) / 8;
    int Y = (y + 1) / 8;
    if(Y % 2 == 1){
        return clos * ( Y - 1 ) - Y / 2 + ( x + 3 ) /8  - 1;
    } else {
        return clos *( Y - 1 ) - ( Y / 2 - 1) + ( x + 3 ) /8  - 1 ;
    } 
}

int closest(int dist[], int final[])
{
    int flag = -1, temp = INF;
    for(int i = 0; i < n; i++)
    {
        if(!final[i] && dist[i] < temp)
        {
            temp = dist[i];
            flag = i;
        }
    }
    return flag;
}

void parse(struct PNG *p,State *S) {
	int Y = (p.height - 5) / 8;
    int X = (p.width - 1) / 8;
    n = X * Y - Y / 2;
  int x = 5;
  int y = 7;
  int L = 1;
  for(y = 7; y < p->height; y += 8){

    for(; x < p->width; x += 8){
        struct PXL *pxl = get_PXL(p, x, y);
            if (pxl) {
                int r = pxl->red; 
                int g = pxl->green;
                int b = pxl->blue;   
                S[node(x,y,p)].industry = 255 * 255 * 3 - r * r - g * g - b * b;
            }
         for (int i = 0; i < 6; i++) {
            int nx = x + dirX[i];
            int ny = y + dirY[i];
            if(node(nx,ny,p) >= 0){
                State *conv = &S[node(x,y,p)];
                while(conv->next != NULL){
                    conv = conv->next;
                }
                conv->next = &S[node(nx,ny,p)];
            }
    }
    
  }
  L++;
x = (L % 2) ? 5: 9;
}

}

int solve1(State *s, int x) {
    int final[n] = {0};
    if(x != -1)
    {
    	final[x] = 1;
	}
    int dist[n] = {INF};
    dist[0] = 0;
    for(int i = 0; i < n; i++)
    {
    	u = closest(dist, final);
    	final[u] = 1;
    	for(int j = 0; j < n; j++)
    	{
    		if(dist[u] + (s + i) -> industry < dist[i])
    		{
    			dist[i] = dist[u] + (s + i) -> industry;
			}
		}
	}
    return dist[n - 1];
}

int solve2(State *s) {
    int final[n] = {0};
    int dist[n] = {INF};
    int path[n] = {0};
    int parent[n] = {-1};
    int u, index = 0;
    parent[0] = 0;
    dist[0] = 0;
    for(int i = 0; i < n; i++)
    {
    	u = closest(dist, final);
    	final[u] = 1;
    	for(int j = 0; j < n; j++)
    	{
    		if(dist[u] + (s + i) -> industry < dist[i])
    		{
    			dist[i] = dist[u] + (s + i) -> industry;
    			parent[i] = u;
			}
		}
	}
	int temp = n - 1;
	while(parent[temp] != temp)
	{
		path[index++] = parent[temp];
		temp = parent[temp];
	}
	int result = INF, t_result;
	while(index > 0)
	{
		index--;
		t_result = solve1(s,index);
		if(t_result < result)
		result = t_result;
	}
    return result;
}
