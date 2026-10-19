/* The race's cars (NOTES §4.6). */
#include "car.h"
#include "track.h"

int lane_width(int n, int half)
{
    /* row n of the table holds i / n in 8.8 for i < 40; only rows 1..3 are made (row 0
       is cleared, the rows past 3 are other variables of the original) */
    if (n < 1 || n > 3 || half < 0 || half >= 40)
        return 0;
    return (short)((unsigned)((half / n << 16) + (half % n << 16) / n) >> 8);
}

void car_grid_place(race_car *c, int grid, int racers, int kind)
{
    /* the lanes' width at node 10, the racers two abreast from node 32 back */
    const unsigned char *n10 = node_ptr(10), *n20 = node_ptr(20);
    int w = lane_width(n10[4] & 15, n10[1] >> 3) << 8;

    c->node = 32 - (racers > 2 ? grid * 2 : grid / 2 * 4);
    c->pos.x = -w / 2 + grid % 2 * w;
    if (kind > 2) {
        c->pos.x += node_i32(c->node, 8);
        c->yaw = (int)((unsigned)(node_i32(c->node, 0x16) >> 16) << 10);
    }
    /* two lanes on the left and one on the right at node 20: the even slots one lane
       further left */
    if (n20[4] >> 4 == 2 && (n20[4] & 15) == 1 && grid % 2 == 0)
        c->pos.x -= w;
    c->dist = 0;
    c->pos.z = node_i32(c->node, 0x10);
}
