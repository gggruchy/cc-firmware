#ifndef HL_BEEP_H
#define HL_BEEP_H
#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
	typedef enum
	{
		BEEPER_SHORT_YET = 0,
		BEEPER_LONG_YET = 1,
		BEEPER_YET_ALWAYS = 2,
		BEEPER_MUSIC = 3,
		BEEPER_STOP = 4
	} beeper_status_t;

	int beepinit(void);
	int beep_on(bool argc);
	void Beep_timer_handler();
	void set_beeper_status(beeper_status_t status, int option_time_10ms, bool force = false);
	void set_beeper_status_repeat(beeper_status_t status, int option_time_10ms, int times, bool force = false);
}
#endif
#endif