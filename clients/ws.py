import time
import requests
import urllib3
import websocket
import threading
import json
import logging
import touchphat
from logging.handlers import SysLogHandler

#GW_URL = "http://localhost/monitor/kodi.php"
GW_URL = "https://gateway/things/"
KODI_URL = "http://libreelec:8080/jsonrpc"
WEB_THING = "ws://localhost:8888"

h = 1
STOPPED_TMR = 0xFFFF
CONST_NO_MOTION_S = (10 * 60 * h) #10min
RETRY_S = (4 * 60 *h) #2min
bright = 0.0

def sendToGW(thing, property, cmd):
    rv = False
    try:
        p = requests.put(GW_URL + thing + '/properties/' + property,
                         headers = {
                                    'Accept': 'application/json',
                                    'content-type': 'application/json',
                                    'Authorization': 'Bearer eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImNjNDc0Y2Q5LWQ0NDAtNDc0Yi1hYWM3LWEwZjcwNmFlYjg0YiJ9.eyJjbGllbnRfaWQiOiJsb2NhbC10b2tlbiIsInJvbGUiOiJhY2Nlc3NfdG9rZW4iLCJzY29wZSI6Ii90aGluZ3M6cmVhZHdyaXRlIiwiaWF0IjoxNTQwMjI0NzAxfQ.oj_bd4W-wNtm8ZREPSGdrI9FchFQpR1uDUbsaF4ILYiX47fDfvdwnCOVOudxFCKzkGEU4Zcg7JwhV3KU-AqxQg',
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

def GetKodiStatus():
    player_active = False
    try:
        r = requests.post(KODI_URL, json = {"jsonrpc": "2.0", "method": "Player.GetActivePlayers", "id": 1})
        if r.status_code == 200:
            player_active = len((r.json())['result']) != 0
    except (requests.exceptions.Timeout, requests.exceptions.ConnectionError):
        logger.debug("Could not reach Kodi!")
        pass
    return player_active

def on_WebThingMsg(ws, message):
    global timer, bright, lock
    msg = json.loads(message)
    #logger.debug(msg)
    if msg['messageType'] == 'propertyStatus':
        for propId in msg['data']:
            if propId == 'motion':
                motion = msg['data'][propId]
                # motion sensor handling
                if motion:
                  lock.acquire()
                  timer = STOPPED_TMR
                  lock.release()
                  if(bright < 0.5):
                    logger.debug("Valid Motion detected @brightness: " + str(bright))
                    sendToGW('miLight-adapter-0', 'on', {'on':True})
                else:
                    lock.acquire()
                    timer = CONST_NO_MOTION_S
                    lock.release()
            if propId == 'light':
                bright = msg['data'][propId]

def on_error(ws, error):
    logger.debug(error)

def on_close(ws):
    logger.debug("Websocket closed!")

def HandleNoMotion():
    global h, timer, lock
    lock.acquire()
    if 0 < timer < STOPPED_TMR:
        timer -= 1
        if timer == 0:
            #check if request is allowed
            if GetKodiStatus() == False:
                logger.debug("No motion timeout!")
                if sendToGW('miLight-adapter-0', 'on', {'on':False}) != False:
                    timer = RETRY_S
            else:
                timer = CONST_NO_MOTION_S
    lock.release()
    threading.Timer(h, HandleNoMotion).start()

@touchphat.on_touch(['Back','A','B','C','D','Enter'])
def handle_All(event):
    logger.debug("Touch:" + event.name)
    if event.name == 'Back':
        thr = threading.Thread(target=SendMultippleOffToGW, args=(['miLight-adapter-0', 'gpio-18'], ), kwargs={})
        thr.start()
    elif event.name == 'A':
        thr = threading.Thread(target=sendToGW, args=('miLight-adapter-0', 'level', {'level':20}, ), kwargs={})
        thr.start()
    elif event.name == 'B':
        thr = threading.Thread(target=sendToGW, args=('miLight-adapter-0', 'level', {'level':40}, ), kwargs={})
        thr.start()
    elif event.name == 'C':
        thr = threading.Thread(target=sendToGW, args=('miLight-adapter-0', 'level', {'level':60}, ), kwargs={})
        thr.start()
    elif event.name == 'D':
        thr = threading.Thread(target=sendToGW, args=('miLight-adapter-0', 'level', {'level':80}, ), kwargs={})
        thr.start()
    elif event.name == 'Enter':
        thr = threading.Thread(target=sendToGW, args=('miLight-adapter-0', 'level',  {'level':100}, ), kwargs={})
        thr.start()
    else:
        pass

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
#set up logger
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
logger.addHandler(SysLogHandler(address='/dev/log'))

#var init
timer = CONST_NO_MOTION_S
lock = threading.Lock()
ws = websocket.WebSocketApp(WEB_THING, on_message=on_WebThingMsg, on_error=on_error, on_close=on_close)

#will be called every 1sec
HandleNoMotion()

#ws.on_open = on_open
#connect ws and run
while True:
    ws.run_forever()
    time.sleep(5)
