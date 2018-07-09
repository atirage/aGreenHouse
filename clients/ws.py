import time
import requests
import websocket
import threading
import json
import logging

GW_URL = "http://127.0.0.1/monitor/kodi.php"
KODI_URL = "http://192.168.0.178:8080/jsonrpc"
WEB_THING = "ws://192.168.0.31:8888"

h = 1
STOPPED_TMR = 0xFF
CONST_NO_MOTION_S = (2 * 60 * h) #2min
bright = 0.0

def GetKodiStatus():
    player_active = False
    r = requests.post(KODI_URL, json = {"jsonrpc": "2.0", "method": "Player.GetActivePlayers", "id": 1})
    if r.status_code == 200:
        player_active = len((r.json())['result']) != 0
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
                    logging.debug("Valid Motion detected @brightness: " + str(bright))
                    p = requests.post(GW_URL, auth=("atirage", "januar14"), data = {"Cmd":"1"})
                else:
                    lock.acquire()
                    timer = CONST_NO_MOTION_S
                    lock.release()
            if propId == 'light':
                bright = msg['data'][propId]

def on_error(ws, error):
    logging.debug(error)

def on_close(ws):
    logging.debug("Websocket closed!")

def HandleNoMotion():
    global h, timer, lock
    lock.acquire()
    if 0 < timer < STOPPED_TMR:
        timer -= 1
        if timer == 0:
            #check if request is allowed
            if GetKodiStatus() == False:
                logging.debug("No motion timeout!")
                p = requests.post(GW_URL, auth=("atirage", "januar14"), data = {"Cmd":"2"})
            else:
                timer = CONST_NO_MOTION_S
    lock.release()
    threading.Timer(h, HandleNoMotion).start()

#var init
timer = CONST_NO_MOTION_S
logging.basicConfig(filename='ws.log', level=logging.DEBUG, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
lock = threading.Lock()
ws = websocket.WebSocketApp(WEB_THING, on_message=on_WebThingMsg, on_error=on_error, on_close=on_close)

#will be called every 1sec
HandleNoMotion()

#ws.on_open = on_open
#connect ws and run
#while True:
ws.run_forever()
