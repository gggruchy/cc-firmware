#ifndef AW_DSP_H
#define AW_DSP_H
#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        DSP_STATE_UNKNOWN,
        DSP_STATE_RUNNING,
        DSP_STATE_OFFLINE,
    };

    int dsp_start(void);
    int dsp_stop(void);
    int dsp_restart(void);
    int dsp_get_state(void);
    

#ifdef __cplusplus
}
#endif
#endif