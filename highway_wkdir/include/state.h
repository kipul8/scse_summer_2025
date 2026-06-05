#ifndef STATE_H_
#define STATE_H_
#include "suan_png.h"
#include "pxl.h"


typedef struct state {
	int industry;
	int num;
	struct state *next; 
    // data structure
}State;

// function
void init_State(struct State *s);
void delete_State(struct State *s);
void assign(struct State *a, struct State *b);
void parse(struct State *s, struct PNG *p);
int solve1(struct State *s);
int solve2(struct State *s);

#endif
