#ifndef A_GH_ACTUATORS

#define A_GH_ACTUATORS

#include <stdbool.h>
#include <wiringPi.h>

typedef struct _tagUDPCommand
{
    unsigned int port; /* Port number to which the messages are sent */
    char address[16];   /* Address (IP or domain name) to which the messages are sent */
    char code;
    char param;
    char zone;
}UDPCommand;

void setRelay(unsigned char pin, unsigned char HiLo, unsigned short int supervisionCycle);
bool sendUDPCmd(const UDPCommand *cmd);

#endif
