import time
import requests
import websocket
import threading
import json
import logging
from logging.handlers import SysLogHandler

GW_URL = "http://localhost/monitor/kodi.php"
KODI_URL = "http://libreelec.local:8080/jsonrpc"
WEB_THING = "ws://raspi0.local:8888"

h = 1
STOPPED_TMR = 0xFF
CONST_NO_MOTION_S = (2 * 60 * h) #2min
bright = 0.0

def GetKodiStatus():
    player_active = False
    try:
        r = requests.post(KODI_URL, json = {"jsonrpc": "2.0", "method": "Player.GetActivePlayers", "id": 1})
        if r.status_code == 200:
            player_active = len((r.json())['result']) != 0
    except (Timeout):
        logger.debug("Could not reach Kodi!")
        pass
    return player_active

def on_WebThingMsg(ws, message):
    global timer, bright, lock
    msg = json.loads(message)
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
                    p = requests.post(GW_URL, auth=("atirage", "januar14"), data = {"Cmd":"1"})
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
                p = requests.post(GW_URL, auth=("atirage", "januar14"), data = {"Cmd":"2"})
            else:
                timer = CONST_NO_MOTION_S
    lock.release()
    threading.Timer(h, HandleNoMotion).start()

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
