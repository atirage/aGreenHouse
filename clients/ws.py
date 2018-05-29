import time
import datetime
import syslog
import requests
import websocket
import threading
import json

URL = "http://127.0.0.1/monitor/"
KODI_URL = "http://127.0.0.1:8080/jsonrpc"
WEB_THING = "http://raspi0:8888"

h = 1
STOPPED_TMR = 0xFF
CONST_NO_MOTION_S = (2 * 60 * h) #2min
bright = 0

def GetKodiStatus():
    player_active = False
    r = requests.post(KODI_URL, json = {"jsonrpc": "2.0", "method": "Player.GetActivePlayers", "id": 1})
    if r.status_code == 200:
        player_active = len((r.json())['result']) != 0
    return player_active

def on_WebThingMsg(ws, message):
    global timer, bright
    msg = json.loads(message)
    if msg.messageType == 'propertyStatus':
        for propId in msg.data:
            if propId == 'motion':
                motion = msg.data[propId]
            if propId == 'light':
                bright = msg.data[propId]
    # motion sensor handling---------------
    if motion:
      timer = STOPPED_TMR
      if(bright < 45):
        syslog.syslog("Valid Motion detected @brightness: " + str(bright))
        p = requests.post(URL + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"1"})
    else:
        timer = CONST_NO_MOTION_S

def on_error(ws, error):
    print(error)

def on_close(ws):
    print("### closed ###")

def HandleNoMotion():
    global h, timer
    if 0 < timer < STOPPED_TMR:
        timer -= 1
        if timer == 0:
            #check if request is allowed
            if GetKodiStatus() == False:
                syslog.syslog("No motion timeout!")
                p = requests.post(URL + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"2"})
            else:
                timer = CONST_NO_MOTION_S
    threading.Timer(h, HandleNoMotion).start()

#var init
timer = CONST_NO_MOTION_S

ws = websocket.WebSocketApp(WEB_THING, on_message=on_WebThingMsg, on_error=on_error, on_close=on_close)

#will be called every 1sec
HandleNoMotion()

#ws.on_open = on_open
#connect ws and run
ws.run_forever()
