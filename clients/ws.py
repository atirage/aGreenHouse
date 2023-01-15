import asyncio
import requests
import urllib3
import websockets
import json
import logging
import time
from logging.handlers import SysLogHandler

#KODI_URL = "http://libreelec:8080/jsonrpc"
JWT = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjMwMmU3NDJiLWRlYTItNDI5NS1iMGI4LTNlNzBiYzg4YjBjYyJ9.eyJjbGllbnRfaWQiOiJsb2NhbC10b2tlbiIsInJvbGUiOiJhY2Nlc3NfdG9rZW4iLCJzY29wZSI6Ii90aGluZ3M6cmVhZHdyaXRlIiwiaWF0IjoxNjY5MDMyMzI0LCJpc3MiOiJodHRwczovL2F0aXJhZ2Uud2VidGhpbmdzLmlvIn0.H2Sgvzhb0dnmnO-6VT7Iix7uxJBWO8YDZlVEe1WGTuB1UZ7XbQkHvBjsds7p4uS3v0wnYZAcKs8fuWe4bQoDtw"
ZGB_BTN_THING = 'zb-84fd27fffe1f812e'
ZGB_MOTION_THING = 'zb-00158d00047bce3d'
MOTION_THING = 'gpio-4'
#KITCHEN_THING = 'http---raspi0.local-8888'
BEDROOM_LIGHT = 'miLight-adapter-1'
HALLWAY_LIGHT = 'miLight-adapter-0'
KITCHEN_LIGHT = 'sonoff-diy-adapter-0'
TIMEOUT1 = 120
TIMEOUT2 = 300
LOCAL = True
DAYLIGHT = 2.0

if not LOCAL:
    GW_URL = "//gateway.local/things/"
else:
    GW_URL = "//127.0.0.1:8080/things/"

logger = logging.getLogger()
light_sensor = 0.0

def sendToGW(thing, property, cmd):
    rv = False
    uri = 'http:' + GW_URL + thing + '/properties/' + property
    try:
        p = requests.put(uri,
                         headers = {
                                    'Accept': 'application/json',
                                    'content-type': 'application/json',
                                    'Authorization': 'Bearer %s' % JWT,
                                    'User-Agent': 'Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/69.0.3497.100 Safari/537.36'
                                   },
                         verify = False,
                         timeout = 5,
                         json = cmd)
        if p.status_code != 200:
            rv = True
    except requests.exceptions.Timeout:
        logger.debug("Could not reach IoT Gateway @" + uri + " !")
        rv = True
    except requests.exceptions.ConnectionError as error:
        logger.debug("Could not reach IoT Gateway: " + str(error))
        rv = True
    return rv

def SendMultippleOffToGW(things):
    for th in things:
        sendToGW(th, 'on', {'on':False})

def SendMultippleLvlToGW(things, lvl):
    for th in things:
        sendToGW(th, 'level', {'level':lvl})

''' websocket connections, used to retrieve events coming from webthings'''
async def ZgbBtnWs():
    uri = 'ws:%s%s?jwt=%s' % (GW_URL, ZGB_BTN_THING, JWT)
    async for ws in websockets.connect(uri):
        try:
            while True:
                msg = json.loads(await ws.recv())
                if msg['messageType'] == 'propertyStatus':
                    for propId in msg['data']:
                        if propId in ('on', 'level'):
                            sendToGW(BEDROOM_LIGHT, propId, {propId : msg['data'][propId]})
        except websockets.ConnectionClosed:
            #try again
            continue

async def MotionWs(q):
    uri = 'ws:%s%s?jwt=%s' % (GW_URL, MOTION_THING, JWT)
    async for ws in websockets.connect(uri):
        try:
            while True:
                msg = json.loads(await ws.recv())
                if msg['messageType'] == 'propertyStatus' and 'on' in msg['data']:
                    if msg['data']['on']:
                        q.put_nowait('motion1')
                    else:
                        q.put_nowait('no_motion1') 
        except websockets.ConnectionClosed:
            #try again
            continue

async def ZgbMotionWs(q):
    uri = 'ws:%s%s?jwt=%s' % (GW_URL, ZGB_MOTION_THING, JWT)
    async for ws in websockets.connect(uri):
        try:
            while True:
                msg = json.loads(await ws.recv())
                if msg['messageType'] == 'propertyStatus' and 'occupied' in msg['data']:
                    if msg['data']['occupied']:
                        q.put_nowait('motion2')
                    else:
                        q.put_nowait('no_motion2') 
        except websockets.ConnectionClosed:
            #try again
            continue


'''async def KitchenWs(q):
    global light_sensor
    uri = 'ws:%s%s?jwt=%s' % (GW_URL, KITCHEN_THING, JWT)
    async for ws in websockets.connect(uri):
        try:
            while True:
                msg = json.loads(await ws.recv())
                if msg['messageType'] == 'propertyStatus':
                    for propId in msg['data']:
                        if propId == 'motion':
                            if msg['data'][propId]:
                                q.put_nowait('motion2')
                            else:
                                q.put_nowait('no_motion2') 
                        elif propId == 'light':
                            light_sensor = msg['data'][propId]    
        except websockets.ConnectionClosed:
            #try again
            continue'''

async def SwitchWs(thing, ev, q):
    uri = 'ws:%s%s?jwt=%s' % (GW_URL, thing, JWT)
    async for ws in websockets.connect(uri):
        try:
            while True:
                msg = json.loads(await ws.recv())
                if msg['messageType'] == 'propertyStatus' and 'on' in msg['data']:
                    if msg['data']['on']:
                        q.put_nowait(ev + '_on')
                    else:
                        q.put_nowait(ev + '_off')  
        except websockets.ConnectionClosed:
            #try again
            continue

async def Delay1(t, q):
    await asyncio.sleep(t)
    q.put_nowait('timeout1')

async def Delay2(t, q):
    await asyncio.sleep(t)
    q.put_nowait('timeout2')

def IsRunning(tmr):
    return (tmr != None and not tmr.done() and not tmr.cancelled())

''' statemachines implementing behaviors based on events '''
async def KitchenLightFsm(q):
    st = 0 #OFF
    tmr = None
    while True:
        item = await q.get()
        dt = time.localtime()
        if st == 0:
            '''if item == 'motion1' and light_sensor < DAYLIGHT and dt.tm_hour >= 7: # "dark" and daytime:
                if not sendToGW(KITCHEN_LIGHT, 'on', {'on':True}):
                    tmr = asyncio.create_task(Delay2(TIMEOUT2, q))
                    st = 1
            el'''
            if item == 'motion2' and light_sensor < DAYLIGHT:
                if not sendToGW(KITCHEN_LIGHT, 'on', {'on':True}):
                    st = 1
            elif item == 'light2_on':
                tmr = asyncio.create_task(Delay2(TIMEOUT2, q))
                st = 1
        elif st == 1:
            if item == 'timeout2':
                if sendToGW(KITCHEN_LIGHT, 'on', {'on':False}):
                    #unsuccessful, try again
                    tmr = asyncio.create_task(Delay2(1, q))
                else: 
                    st = 1
            elif item == 'motion2':
                if IsRunning(tmr):
                    tmr.cancel()
            elif item == 'no_motion2':
                if not IsRunning(tmr):
                    tmr = asyncio.create_task(Delay2(TIMEOUT2, q))
            elif item == 'light2_off':
                if IsRunning(tmr):
                    tmr.cancel()
                st = 0 
        else:
            pass            
        q.task_done()
        logger.debug('Kitchen State: ' + str(st))
    return

async def HallwayLightFsm(q):
    st = 0 #OFF
    tmr = None
    while True:
        item = await q.get()
        dt = time.localtime()
        if st == 0:
            if item == 'motion1':
                level = 50
                if dt.tm_hour >= 22 or dt.tm_hour < 7: level = 15
                elif light_sensor < DAYLIGHT and dt.tm_hour >= 7: level = 100 # "dark" and daytime
                if not sendToGW(HALLWAY_LIGHT, 'level', {'level':level}):
                    st = 1
            elif item == 'light1_on':
                #maybe start timeout
                st = 1
        elif st == 1:
            if item == 'timeout1':
                if sendToGW(HALLWAY_LIGHT, 'on', {'on':False}):
                    #unsuccessful, try again
                    tmr = asyncio.create_task(Delay1(1, q))
                else: 
                    st = 0
            elif item == 'motion1':
                if IsRunning(tmr):
                    tmr.cancel()
            elif item == 'no_motion1':
                if not IsRunning(tmr):
                    tmr = asyncio.create_task(Delay1(TIMEOUT1, q))
            elif item == 'light1_off':
                if IsRunning(tmr):
                    tmr.cancel()
                st = 0
        else:
            pass
        q.task_done()
        logger.debug('Hallway State: ' + str(st))
    return

''' putting it all together '''
async def main():
    Q1 = asyncio.Queue()
    Q2 = asyncio.Queue()
    L = await asyncio.gather(
                ZgbBtnWs(),
                SwitchWs(HALLWAY_LIGHT, 'light1', Q1),
                SwitchWs(KITCHEN_LIGHT, 'light2', Q2),
                #KitchenWs(Q2),
                MotionWs(Q1),
                ZgbMotionWs(Q2),
                HallwayLightFsm(Q1),
                KitchenLightFsm(Q2)
        )
    return

if __name__ == '__main__':
    #set up logger
    logging.getLogger("websockets").propagate = False
    logging.getLogger("websockets").setLevel(logging.CRITICAL)
    logger.setLevel(logging.DEBUG)
    logger.addHandler(SysLogHandler(address='/dev/log'))
    
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    asyncio.run(main())