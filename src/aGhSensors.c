#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sched.h>
#include <poll.h>
#include <unistd.h>
#include <syslog.h>

#include "aGhSensors.h"

#define DEBUG

#define RF_FRAME_SIZE (7u)
#define DHT_TYPE (11)

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
    unsigned char buffer[5] = {0}, byteInd = 0, bitInd = 0, feCnt = 3, state, laststate;
    unsigned t1, t2, dt;

    /* start seq */
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
#if (DHT_TYPE == 11)
    usleep(18000);
#else// if (DHT_TYPE == 22)
    usleep(700);
#endif
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
#ifdef DEBUG
        syslog(LOG_INFO, "DhtXX: %d|%d|%d|%d|%d\n",buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
#endif
        return NOK;
    }
#if (DHT_TYPE == 11)
    dhtValues->humidity = buffer[0];
    dhtValues->temperature = buffer[2];
    if( (dhtValues->humidity < 20) || (dhtValues->humidity > 90) ||
        (dhtValues->temperature < 0) || (dhtValues->temperature > 50) )
    {/* out of range values, probably caused by a 2bit error which the checksum cannot detect */
        return NOK;
    }
#else //if (DHT_TYPE == 22)
    dhtValues->humidity = (buffer[0] * 256 + buffer[1]) / 10.0;
    dhtValues->temperature = ((buffer[2] & 0x7F) * 256 + buffer[3]) / 10.0;
    if(buffer[2] & 0x80)
    {
        dhtValues->temperature *= (-1);
    }
    if( (dhtValues->humidity < 0) || (dhtValues->humidity > 100) ||
        (dhtValues->temperature < -40) || (dhtValues->temperature > 80) )
    {/* out of range values, probably caused by a 2bit error which the checksum cannot detect */
        return NOK;
    }
#endif
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

/* function needs to be called cyclically, for one button press the RF watch generates more frames, this needs to be filtered out */
unsigned int getRfWatchValues(int devFd, t_s_rf_watch_values *rfValues)
{
	static const unsigned char data_req[RF_FRAME_SIZE] = {0xFF, 0x08, 0x07, 0x00, 0x00, 0x00, 0x00};
	unsigned char buffer[RF_FRAME_SIZE];
	static unsigned char inhibitBtnPress[3] = {0};

	if(write(devFd, data_req, sizeof(data_req)) != sizeof(data_req))
	{
		return NOK;
	}

	if( (read(devFd, buffer, RF_FRAME_SIZE) != RF_FRAME_SIZE) ||
	    (buffer[0] != 0xFF || buffer[1] != 0x6 || buffer[2] != 0x7) )
	{/* invalid response */
#ifdef DEBUG
		syslog(LOG_ERR, "Valid data: %d %d %d %d %d %d %d \n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6]);
#endif
		return NOK;
	}

	/* valid response */
	rfValues->switches = RF_SWITCH_NONE_MASK;
	rfValues->acc_fresh = FALSE;
	if(0xFF == buffer[3])
	{/* no new data */
		if(inhibitBtnPress[0])
		{
			inhibitBtnPress[0]--;
		}
		if(inhibitBtnPress[1])
		{
			inhibitBtnPress[1]--;
		}
		if(inhibitBtnPress[2])
		{
			inhibitBtnPress[2]--;
		}
	}
	else
	{
		if(buffer[3] & 0x01)
		{/* acc data present */
			rfValues->acc_fresh = TRUE;
			rfValues->acc_x = buffer[5];
			rfValues->acc_y = buffer[4];
			rfValues->acc_z = buffer[6];
		}
		switch(buffer[3])
		{
		case 0x01:
			rfValues->switches = RF_SWITCH_NONE_MASK;
			if(inhibitBtnPress[0])
			{
				inhibitBtnPress[0]--;
			}
			if(inhibitBtnPress[1])
			{
				inhibitBtnPress[1]--;
			}
			if(inhibitBtnPress[2])
			{
				inhibitBtnPress[2]--;
			}
			break;
		case 0x11:
		case 0x12:
			if(inhibitBtnPress[0] == 0)
			{
				rfValues->switches |= RF_SWITCH_TOP_LEFT_MASK;
				inhibitBtnPress[0] = 1;
			}
			break;
		case 0x21:
		case 0x22:
			if(inhibitBtnPress[1] == 0)
			{
				rfValues->switches |= RF_SWITCH_BOTTOM_LEFT_MASK;
				inhibitBtnPress[1] = 2;
			}
			break;
		case 0x31:
		case 0x32:
			if(inhibitBtnPress[2] == 0)
			{
				rfValues->switches |= RF_SWITCH_TOP_RIGHT_MASK;
				inhibitBtnPress[2] = 5;
			}
			break;
		default:
			return NOK;
			break;
		}

	}
	return OK;
}

