#ifndef A_GH_SENSORS

#define A_GH_SENSORS

#include <wiringPi.h>

#define OK      0
#define NOK     1

#ifndef FALSE
#	define FALSE   0
#endif
#ifndef TRUE
#	define TRUE    1
#endif

#define RF_SWITCH_NONE_MASK        (0x00u)
#define RF_SWITCH_TOP_LEFT_MASK    (0x01u)
#define RF_SWITCH_BOTTOM_LEFT_MASK (0x02u)
#define RF_SWITCH_TOP_RIGHT_MASK   (0x04u)


typedef struct _tag_dht_values{
    float humidity;
    float temperature;
}t_s_dht_values;

typedef struct _tag_rf_watch_values{
	unsigned char acc_x;
	unsigned char acc_y;
	unsigned char acc_z;
	unsigned char acc_fresh;
	unsigned char switches;
}t_s_rf_watch_values;

extern unsigned int get1wTemperature(char *devicePath, int *tempRawVal);
extern unsigned int getDht22Values(unsigned char pin, t_s_dht_values *dhtValues);
extern unsigned int getPwmFlow(unsigned char pin, float *flowRate);
extern unsigned int getRfWatchValues(int devFd, t_s_rf_watch_values *rfValues);
extern unsigned int makeRfLink(int devFd);

#endif
