#ifndef TRAPQ_H
#define TRAPQ_H

#include "list.h" // list_node

struct coord {
    union {
        struct {
            double x, y, z;
        };
        double axis[3];
    };
};

struct move {           //T形运动一段参数
    double print_time;   //开始 打印时间    
    double move_t;      //运动时间
    double start_v;      //运动起始速度
    double half_accel;         //  加速度一半
    struct coord start_pos; //开始运动时位置 
    struct coord axes_r;     // 运动方向单位向量   运动分向量占移动距离的百分比

    struct list_node node;
};

struct trapq {
    struct list_head moves;
};

struct move *move_alloc(void);
void trapq_append(struct trapq *tq, double print_time
                  , double accel_t, double cruise_t, double decel_t
                  , double start_pos_x, double start_pos_y, double start_pos_z
                  , double axes_r_x, double axes_r_y, double axes_r_z
                  , double start_v, double cruise_v, double accel);
double move_get_distance(struct move *m, double move_time);
struct coord move_get_coord(struct move *m, double move_time);
struct trapq *trapq_alloc(void);
void trapq_free(struct trapq *tq);
void trapq_check_sentinels(struct trapq *tq);
void trapq_add_move(struct trapq *tq, struct move *m);
void trapq_free_moves(struct trapq *tq, double print_time);

#endif // trapq.h
