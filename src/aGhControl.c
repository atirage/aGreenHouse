#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>

/* user includes */
#include "aGhSensors.h"
#include "aGhActuators.h"

#define DEBUG

/* Application error codes */
#define ERR_APP_SQL     1
#define ERR_APP_HEAP    2
#define ERR_APP_THREAD  3
#define ERR_APP_BCM2835 4

#define LOCATION_SIZE 20
#define ACCESSED_BY_SIZE 13

typedef enum {
    _1W_TEMP = 0,
    PWM_FLOW,
    DHT_HMD_TEMP,
    RF_SWITCH,
    SENS_UNKNOWN,
    NR_SENS_TYPE
}t_e_sensor_type;

typedef enum {
    ON_OFF_TIME = 0,
    ON_OFF_FDB,
    ACT_UNKNOWN,
    NR_ACT_TYPE
}t_e_actuator_type;

typedef enum {
    U_NONE = 0,
    U_C,
    U_PERCENT,
    U_L_PER_MIN,
    NR_UNITS
}t_e_unit;

typedef enum {
    CMD_RELEASE = 0,
    CMD_ACTIVATE,
    CMD_DEACTIVATE,
    CMD_CALCULATE,
    CMD_UPDATE_FNC
}t_e_ext_cmd;

typedef struct {
    pthread_t WorkerThread;                        /* activation thread */
    pthread_mutex_t cmd_mutex;
    pthread_cond_t cmd_cv;
    unsigned short int DbId;                       /* Id from DB */
    char Location[LOCATION_SIZE];                  /* physical location of sensor */
    char AccessedBy[ACCESSED_BY_SIZE];             /* connection to the raspberry, for ex. file, gpio, etc */
    t_e_actuator_type Type;                        /* type of sensor, defines how the actuator will be handled */
    t_e_unit FdbUnit;							   /* unit of feedback, if any */
    void *ctrlFnc;                                 /* assigned control func */
    t_e_ext_cmd extCmd;                            /* external cmd */
    float paramCmd;                                /* command parameter */
    unsigned short int supervisionCycle;           /* if actuator output is not refreshed with at least this period a reset is triggered */
}t_s_actuator;

typedef struct tag_s_act_list{
    t_s_actuator *ptrAct;
    struct tag_s_act_list *nextAct;
}t_s_act_list;

typedef struct {
    pthread_t WorkerThread;                         /* acquisition thread */
    unsigned short int DbId;                        /* Id from DB */
    char Location[LOCATION_SIZE];                   /* physical location of sensor */
    char AccessedBy[ACCESSED_BY_SIZE];              /* connection to the raspberry, for ex. file, gpio, etc */
    t_e_sensor_type Type;                           /* type of sensor, defines how the sensor will be handled */
    unsigned int SampleTime;                        /* desired refresh rate of sensor data */
    t_s_act_list *headAct;
}t_s_sensor;

typedef struct {
    unsigned int resolution;
    unsigned int elemNr;
    unsigned char *elements;
}t_s_time_fct;

typedef struct {
    float threshold;
    float hysteresis;
}t_s_temp_hyst_fct;

typedef void* (*t_s_thread_func)(void*);

/* globals */
sqlite3 *db;
t_s_sensor *sensors = NULL;
t_s_actuator *actuators = NULL;
unsigned int nrSens, nrAct;

 /* SQL functions  */
static int getAllSensors_CB(void *countRow, int nCol, char **valCol, char **nameCol);
static int getSensorCount_CB(void *notUsed, int nCol, char **valCol, char **nameCol);
static int getAllActuators_CB(void *countRow, int nCol, char **valCol, char **nameCol);
static int getActCount_CB(void *notUsed, int nCol, char **valCol, char **nameCol);
static int insertSensorValue(const t_s_sensor *self, float Value, t_e_unit Unit, unsigned Timestamp);
static int insertActuatorState(const t_s_actuator *self, float State, t_e_unit Unit);
static int updateActuatorCtrlFnc_CB(void *actPtr, int nCol, char **valCol, char **nameCol);
static int getAlert_CB(void *Value, int nCol, char **valCol, char **nameCol);

/* threads */
void *read1wTempSensor(void *self);
void *readDhtSensor(void *self);
void *readPwmFlowSensor(void *self);
void *controlTimeRelay(void * self);
void *controlHysteresis(void * self);
void *readRfSwitch(void *self);
void *dummyThread(void *self);

/* misc */
static void cleanup(void);
static int getWPiPin(const char *AccessedBy, unsigned char *pin);
static unsigned short int getSensorInd(unsigned short int dbId);
static unsigned short int getActuatorInd(unsigned short int dbId);
static void issueActCmd(t_s_act_list *actList, float *paramList);
static void appendUnit(char *unitString, t_e_unit unit);
static void checkAlert(unsigned short int dbSensId, float value, t_e_unit unit);

/* map sensor type to thread function */
const t_s_thread_func SensThreadCfg[NR_SENS_TYPE] = {
        read1wTempSensor,   //_1W_TEMP
        readPwmFlowSensor,  //PWM_FLOW
		readDhtSensor,      //DHT_HMD_TEMP
		readRfSwitch,       //RF_SWITCH
        NULL                //SENS_UNKNOWN
};
/* map actuator type to thread function */
const t_s_thread_func ActThreadCfg[NR_ACT_TYPE] = {
        controlTimeRelay,    //ON_OFF_TIME
        controlHysteresis,   //ON_OFF_FDB
        NULL                 //ACT_UNKNOWN
};

/* ------------------function implementations----------------------------- */

static void cleanup(void)
{
    unsigned int i;
    sqlite3_close(db);
    if(actuators != NULL)
    {
        for(i=0; i < nrAct; i++)
        {
            if(actuators[i].ctrlFnc) {free(actuators[i].ctrlFnc);}
        }
        free(actuators);
    }
    if(sensors != NULL)
    {
        for(i=0; i < nrSens; i++)
        {
            if(sensors[i].headAct) 
            {
                //todo: free list
            }
        }
        free(sensors);
    }
}

static int getWPiPin(const char *AccessedBy, unsigned char *pin)
{
    int ret = -1;
    char *ptr = NULL;

    if(strstr(AccessedBy, "GPIO_"))
    {/* it is indeed GPIO */
        if(ptr = strstr(AccessedBy, "P1_"))
        {/* P1 header */
            ret = 0;
            switch(strtol(ptr + 3, NULL, 10))
            {
            case 3:
                *pin = 8;
                break;
            case 5:
                *pin = 9;
                break;
            case 7:
                *pin = 7;
                break;
            case 8:
                *pin = 15;
                break;
            case 10:
                *pin = 16;
                break;
            case 11:
                *pin = 0;
                break;
            case 12:
                *pin = 1;
                break;
            case 13:
                *pin = 2;
                break;
            case 15:
                *pin = 3;
                break;
            case 16:
                *pin = 4;
                break;
            case 18:
                *pin = 5;
                break;
            case 19:
                *pin = 12;
                break;
            case 21:
                *pin = 13;
                break;
            case 22:
                *pin = 6;
                break;
            case 23:
                *pin = 14;
                break;
            case 24:
                *pin = 10;
                break;
            case 26:
                *pin = 11;
                break;
            default:
                ret = -1;
                break;
            }
        }
        /*else if()
        {}
        else
        {}*/
    }
    return ret;
}

static unsigned short int getSensorInd(unsigned short int dbId)
{
    unsigned int i = 0;
    for(; i < nrSens; i++)
    {
        if(sensors[i].DbId == dbId)
        {
            return i;
        }
    }
    return nrSens;
}

static unsigned short int getActuatorInd(unsigned short int dbId)
{
    unsigned int i = 0;
    for(; i < nrAct; i++)
    {
        if(actuators[i].DbId == dbId)
        {
            return i;
        }
    }
    return nrAct;
}

static void issueActCmd(t_s_act_list *actList, float *paramList)
{
    do
    {
        pthread_mutex_lock(&(actList->ptrAct->cmd_mutex));
        switch(actList->ptrAct->Type)
        {
        case ON_OFF_FDB:
            actList->ptrAct->extCmd = CMD_CALCULATE;
        	switch(actList->ptrAct->FdbUnit)
        	{
        	case U_C:
        		actList->ptrAct->paramCmd = paramList[U_C];
        		break;
        	case U_L_PER_MIN:
        		actList->ptrAct->paramCmd = paramList[U_L_PER_MIN];
        		break;
        	case U_PERCENT:
        		actList->ptrAct->paramCmd = paramList[U_PERCENT];
        		break;
        	case U_NONE:
        		actList->ptrAct->paramCmd = paramList[U_NONE];
        		break;
        	default:
        		actList->ptrAct->paramCmd = 0;
        		break;
        	}
            break;
        default:
        	actList->ptrAct->extCmd = CMD_RELEASE;
            actList->ptrAct->paramCmd = 0;
            break;
        }

        pthread_cond_signal(&(actList->ptrAct->cmd_cv));
        pthread_mutex_unlock(&(actList->ptrAct->cmd_mutex));
        actList = actList->nextAct;
    }while(actList);
}

static void checkAlert(unsigned short int dbSensId, float value, t_e_unit unit)
{
    char queryString[65], unitString[10] = {0};
    /* get the alert */
    appendUnit(unitString, unit);
    snprintf(queryString, 65, "select * from Alerts where SensId = %d and Unit = '%s';", dbSensId, unitString);
    if( sqlite3_exec(db, queryString, getAlert_CB, (void *)&value, NULL) != SQLITE_OK )
    {
        syslog(LOG_ERR, "SQL error when querying for alert for Sensor %d\n", dbSensId);
    }
}

static void appendUnit(char *unitString, t_e_unit unit)
{
    switch(unit)
    {
    case U_NONE:
        strcat(unitString, "NONE");
        break;
    case U_C:
        strcat(unitString, "oC");
        break;
    case U_PERCENT:
        strcat(unitString, "%");
        break;
    case U_L_PER_MIN:
        strcat(unitString, "L/min");
        break;
    default:
        break;
    }
}

static int updateActuatorCtrlFnc_CB(void *actPtr, int nCol, char **valCol, char **nameCol)
{
    t_s_time_fct *ptrFct = NULL;
    char *delim = NULL;
    unsigned int k, tmp = 0;
    
    switch(((t_s_actuator *)actPtr)->Type)
    {
    case ON_OFF_TIME:/* 1010..1010|1800 */
        if(NULL == ((t_s_actuator *)actPtr)->ctrlFnc)
        {
            if((((t_s_actuator *)actPtr)->ctrlFnc = malloc(sizeof(t_s_time_fct))) == NULL)
            {
                return 1;
            }
            ((t_s_time_fct *)(((t_s_actuator *)actPtr)->ctrlFnc))->elements = NULL;
        }
        ptrFct = (t_s_time_fct *)(((t_s_actuator *)actPtr)->ctrlFnc);
        if((delim = strchr(valCol[0], '|')) == NULL)
        {
            return 1;
        }
        ptrFct->resolution = strtol(delim + 1, NULL, 10);
        delim[0] = 0;
        ptrFct->elemNr = strlen(valCol[0]);
        if(ptrFct->elements != NULL)
        {
            free(ptrFct->elements);
        }
        if((ptrFct->elements = malloc((ptrFct->elemNr + 7) / 8)) == NULL)
        {
            return 1;
        }
        for(k = 0; k < ptrFct->elemNr; k++)
        {
            if((k / 8) && (k % 8 == 0))
            {
                (ptrFct->elements)[k / 8 - 1] = tmp;
                tmp = 0;
            }
            if(valCol[0][k] == '1')
            {
                tmp |= (unsigned char)(1 << (k % 8));
            }
        }
        (ptrFct->elements)[(k - 1) / 8] = tmp;
        break;
    case ON_OFF_FDB:/* 23.5|1.5 */
        if(NULL == ((t_s_actuator *)actPtr)->ctrlFnc)
        {
            if((((t_s_actuator *)actPtr)->ctrlFnc = malloc(sizeof(t_s_temp_hyst_fct))) == NULL)
            {
                return 1;
            }
        }
        if((delim = strchr(valCol[0], '|')) == NULL)
        {
            return 1;
        }
        delim[0] = 0;
        ((t_s_temp_hyst_fct*)(((t_s_actuator *)actPtr)->ctrlFnc))->threshold = atof(valCol[0]);
        ((t_s_temp_hyst_fct*)(((t_s_actuator *)actPtr)->ctrlFnc))->hysteresis = atof(delim + 1);
        break;
    default:
        ((t_s_actuator *)actPtr)->ctrlFnc = NULL;
        break;
    }
    return 0;
}

static int getAllSensors_CB(void *countRow, int nCol, char **valCol, char **nameCol)
{
    unsigned int i = *((unsigned int *)countRow), j;

    for(j=0; j<nCol; j++)
    {
        syslog(LOG_INFO, "%s = %s\n", nameCol[j], valCol[j] ? valCol[j] : "NULL");
    }

    /* 0: DbId - cannot be NULL */
    sensors[i].DbId = strtol(valCol[0], NULL, 10);

    /* 1: Location - can be NULL */
    if(valCol[1])
    {
        snprintf(sensors[i].Location, LOCATION_SIZE, "%s", (const char*)(valCol[1]));
    }
    else
    {
        sensors[i].Location[0] = 0;
    }
    
    /* 2: Access - cannot be NULL */
    snprintf(sensors[i].AccessedBy, ACCESSED_BY_SIZE, "%s", (const char*)(valCol[2]));
    
    /* 3: Type - cannot be NULL */
    if(strcmp(valCol[3], "1wTemp") == 0)
    {
        sensors[i].Type = _1W_TEMP;
    }
    else if(strcmp(valCol[3], "Dht22") == 0)
    {
        sensors[i].Type = DHT_HMD_TEMP;
    }
    else if(strcmp(valCol[3], "PwmFlow") == 0)
    {
        sensors[i].Type = PWM_FLOW;
    }
    else if(strcmp(valCol[3], "RfSwitch") == 0)
    {
    	sensors[i].Type = RF_SWITCH;
	}
    else
    {
        sensors[i].Type = SENS_UNKNOWN;
    }
    /* 4: Sample Time - cannot be NULL */
    sensors[i].SampleTime = strtol(valCol[4], NULL, 10);
    
    sensors[i].headAct = NULL;
    (*((unsigned int *)countRow))++;
    return 0;
}

static int getSensorCount_CB(void *notUsed, int nCol, char **valCol, char **nameCol)
{
    nrSens = strtol(valCol[nCol - 1], NULL, 10);
    return 0;
}

static int getAllActuators_CB(void *countRow, int nCol, char **valCol, char **nameCol)
{
    unsigned int i = *((unsigned int *)countRow), j;
    unsigned char k, tmp = 0;
    char *delim = NULL;
    t_s_time_fct *ptrFct = NULL;
    t_s_act_list *actList = NULL;

    for(j=0; j<nCol; j++)
    {
        syslog(LOG_INFO, "%s = %s\n", nameCol[j], valCol[j] ? valCol[j] : "NULL");
    }

    /* 0: DbId - cannot be NULL */
    actuators[i].DbId = strtol(valCol[0], NULL, 10);
    /* 1: Location - can be NULL */
    if(valCol[1])
    {
        snprintf(actuators[i].Location, LOCATION_SIZE, "%s", (const char*)(valCol[1]));
    }
    else
    {
        actuators[i].Location[0] = 0;
    }
    /* 2: Access - cannot be NULL */
    snprintf(actuators[i].AccessedBy, ACCESSED_BY_SIZE, "%s", (const char*)(valCol[2]));
    /* 3: Type - cannot be NULL */
    if(strcmp(valCol[3], "ON_OFF_TIME") == 0)
    {
        actuators[i].Type = ON_OFF_TIME;
        actuators[i].FdbUnit = U_NONE;
    }
    else if(strcmp(valCol[3], "ON_OFF_FDB_T") == 0)
    {
        actuators[i].Type = ON_OFF_FDB;
        actuators[i].FdbUnit = U_C;
    }
    else if(strcmp(valCol[3], "ON_OFF_FDB_H") == 0)
    {
        actuators[i].Type = ON_OFF_FDB;
        actuators[i].FdbUnit = U_PERCENT;
    }
    else if(strcmp(valCol[3], "ON_OFF_FDB_DIG") == 0)
    {
        actuators[i].Type = ON_OFF_FDB;
        actuators[i].FdbUnit = U_NONE;
    }
    else
    {
        actuators[i].Type = ACT_UNKNOWN;
        actuators[i].FdbUnit = U_NONE;
    }
    /* 4: CtrlFunc - cannot be NULL*/
    switch(actuators[i].Type)
    {
    case ON_OFF_TIME:/* 1010..1010|1800 */
        if((actuators[i].ctrlFnc = malloc(sizeof(t_s_time_fct))) == NULL)
        {
            return 1;
        }
        ptrFct = (t_s_time_fct *)actuators[i].ctrlFnc;
        if((delim = strchr(valCol[4], '|')) == NULL)
        {
            return 1;
        }
        ptrFct->resolution = strtol(delim + 1, NULL, 10);
        delim[0] = 0;
        ptrFct->elemNr = strlen(valCol[4]);
        if((ptrFct->elements = malloc((ptrFct->elemNr+ 7) / 8)) == NULL)
        {
            return 1;
        }
        for(k = 0; k < ptrFct->elemNr; k++)
        {
            if((k / 8) && (k % 8 == 0))
            {
                (ptrFct->elements)[k / 8 - 1] = tmp;
                tmp = 0;
            }
            if(valCol[4][k] == '1')
            {
                tmp |= (unsigned char)(1 << (k % 8));
            }
        }
        (ptrFct->elements)[(k - 1) / 8] = tmp;
        actuators[i].supervisionCycle = ptrFct->resolution * 2u;
#ifdef DEBUG
        for(k = 0; k < ((ptrFct->elemNr+ 7) / 8); k++)
        {
            syslog(LOG_INFO, "time relay:%d\n", ptrFct->elements[k]);
        }
#endif
        break;
    case ON_OFF_FDB:/* 23.5|1.5 */
        if((actuators[i].ctrlFnc = malloc(sizeof(t_s_temp_hyst_fct))) == NULL)
        {
            return 1;
        }
        if((delim = strchr(valCol[4], '|')) == NULL)
        {
            return 1;
        }
        delim[0] = 0;
        ((t_s_temp_hyst_fct*)(actuators[i].ctrlFnc))->threshold = atof(valCol[4]);
        ((t_s_temp_hyst_fct*)(actuators[i].ctrlFnc))->hysteresis = atof(delim + 1);
        actuators[i].supervisionCycle = 3600u;
        break;
    default:
        actuators[i].ctrlFnc = NULL;
        actuators[i].supervisionCycle = 0xffffu;
        break;
    }

     /* 5: SensInd - can be NULL*/
     if(valCol[5] != NULL)
     {
        //find in sensors array
        j = getSensorInd(strtol(valCol[5], NULL, 10));
        if(j < nrSens) //found
        {
            if(sensors[j].headAct == NULL)
            {
                sensors[j].headAct = malloc(sizeof(t_s_act_list));
                sensors[j].headAct->ptrAct = &actuators[i];
                sensors[j].headAct->nextAct = NULL;
            }
            else
            {
                actList = sensors[j].headAct;
                while(actList->nextAct != NULL)
                {
                    actList = actList->nextAct;
                }
                actList->nextAct = malloc(sizeof(t_s_act_list));
                actList->nextAct->ptrAct = &actuators[i];
                actList->nextAct->nextAct = NULL;
            }
        }
     }
     
    (*((unsigned int *)countRow))++;
    return 0;
}

static int getActCount_CB(void *notUsed, int nCol, char **valCol, char **nameCol)
{
    nrAct = strtol(valCol[nCol - 1], NULL, 10);
    return 0;
}

static int insertSensorValue(const t_s_sensor *self, float Value, t_e_unit Unit, unsigned Timestamp)
{
    char queryString[120];
    char *zErrMsg = 0;
    int rc;
    /* prepare query string */
    strcpy(queryString, "insert into Measurements (SensorInd, Value, Unit, Timestamp) VALUES ('");
    snprintf(queryString + strlen(queryString), 4, "%d", self->DbId);
    strcat(queryString, "','");

    snprintf(queryString + strlen(queryString), 7, "%.3f", Value);
    strcat(queryString, "','");
    appendUnit(queryString, Unit);
    strcat(queryString, "','");

    snprintf(queryString + strlen(queryString), 11, "%d", Timestamp);
    strcat(queryString, "');");

    /* insert into database */
    rc = sqlite3_exec(db, queryString, NULL, 0, &zErrMsg);
    if(rc != SQLITE_OK)
    {
        syslog(LOG_ERR, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}

static int insertActuatorState(const t_s_actuator *self, float State, t_e_unit Unit)
{
    char queryString[120];
    char *zErrMsg = 0;
    int rc;
    /* prepare query string */
    strcpy(queryString, "insert into Activations (ActuatorInd, Value, Unit, Timestamp) VALUES ('");
    snprintf(queryString + strlen(queryString), 4, "%d", self->DbId);
    strcat(queryString, "','");

    snprintf(queryString + strlen(queryString), 7, "%.3f", State);
    strcat(queryString, "','");
    appendUnit(queryString, Unit);
    strcat(queryString, "','");

    snprintf(queryString + strlen(queryString), 11, "%d", (unsigned int)time(NULL));
    strcat(queryString, "');");

    /* insert into database */
    rc = sqlite3_exec(db, queryString, NULL, 0, &zErrMsg);
    if(rc != SQLITE_OK)
    {
        syslog(LOG_ERR, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}

static int getAlert_CB(void *Value, int nCol, char **valCol, char **nameCol)
{
    pid_t pId = -1;
    float val =  *((float*)Value);
    char mailCmd[150];   
    if((atof(valCol[2]) > val) || (atof(valCol[3]) < val))
    {
        snprintf(mailCmd, 150, "echo Sensor %s value: %f %s out of range [%s, %s]! | mail -s 'Sensor Alert' %s", valCol[1], val, valCol[4], valCol[2], valCol[3], valCol[5]);
        pid_t pId = fork();
        if (pId == 0)
        {// child
        #ifdef DEBUG
            fprintf(stdout, mailCmd);  
            fprintf(stdout, "\n\n\n");
        #endif 
            execl("/bin/sh", "sh", "-c", mailCmd, NULL);
        #ifdef DEBUG
            fprintf(stdout, "Exec error!");  
            fprintf(stdout, "\n\n\n");
        #endif 
        }
    }
    return 0;
}

/*---------- MAIN -------------*/
int main(void)
{
    char *zErrMsg = NULL, str[15], c;
    int rc, cmd_fd, resp_fd;
    unsigned short int cmd, actInd, phase = 0, i = 0, count;
    unsigned int row;
    float param = 0;
    struct stat st = {0};
    
    openlog("root", LOG_ODELAY, LOG_USER);
    
    if(wiringPiSetup () < 0)
    {
        syslog(LOG_ERR, "Init error! Stopping!");
        return ERR_APP_BCM2835;
    }

    if(sqlite3_open("/var/lib/sqlite3/Greenhouse", &db))
    {
        syslog(LOG_ERR, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return ERR_APP_SQL;
    }

    if (stat("/var/lib/aGreenHouse", &st) == -1)
    {
        mkdir("/var/lib/aGreenHouse", 0700);
    }

    /* ------------ prepare sensor acquisition and actuator control ------------- */
    /* get the nr of actuators */
    rc = sqlite3_exec(db, "select count(Ind) as ActuatorCount from Actuators;", getActCount_CB, 0, &zErrMsg);
    if( rc != SQLITE_OK )
    {
        syslog(LOG_ERR, "SQL error when querying for actuator count: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return ERR_APP_SQL;
    }
    /* get the nr of sensors */
    rc = sqlite3_exec(db, "select count(Ind) as SensorCount from Sensors;", getSensorCount_CB, 0, &zErrMsg);
    if( rc != SQLITE_OK )
    {
        syslog(LOG_ERR, "SQL error when querying for sensor count: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return ERR_APP_SQL;
    }

    syslog(LOG_INFO, "Found %d sensors and %d actuators \n", nrSens, nrAct);

    /* allocate memory for sensors & actuators */
    if( ((actuators = malloc(nrAct * sizeof(t_s_actuator))) == NULL) ||
        ((sensors = malloc(nrSens * sizeof(t_s_sensor))) == NULL) )
    {
        syslog(LOG_ERR, "Can't allocate heap space");
        cleanup();
        return ERR_APP_HEAP;
    }

    syslog(LOG_INFO, "-------------Preparing sensors------------- \n");
    /* query for the sensors */
    row = 0;
    rc = sqlite3_exec(db, "select * from Sensors;", getAllSensors_CB, &row, &zErrMsg);
    if( rc != SQLITE_OK )
    {
        syslog(LOG_ERR, "SQL error when querying for sensors: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cleanup();
        return ERR_APP_SQL;
    }
    
    syslog(LOG_INFO, "-------------Preparing actuators------------- \n");
    /* query for the actuators */
    row = 0;
    rc = sqlite3_exec(db, "select * from Actuators;", getAllActuators_CB, &row, &zErrMsg);
    if( rc != SQLITE_OK )
    {
        syslog(LOG_ERR, "SQL error when querying for actuators: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        cleanup();
        return ERR_APP_SQL;
    }
    
    syslog(LOG_INFO, "Starting actuator threads... \n");
    /* start threads according to actuator type */
    rc = FALSE;
    for(i = 0; i < nrAct; i++)
    {
        if( (actuators[i].Type < NR_ACT_TYPE) &&
            (ActThreadCfg[actuators[i].Type] != NULL) )
        {
            pthread_mutex_init(&(actuators[i].cmd_mutex), NULL);
            pthread_cond_init(&(actuators[i].cmd_cv), NULL);
			if(pthread_create(&(actuators[i].WorkerThread), NULL, ActThreadCfg[actuators[i].Type], (void*) (actuators + i)))
            {
                rc = TRUE;
            }
        }
    }
    if(rc)
    {
        syslog(LOG_ERR, "Could not create all threads!");
        cleanup();
        return ERR_APP_THREAD;
    }

    syslog(LOG_INFO, "Starting acquisition threads... \n");
    /* start threads according to sensor type */
    rc = FALSE;
    for(i = 0; i < nrSens; i++)
    {
        if( (sensors[i].Type < NR_SENS_TYPE) &&
            (SensThreadCfg[sensors[i].Type] != NULL) &&
            (sensors[i].SampleTime != 0) )
        {
            if (pthread_create(&(sensors[i].WorkerThread), NULL, SensThreadCfg[sensors[i].Type], (void*) (sensors + i)))
            {
                rc = TRUE;
            }
        }
    }
    if(rc)
    {
        syslog(LOG_ERR, "Could not create all threads!");
        cleanup();
        return ERR_APP_THREAD;
    }

    syslog(LOG_INFO, "Measuring and waiting for commands... \n");
    /* wait for commands from fifo */
    umask(0);
    mknod("/var/lib/aGreenHouse/cmdFIFO", S_IFIFO|0666, 0);//EEXIST
    mknod("/var/lib/aGreenHouse/respFIFO", S_IFIFO|0666, 0);//EEXIST
    cmd_fd = open("/var/lib/aGreenHouse/cmdFIFO", O_RDONLY);//check for error
    resp_fd = open("/var/lib/aGreenHouse/respFIFO", O_WRONLY);//check for error
    i = 0;
    while(1)
    {
        if(!(read(cmd_fd, &c, 1) > 0))
        {
            continue;
        }
        if((c == '\n') || (c == ' '))
        {
            str[i]=0;
            if(phase == 0)
            {
                actInd = strtol(str, NULL, 10);
            }
            else if(phase == 1)
            {
                cmd = strtol(str, NULL, 10);
            }
            else
            {
                //keep str
            }
            i=0;
            phase++;
            if(c == '\n') 
            {
                actInd = getActuatorInd(actInd);
#ifdef DEBUG
                syslog(LOG_INFO, "Command received for %d: %d with ctrl fnc %s \n", actInd, cmd, str);
#endif
                if(actInd < nrAct)
                {
                    if(phase > 2)
                    {/* new ctrl function */
                        pthread_mutex_lock(&(actuators[actInd].cmd_mutex));
                        actuators[actInd].extCmd = CMD_UPDATE_FNC;
                        actuators[actInd].paramCmd = 0;
                        pthread_cond_signal(&(actuators[actInd].cmd_cv));
                        pthread_mutex_unlock(&(actuators[actInd].cmd_mutex));
                    }                   
                    pthread_mutex_lock(&(actuators[actInd].cmd_mutex));
                    actuators[actInd].extCmd = cmd;
                    actuators[actInd].paramCmd = 0;
                    pthread_cond_signal(&(actuators[actInd].cmd_cv));
                    pthread_mutex_unlock(&(actuators[actInd].cmd_mutex));
                }
                phase = 0;
                param = 0;
                write(resp_fd, "OK", 2);
            }
        }
        else
        {
            str[i++]=c;
        }
    }
    /* wait for threads to finish */
    for(i = 0; i < nrSens; i++)
    {
        pthread_join(sensors[i].WorkerThread, NULL);
    }
    for(i = 0; i < nrAct; i++)
    {
        pthread_join(actuators[i].WorkerThread, NULL);
    }
    cleanup();
    return 0;
}

/*----------  Worker Threads ----------*/
void *dummyThread(void *self)
{
    static unsigned timestamp;
	while(1)
    {
        usleep(5000);
    }
    return NULL;
}

void *read1wTempSensor(void *self)
{
    const t_s_sensor *SensPtr = (const t_s_sensor *) self;
    char devPath[46];
    int tempVal;
    float params[NR_UNITS] = {0};
    
    /* get device path from Db */
    strcpy(devPath, "/sys/bus/w1/devices/28-XXXXXXXXXXXX/w1_slave");
    strncpy(devPath + 23, SensPtr->AccessedBy, ACCESSED_BY_SIZE - 1);
    while(1)
    {
        if(get1wTemperature(devPath, &tempVal) != OK)
        {/* unsuccesful, try again later */
            syslog(LOG_ERR, "Error reading sensor: %d!\n", SensPtr->DbId);
            sleep(10);
        }
        else
        {
            /* insert into DB */
            if(insertSensorValue(SensPtr, (float)tempVal/1000.0, U_C, (unsigned)time(NULL)) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting values for sensor: %d !\n", SensPtr->DbId);
            }
            /* issue cmd to actuators if configured */
            if(SensPtr->headAct)
            {
                params[U_C] = (float)tempVal/1000.0;
                issueActCmd(SensPtr->headAct, params);
            }
            /* check alert */
            checkAlert(SensPtr->DbId, (float)tempVal/1000.0, U_C);
            /* go to sleep */
            sleep(SensPtr->SampleTime);
        }
    }
    return NULL;
}

void *readRfSwitch(void *self)
{
    const t_s_sensor *SensPtr = (const t_s_sensor *) self;
    static unsigned char SwitchState = 0;
    char devPath[46];
    unsigned char Switches;
    int devFd;
    struct termios cf;
    float params[NR_UNITS] = {0};

    /* get device path from Db */
    strcpy(devPath, "/dev/XXXXXXX");  //ex. ttyACM0
    strncpy(devPath + 5, SensPtr->AccessedBy, 8);
#ifdef DEBUG
    syslog(LOG_INFO, "Opening %s\n", devPath);
#endif
    /* open, set and connect */
    devFd = open(devPath, O_RDWR);
    if(devFd < 0)
    {
    	syslog(LOG_ERR, "Unable to open '%s'\n", devPath);
    	return NULL;
    }
    if(tcgetattr(devFd, &cf) < 0)
    {
    	syslog(LOG_ERR, "Unable to get termios details\n");
    	return NULL;
    }
    if(cfsetispeed(&cf, B115200) < 0 || cfsetospeed(&cf, B115200) < 0)
    {
    	syslog(LOG_ERR, "Unable to set speed\n");
    	return NULL;
    }

    /* Make it a raw stream and turn off software flow control */
    cfmakeraw(&cf);
    cf.c_iflag &= ~(IXON | IXOFF | IXANY);
    if(tcsetattr(devFd, TCSANOW, &cf) < 0)
    {
    	syslog(LOG_ERR, "Unable to set termios details\n");
    	return NULL;
    }

    if(makeRfLink(devFd) != OK)
    {
    	syslog(LOG_ERR, "Unable to set up RF LINK\n");
    	return NULL;
    }

    /* wait for presses */
#ifdef DEBUG
    syslog(LOG_INFO, "Waiting for button presses... \n");
#endif
    while(1)
    {
        char bytesRcvd;
    	if(getRfSwitch(devFd, &Switches) != OK)
        {/* unsuccessful, try again later */
            syslog(LOG_ERR, "Error reading sensor: %d!\n", SensPtr->DbId);
            sleep(10);
        }
        else
        {
#ifdef DEBUG
        	if(Switches)
        	{
        		syslog(LOG_INFO, "Read switch: %d", Switches);
        	}
#endif
        	if ((Switches & RF_SWITCH_BOTTOM_LEFT_MASK) != 0)
            {/* bottom right button pressed, flip switch state */
            	SwitchState = (~SwitchState) & 0x01u;
            	/* issue cmd to actuators if configured */
            	if(SensPtr->headAct)
            	{
            		params[U_NONE] = (float)SwitchState;
            		issueActCmd(SensPtr->headAct, params);
            	}
            	/* insert into DB */
            	if(insertSensorValue(SensPtr, SwitchState, U_NONE, (unsigned)time(NULL)) != SQLITE_OK)
            	{
            		syslog(LOG_ERR, "SQL error when inserting values for sensor: %d !\n", SensPtr->DbId);
            	}
            }
        	/* go back to sleep */
        	usleep(SensPtr->SampleTime * 1000u);
        }
    }
    return NULL;
}

void *readDhtSensor(void *self)
{
    const t_s_sensor *SensPtr = (const t_s_sensor *) self;
    char fName[48];
	t_s_dht_values DhtValues;
    unsigned char Pin;
    unsigned TimeStamp;
    float params[NR_UNITS] = {0};
    struct sched_param param = {0};
    
    /* increase thread prio */
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if(pthread_setschedparam(SensPtr->WorkerThread, SCHED_FIFO, &param) == -1)
    {
            syslog(LOG_ERR, "Cannot increase Dht reading priority: %d!", errno);
            perror("sched_setscheduler");
            return NULL;
    }
#ifdef DEBUG
    syslog(LOG_INFO, "Increased to priority: %d!", param.sched_priority);
#endif
    
    /* get gpio pin from Db */
    if(getWPiPin(SensPtr->AccessedBy, &Pin) < 0)
    {
         syslog(LOG_ERR, "Invalid Pin for SensorInd: %d \n", SensPtr->DbId);
         return NULL;
    }
    /* delay the start a bit */
    sleep(1); 
    while(1)
    {
        if(getDht22Values(Pin, &DhtValues) != OK)
        {/* unsuccessful, try again later */
            syslog(LOG_ERR, "Error reading sensor: %d!\n", SensPtr->DbId);
            sleep(10);
        }
        else
        {
            if(SensPtr->headAct)
            {
                params[U_C] = DhtValues.temperature;
                params[U_PERCENT] = DhtValues.humidity;
                issueActCmd(SensPtr->headAct, params);
            }
            /* insert into DB */
            TimeStamp = (unsigned)time(NULL);
            if(insertSensorValue(SensPtr, DhtValues.humidity, U_PERCENT, TimeStamp) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting values for sensor: %d !\n", SensPtr->DbId);
            }
            if(insertSensorValue(SensPtr, DhtValues.temperature, U_C, TimeStamp) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting values for sensor: %d !\n", SensPtr->DbId);
            }
            /* check alert */
            checkAlert(SensPtr->DbId, DhtValues.temperature, U_C);
            checkAlert(SensPtr->DbId, DhtValues.humidity, U_PERCENT);
            /* go to sleep */
            sleep(SensPtr->SampleTime);
        }
    }
    return NULL;
}

void *readPwmFlowSensor(void *self)
{
    const t_s_sensor *SensPtr = (const t_s_sensor *) self;
    float flowRate, params[NR_UNITS] = {0};
    unsigned char Pin;

    /* get gpio pin from Db */
    if(getWPiPin(SensPtr->AccessedBy, &Pin) < 0)
    {
         syslog(LOG_ERR, "Invalid Pin for SensorInd: %d \n", SensPtr->DbId);
         return NULL;
    }
    while(1)
    {
        if(getPwmFlow(Pin, &flowRate) != OK)
        {/* unsuccesful, try again later */
            syslog(LOG_ERR, "Error reading sensor: %d!\n", SensPtr->DbId);
            sleep(10);
        }
        else
        {
            /* issue cmd to actuators if configured */
            if(SensPtr->headAct)
            {
                params[U_L_PER_MIN] = flowRate;
                issueActCmd(SensPtr->headAct, params);
            }
            /* insert into DB */
            if(insertSensorValue(SensPtr, flowRate, U_L_PER_MIN, (unsigned)time(NULL)) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting values for sensor: %d !\n", SensPtr->DbId);
            }
            /* check alert */
            checkAlert(SensPtr->DbId, flowRate, U_L_PER_MIN);
            /* go to sleep */
            sleep(SensPtr->SampleTime);
        }
    }
    return NULL;
}

void *controlTimeRelay(void * self)
{
    t_s_actuator *ActPtr = (t_s_actuator *) self;
    t_s_time_fct *ptrFct = NULL;
    struct timespec abstime;
    time_t currTime;
    struct tm *localTime;
    unsigned char Pin, Cmd, Level = HIGH, chg = FALSE;
    unsigned int index, toWait;
    int rv, rc;
    char queryString[50];


     abstime.tv_nsec = 0;
     ptrFct = (t_s_time_fct *)ActPtr->ctrlFnc;
     if(getWPiPin(ActPtr->AccessedBy, &Pin) < 0)
     {
         syslog(LOG_ERR, "Invalid Pin for ActuatorInd: %d \n", ActPtr->DbId);
         return NULL;
     }
     while(1)
     {
         while(1)
         {/* activate according to control func and insert into activation history*/
             currTime = time(NULL);
             localTime = localtime(&currTime);
             syslog(LOG_INFO, "Last activation: %d %d %d\n", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
             index = (localTime->tm_hour * 3600 + localTime->tm_min * 60 + localTime->tm_sec);
             toWait = ptrFct->resolution - (index % ptrFct->resolution);
             index /= ptrFct->resolution;
             if(index >= ptrFct->elemNr)
             {
                 index %= ptrFct->elemNr;
             }
             Level = ((ptrFct->elements)[index / 8u] & (unsigned char)(1u << (index % 8u))) ? LOW : HIGH;
             setRelay(Pin, Level, ActPtr->supervisionCycle);
             if((insertActuatorState(ActPtr, (Level == LOW) ? 1 : 0, U_NONE)) != SQLITE_OK)
             {
                 syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
             }

             /* wait for next cycle */
             abstime.tv_sec = currTime + toWait;
             pthread_mutex_lock(&(ActPtr->cmd_mutex));
             rv = pthread_cond_timedwait(&(ActPtr->cmd_cv), &(ActPtr->cmd_mutex), &abstime);
             if(ETIMEDOUT != rv)
             {
                 Cmd = ActPtr->extCmd;
                 pthread_mutex_unlock(&(ActPtr->cmd_mutex));
                 break;
             }
             pthread_mutex_unlock(&(ActPtr->cmd_mutex));
         }
         /* external command handling */
         while(1)
         {/* activate according to ext cmd */
             if(Cmd == CMD_ACTIVATE)
             {/* activate and insert into activation history */
                 Level = LOW;
                 chg = TRUE;
             }
             else if(Cmd == CMD_DEACTIVATE)
             {/* deactivate and insert into activation history */
                 Level = HIGH;
                 chg = TRUE;
             }
             else if(Cmd == CMD_UPDATE_FNC)
             {
                 snprintf(queryString, 45, "select CtrlFnc from Actuators where Ind = %d;", ActPtr->DbId);
                 rc = sqlite3_exec(db, queryString, updateActuatorCtrlFnc_CB, ActPtr, NULL);
                 if(rc != SQLITE_OK)
                 {
                     syslog(LOG_ERR, "SQL error when querying for actuator %d\n", ActPtr->DbId);
                     //send back NOK
                 }
             }
             else
             {
                 break;
             }
             if(chg)
             {
                 setRelay(Pin, Level, ActPtr->supervisionCycle);
                 if((insertActuatorState(ActPtr, (Level == LOW) ? 1 : 0, U_NONE)) != SQLITE_OK)
                 {
                     syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
                 }
             }
             /* wait for next cmd */
             pthread_mutex_lock(&(ActPtr->cmd_mutex));
             pthread_cond_wait(&(ActPtr->cmd_cv), &(ActPtr->cmd_mutex));
             Cmd = ActPtr->extCmd;
             pthread_mutex_unlock(&(ActPtr->cmd_mutex));
         }
     }
     return NULL;
}

void *controlHysteresis(void * self)
{
    t_s_actuator *ActPtr = (t_s_actuator *) self;
    t_s_temp_hyst_fct *ptrFct = NULL;
    unsigned char Pin, Cmd, Ovride = FALSE;
    float Param;
    char queryString[50];
    int rc;
    struct sched_param param = {0};

    /* increase thread prio */
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if(pthread_setschedparam(ActPtr->WorkerThread, SCHED_FIFO, &param) == -1)
    {
    	syslog(LOG_ERR, "Cannot increase thread priority: %d!", errno);
        perror("sched_setscheduler");
        return NULL;
    }

    ptrFct = (t_s_temp_hyst_fct *)ActPtr->ctrlFnc;
    if(getWPiPin(ActPtr->AccessedBy, &Pin) < 0)
    {
        syslog(LOG_ERR, "Invalid Pin for ActuatorInd: %d \n", ActPtr->DbId);
        return NULL;
    }
     /* command handling */
    while(1)
    {
     /* wait for cmd */
        pthread_mutex_lock(&(ActPtr->cmd_mutex));
        pthread_cond_wait(&(ActPtr->cmd_cv), &(ActPtr->cmd_mutex));
        Cmd = ActPtr->extCmd;
        Param = ActPtr->paramCmd;
        pthread_mutex_unlock(&(ActPtr->cmd_mutex));

        syslog(LOG_INFO, "Command received for %d with param %f \n", ActPtr->DbId, Param);
        switch(Cmd)
        {
        case CMD_ACTIVATE:
            /* activate and insert into activation history */
            Ovride = TRUE;
            setRelay(Pin, LOW, ActPtr->supervisionCycle);
            if((insertActuatorState(ActPtr, 1, U_NONE)) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
            }
            break;
        case CMD_DEACTIVATE:
            /* deactivate and insert into activation history */
            Ovride = TRUE;
            setRelay(Pin, HIGH, ActPtr->supervisionCycle);
            if((insertActuatorState(ActPtr, 0, U_NONE)) != SQLITE_OK)
            {
                syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
            }
            break;
        case CMD_CALCULATE:
            if(FALSE == Ovride)
            {
                if(Param < ptrFct->threshold - ptrFct->hysteresis)
                {
                    /* activate and insert into activation history */
                    setRelay(Pin, LOW, ActPtr->supervisionCycle);
                    if((insertActuatorState(ActPtr, 1, U_NONE)) != SQLITE_OK)
                    {
                        syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
                    }
                }
                else if(Param > ptrFct->threshold + ptrFct->hysteresis)
                {
                	/* deactivate and insert into activation history */
                    setRelay(Pin, HIGH, ActPtr->supervisionCycle);
                    if((insertActuatorState(ActPtr, 0, U_NONE)) != SQLITE_OK)
                    {
                        syslog(LOG_ERR, "SQL error when inserting state for actuator: %d !\n", ActPtr->DbId);
                    }
                }
                else
                {/* no change */
                }
            }
            break;
        case CMD_UPDATE_FNC:
            snprintf(queryString, 45, "select CtrlFnc from Actuators where Ind = %d;", ActPtr->DbId);
            rc = sqlite3_exec(db, queryString, updateActuatorCtrlFnc_CB, ActPtr, NULL);
            if( rc != SQLITE_OK )
            {
                syslog(LOG_ERR, "SQL error when querying for actuator %d\n", ActPtr->DbId);
                //send back NOK
            }
            break;
        case CMD_RELEASE:
        default:
            Ovride = FALSE;
            break;
        }
    }
    return NULL;
}
