#include <stdio.h>
#include <string.h>
#include <time.h>
#include "aGhActuators.h"

static void addTimeStamp(unsigned char pin, unsigned short int supervisionCycle);

void setRelay(unsigned char pin, unsigned char HiLo, unsigned short int supervisionCycle)
{
    /* refresh output */
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HiLo);
    /* timestamp it */
    addTimeStamp(pin, supervisionCycle);
}

static void addTimeStamp(unsigned char pin, unsigned short int supervisionCycle)
{
    FILE *fp = NULL;
    char fn[30] = {0}, stamp[18] = {0};

    snprintf(fn, 30, "/var/lib/aGreenHouse/out_%d", pin);
    snprintf(stamp, 18, "%d@%d", supervisionCycle, (unsigned)time(NULL));
    fp = fopen(fn, "w");
    if(fp < 0)
    {
    	return;
    }
    fwrite(stamp, 1, strlen(stamp), fp);
    fclose(fp);
}

/*
 * find /path/to/directory -type f -name 'out_*' -delete
 *
#!/bin/bash
 FILES=(/var/lib/aGreenhouse/out_*)
 for f in "${FILES[@]}"
 do
  echo "$f-"
  while read -r line || [[ -n $line ]]
  do
    echo "$line"
    IFS='@' read -a array <<< "$line"
    let diff_s=$(date +%s)-${array[1]}
    echo "${array[0]} ${array[1]} $diff_s"
    if [ "$diff_s" -gt ${array[0]} ]; then
      exit 1
    fi
  done < "$f"
 done
 exit 0
*/
