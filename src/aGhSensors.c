#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>
#include <poll.h>
#include <unistd.h>

#include "aGhSensors.h"

#include RF_FRAME_SIZE (7u)

static unsigned dtAbs(unsigned t1, unsigned t2);

/* ------------------function implementations----------------------------- */
static unsigned dtAbs(unsigned t1, unsigned t2)
{
    if(t2 >= t1)
    {/* no overflow */
        return (t2 - t1);
    }
    else
    {/* overflow */
        t1 = 0xFFFFFFFFu - t1;
        return t1 + t2;
    }
}

unsigned int get1wTemperature(char *devicePath, int *tempRawVal)
{
    FILE *fp = NULL;
    char *line = NULL, *value;
    size_t len = 0;

    if((fp = fopen(devicePath, "r")) == NULL)
    {
        return NOK;
    }
    if (getline(&line, &len, fp) != -1)
    {/* succesfully read the first line, check for YES */
        if(strstr(line, "YES") != NULL)
        {/* read second line: the temperature */
            if (getline(&line, &len, fp) != -1)
            {/* retrieve temperature value */
                value = strchr(line, 't') ;
                if(value)
                {
                    *tempRawVal = strtol(value + 2, NULL, 10);
                    fclose(fp);
                    free(line);
                    return OK;
                }
            }
        }
    }
    fclose(fp);
    free(line);
    return NOK;
}

unsigned int getDht22Values(unsigned char pin, t_s_dht_values *dhtValues)
{
    unsigned char buffer[5] = {0},
                          byteInd = 0, bitInd = 0, feCnt = 3, state, laststate;
    unsigned t1, t2, dt;
    
    /* start seq */
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    usleep(700);
    digitalWrite(pin, HIGH);
    
    /* read sensor data */
    pinMode(pin, INPUT); 
    laststate = digitalRead(pin);
    if(laststate == LOW) feCnt--;
    t1 = (unsigned)micros();
    while(1)
    {
        state = digitalRead(pin);
        t2 = (unsigned)micros();
        dt = dtAbs(t1, t2);
        if(dt > 200) break;     /* timeout */
        if(state == laststate) continue;
        laststate = state;
        if(state == HIGH) continue;
        /* falling edge, process it */
        if(feCnt) feCnt--;
		if(feCnt)
        {
            t1 = (unsigned)micros();
            continue;
        }
        t1 = t2;
        if (dt > 120)
		{
			buffer[byteInd] |= 0x80u >> bitInd;
		}
        bitInd++;
        if(bitInd == 8)
        {
            bitInd = 0;
            byteInd++;
            if(byteInd == 5)
            {/* all read */
                break;
            }
        }
    }
    /* check CRC */
    if((((unsigned char)(buffer[0] + buffer[1] + buffer[2] + buffer[3]) != buffer[4])) || (buffer[4] == 0))
    {
        return NOK;
    }

    dhtValues->humidity = (buffer[0] * 256 + buffer[1]) / 10.0;
    dhtValues->temperature = ((buffer[2] & 0x7F) * 256 + buffer[3]) / 10.0;
    if(buffer[2] & 0x80)
    {
        dhtValues->temperature *= (-1);
    }
    return OK;
}

unsigned int getPwmFlow(unsigned char pin, float *flowRate)
{
    int delta = 500; //ms
    unsigned int edgeCnt = 0, t0;
    char dummy, fName[64];
    struct pollfd polls;
    int sysFd = -1;
    
    sprintf (fName, "/sys/class/gpio/gpio%d/value", wpiPinToGpio(pin)) ;
    if((sysFd = open (fName, O_RDONLY)) < 0)
    {
        return NOK;
    }
    polls.fd = sysFd;
    polls.events = POLLPRI;
    (void)read(sysFd, &dummy, 1);
    while(delta > 0)
    {
        t0 = millis();
        (void)poll(&polls, 1, delta);
        delta -= dtAbs(t0, millis());
        (void)read(sysFd, &dummy, 1);
        edgeCnt++;
    }
    *flowRate = ((edgeCnt - 1) * 10) / 75.0;
    close(sysFd);
    return OK;
}

unsigned int makeRfLink(int devFd)
{
	/* See if you can get a response to a hello */
	unsigned char dont_give_up = 5;
	static unsigned char hello_str[3] = {0xFF, 0x07, 0x03};

	while(dont_give_up)
	{
		unsigned char hello_response[3];
		int i;

		usleep(250000);

		if(write(devFd, hello_str, sizeof(hello_str)) != sizeof(hello_str))
		{
			return NOK;
		}

		i = read(devFd, hello_response, sizeof(hello_response));
		if(i < 0)
		{
			return NOK;
		}

		if(i != sizeof(hello_response))
		{
			dont_give_up--;
		}
		else if(hello_response[0] == 0xFF && hello_response[1] == 0x6 && hello_response[2] == 0x3)
		{
			break;
		}
		else
		{
			dont_give_up--;
		}
	}

	if(dont_give_up)
	{
		return OK;
	}
	else
	{
		return NOK;
	}
}

unsigned int getRfSwitch(int devFd, unsigned char *switches)
{
	static const unsigned char data_req[RF_FRAME_SIZE] = {0xFF, 0x08, 0x07, 0x00, 0x00, 0x00, 0x00};
	unsigned char buffer[RF_FRAME_SIZE];

	if(write(devFd, data_req, sizeof(data_req)) != sizeof(data_req))
	{
		return NOK;
	}

	*bytesRcvd = read(devFd, buffer, RF_FRAME_SIZE);
	if( (bytesRcvd != RF_FRAME_SIZE) ||
	    (buffer[0] != 0xFF || buffer[1] != 0x6 || buffer[2] != 0x7) )
	{
		return NOK;
	}

	/* valid response */
	*switches = 0x00u;
	switch(buffer[3])
	{
	case 0x12:
		*switches |= RF_SWITCH_TOP_LEFT_MASK;
		break;
	case 0x22:
		*switches |= RF_SWITCH_BOTTOM_LEFT_MASK;
		break;
	case 0x32:
		*switches |= RF_SWITCH_TOP_RIGHT_MASK;
		break;
	case 0xFF:
		break;
	default:
		return NOK;
		break;
	}
    return OK;
}

