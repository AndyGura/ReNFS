/* For tools/cars_emu.py: the parts of a car record the drawing reads that the check's
   copy of race_car leaves in its "rest", set from the original's. */
#include "car.h"

void cars_test_tyres(race_car *c, int speed, int steer, int tyres_front, int tyres_rear,
                     int shown_front, int shown_rear)
{
    c->speed = speed;
    c->steer = steer;
    c->tyres[0] = (unsigned char)tyres_front;
    c->tyres[1] = (unsigned char)tyres_rear;
    c->tyre_shown[0] = shown_front;
    c->tyre_shown[1] = shown_rear;
}
