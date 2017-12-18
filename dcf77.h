#ifndef DCF_77
#define DCF_77

/* time for bit low */
#define DCF77_BIT_LOW_MIN     60
#define DCF77_BIT_LOW_MAX     140

/* time for bit high */
#define DCF77_BIT_HIGH_MIN    160
#define DCF77_BIT_HIGH_MAX    240

#define DCF77_BIT_IDLE_MIN    (1000 - (DCF77_BIT_HIGH_MAX))
#define DCF77_BIT_IDLE_MAX    (1000 - (DCF77_BIT_LOW_MIN))

/* start of minute */
#define DCF77_SIG_START_MIN   1600
#define DCF77_SIG_START_MAX   2200

/* DFC77 input debounce time */
#define DCF77_DEBOUNCE_TIME   35

/* DCF77 synchronization states */
#define DCF77_SYNC_WAIT       0
#define DCF77_SYNC_FIRST      1
#define DCF77_SYNC_OK         2

/* functions */
void dcf77_1ms_task(void);
uint8 dcf77_newdata(void);

uint8 dcf77_get_status(void);
uint8 dcf77_get_sync(void);

uint16 dcf77_get_year(void);
uint8 dcf77_get_month(void);
uint8 dcf77_get_dow(void);
uint8 dcf77_get_day(void);
uint8 dcf77_get_hrs(void);
uint8 dcf77_get_min(void);



#endif
