import asyncio
import requests
import urllib3
import websockets
import json
import logging
from logging.handlers import SysLogHandler

#KODI_URL = "http://libreelec:8080/jsonrpc"
#GW_URL = "//localhost/things/"
GW_URL = "//gateway.local/things/"
ZGB_BTN_THING = "zb-84fd27fffe1f812e" 
MOTION_THING = "gpio-4"
JWT = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjMwMmU3NDJiLWRlYTItNDI5NS1iMGI4LTNlNzBiYzg4YjBjYyJ9.eyJjbGllbnRfaWQiOiJsb2NhbC10b2tlbiIsInJvbGUiOiJhY2Nlc3NfdG9rZW4iLCJzY29wZSI6Ii90aGluZ3M6cmVhZHdyaXRlIiwiaWF0IjoxNjY5MDMyMzI0LCJpc3MiOiJodHRwczovL2F0aXJhZ2Uud2VidGhpbmdzLmlvIn0.H2Sgvzhb0dnmnO-6VT7Iix7uxJBWO8YDZlVEe1WGTuB1UZ7XbQkHvBjsds7p4uS3v0wnYZAcKs8fuWe4bQoDtw"

def sendToGW(thing, property, cmd):
    rv = False
    try:
        p = requests.put('http:' + GW_URL + thing + '/properties/' + property,
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
        logger.debug("Could not reach IoT Gateway!")
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

async def ZgbBtnWs():
    async with websockets.connect('ws:%s%s?jwt=%s' % (GW_URL, ZGB_BTN_THING, JWT)) as ws:
        while True:
            msg = json.loads(await ws.recv())
            if msg['messageType'] == 'propertyStatus':
                for propId in msg['data']:
                    if propId in ('on', 'level'):
                        sendToGW('miLight-adapter-2', propId, {propId : msg['data'][propId]})
                    else:
                        pass

async def MotionWs(q):
    async with websockets.connect('ws:%s%s?jwt=%s' % (GW_URL, MOTION_THING, JWT)) as ws:
        while True:
            msg = json.loads(await ws.recv())
            if msg['messageType'] == 'propertyStatus' and 'on' in msg['data'] and msg['data']['on']:
                q.put_nowait('motion')

async def Delay(q):
    await asyncio.sleep(120)
    q.put_nowait('timeout')

async def SwitchFsm(q):
    st = 0 #OFF
    tmr = None
    while True:
        item = await q.get()
        if st == 0:
            if item == 'motion':
                print('motion')
                #calculate needed level, could be 0
                sendToGW('miLight-adapter-0', 'on', {'on':True})
                #start timer
                tmr = asyncio.create_task(Delay(q))
                st = 1
        elif st == 1:
            if item == 'timeout':
                print('timeout')
                sendToGW('miLight-adapter-0', 'on', {'on':False})
                st = 0
            elif item == 'motion':
                tmr.cancel()
                tmr = asyncio.create_task(Delay(q))
            else:
                pass    
        else:
            pass
        q.task_done()
    return

async def main():

    Q = asyncio.Queue()
    L = await asyncio.gather(
                #ZgbBtnWs(),
                MotionWs(Q),
                SwitchFsm(Q),
        )
    return

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
#set up logger
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
logger.addHandler(SysLogHandler(address='/dev/log'))

asyncio.run(main())