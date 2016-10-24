#ifndef A_GH_ACTUATORS

#define A_GH_ACTUATORS

#include <wiringPi.h>

void setRelay(unsigned char pin, unsigned char HiLo, unsigned short int supervisionCycle);

#endif
