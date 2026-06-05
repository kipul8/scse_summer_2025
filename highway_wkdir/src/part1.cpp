#include "suan_png.h"
#include "pxl.h"
#include "state.h"
#include <string>
#include <iostream>

int solve_algo();

int main() {
    solve_algo();
    return 0;
}


int solve_algo() {
    struct PNG *png = new PNG();
    init_PNG(png);
    load(png, "pic/test1.png");
    State *state = new State[5000];
    parse(state, png);
    std::cout << solve1(state, -1) << std::endl;
    std::cout << solve2(state) << std::endl;
    save(png, "pic/test1.png");
    delete_PNG(png);
    delete[5000] state;
    delete png;
    return -1;
}
