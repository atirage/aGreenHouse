#ifndef A_GH_SENSORS

#define A_GH_SENSORS

#include <wiringPi.h>

#define OK      0
#define NOK     1

typedef struct _tag_dht_values{
    float humidity;
    float temperature;
}t_s_dht_values;

extern unsigned int get1wTemperature(char *devicePath, int *tempRawVal);
extern unsigned int getDht22Values(unsigned char pin, t_s_dht_values *dhtValues);
extern unsigned int getPwmFlow(unsigned char pin, float *flowRate);
extern unsigned int getRfSwitch(int devFd, unsigned char pressed);
extern unsigned int makeRfLink(int devFd);

#endif
