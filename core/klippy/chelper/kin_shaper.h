#ifndef KIN_SHAPER
#define KIN_SHAPER

int input_shaper_set_sk(struct stepper_kinematics *sk
                    , struct stepper_kinematics *orig_sk);

int input_shaper_set_shaper_params(struct stepper_kinematics *sk
                               , int shaper_type_x
                               , int shaper_type_y
                               , double shaper_freq_x
                               , double shaper_freq_y
                               , double damping_ratio_x
                               , double damping_ratio_y);                   
double input_shaper_get_step_generation_window(int shaper_type, double shaper_freq
                                        , double damping_ratio);

struct stepper_kinematics *input_shaper_alloc(void);  
#endif