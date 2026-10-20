/* The cars drawn into the road as it is painted far to near (NOTES §4.10): which road
   rows have a car (0x678a4), each car's place in the view, its turn and its models
   (cars_at_node 0x6837c, 0x68258, 0x67f1c, 0x66d08, 0x66880). */
#ifndef RENFS_CARDRAW_H
#define RENFS_CARDRAW_H

#include "view.h"

#define RACE_CARS_MAX 12
#define CARS_SEEN 35           /* road nodes from the camera's on (0x14e5d0) */
#define CARS_BEHIND 11         /* road nodes back from the camera's (0x14e5c5) */

extern race_car *g_cars[RACE_CARS_MAX];      /* 0x13c24c: every car of the race */
extern int g_ncars;                          /* 0x12da38 */
extern race_car *g_car_order[RACE_CARS_MAX]; /* 0x13c224: the cars on the road, as drawn */
extern int g_car_order_n;                    /* 0x13c220 */
extern int g_player;                         /* 0x12dd58: the player's car */
extern int g_racers;                         /* 0x13c18c: the cars with a mark */
extern int g_race_type;                      /* 0x12da20: CONFIG.DAT +0x6d, 0 time trial
                                                (no markers), 1 head to head, 2 single
                                                race, 3 tournament */
extern int g_field;                          /* 0x12da2c: the cars racing (1, 2 or 8) */
extern int g_humans;                         /* 0x12da30: the players' cars, the first slots */
extern unsigned char g_car_marks[RACE_CARS_MAX]; /* 0x14e5bc: each one's palette entry */
extern int g_no_markers;                     /* 0x12d9db: the racers' markers off (F6) */
extern int g_car_ticket;                     /* 0xea81c or 0xea820: the police's ticket or
                                                warning shown (its lights flash) */
extern unsigned char g_cars_seen[CARS_SEEN];     /* 0x14e5d0 */
extern unsigned char g_cars_behind[CARS_BEHIND]; /* 0x14e5c5 */
extern const race_camera *g_camera;          /* 0x143df4 */
extern const int *g_camera_m;                /* 0x143df8: the matrix being drawn with */

/* the road's plane under the car (0x38548): its half axes along and across it turned by
   yaw (+0x36d, +0x379, 0x384bc / 0x38434) and the point under its middle, their heights
   from the plane through the node's point A, the point B beside it (+0x1c .. twice) and
   the next node's C (0x382f0); the shadow's with the yaw drawn (+0x359), the driving's
   with its own (+0x385) */
void car_road_plane(race_car *c, int yaw, vec3 *centre);

/* the cars on the road as the race's step lists them (0x51f8f): each drawn at its own
   node, with its angles */
void cars_list(void);
/* the road nodes ahead of the camera that have a car, a car near the start of its node
   drawn one node back (0x678a4) */
void cars_seen(int cam_node);
/* the cars at a road node, as the road reaches it (0x6837c; g_road_paint.cars) */
void cars_at_node(int node);
/* the cars on road node `node` in the mirror (0x68430): within ten nodes behind the
   camera's, in the list's order, no shadows, nearer than 0x2100 */
void cars_behind_at(int node);

#endif
