#from gpiozero import MotionSensor
#import subprocess

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

import time
import datetime
import syslog
import requests
import RPi.GPIO as GPIO
import Adafruit_GPIO.SPI as SPI
import Adafruit_SSD1306

URL = "http://192.168.0.150/monitor/"
KODI_URL = "http://192.168.0.178:8080/jsonrpc"

def CheckTimeIn(h0, m0, h1, m1):
    start = datetime.time(h0, m0)
    end = datetime.time(h1, m1)
    timenow = datetime.datetime.now().time()
    if (start <= timenow <= end):
        return True
    else:
        return False 

def GetKodiStatus():
    player_active = False
    r = requests.post(KODI_URL, json = {"jsonrpc": "2.0", "method": "Player.GetActivePlayers", "id": 1})
    if r.status_code == 200:
        player_active = len((r.json())['result']) != 0
    return player_active

def GetLivingData(ambT, ambRH, wifiLED):
    r = requests.get(URL + "current.php", auth=("atirage", "januar14"))
    rv = r.status_code
    if rv == 200:
        start = 0
        end = len(r.content) - 1
        target = 3
        while start < end:
            i = (r.content).find("addRows", start, end)
            if(i != -1):
                j = (r.content).find(";", i, end)
                line = (r.content)[i : j]
                #look for amb temp
                k = line.find("\u00BAC")
                if k != -1:
                    if line.find("Living", k - 13, k - 1):
                        l = line.rfind("\"", k - 13, k - 1)
                        target -= 1
                        ambT = line[l + 1 : k] + "\xB0C"
                #look for RH
                k = line.find("%")
                if k != -1:
                    if line.find("Living", k - 13, k -1):
                        l = line.rfind("\"", k - 13, k -1)
                        target -= 1
                        ambRH = line[l + 1 : k] + "%"
                #look for wifiLED state
                k = line.find("Lobby")
                if k != -1:
                    if line.find("ON", k + 5, k + 15) != -1:
                        target -= 1
                        wifi_LED = True
                    elif line.find("OFF", k + 5, k + 15) != -1:
                        target -= 1
                        wifi_LED = False
                start = j
            else:
                start = end
            if target <= 0:
                start = end
    return ambT, ambRH, wifi_LED

# set up PiOLED -------------------------------
# Raspberry Pi pin configuration:
RST = None
# Note the following are only used with SPI:
DC = 23
SPI_PORT = 0
SPI_DEVICE = 0
# 128x32 display with hardware I2C:
disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST)
 
# 128x64 display with hardware I2C:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST)
 
# Alternatively you can specify an explicit I2C bus number, for example
# with the 128x32 display you would use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, i2c_bus=2)
 
# 128x32 display with hardware SPI:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))
 
# 128x64 display with hardware SPI:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))
 
# Alternatively you can specify a software SPI implementation by providing
# digital GPIO pin numbers for all the required display pins.  For example
# on a Raspberry Pi with the 128x32 display you might use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, sclk=

# Initialize library.
disp.begin()
 
# Clear display.
disp.clear()
disp.display()
 
# Create blank image for drawing.
# Make sure to create image with mode '1' for 1-bit color.
width = disp.width
height = disp.height
image = Image.new('1', (width, height))
 
# Get drawing object to draw on image.
draw = ImageDraw.Draw(image)

# Load font.
font = ImageFont.truetype('/home/pi/.fonts/OpenSans-Regular.ttf', size=21)

# set up PIR -------------------------------
#GPIO.setmode(GPIO.BOARD)
GPIO.setmode(GPIO.BCM)
GPIO.setup(20, GPIO.IN)
GPIO.setup(21, GPIO.IN)
#pir = MotionSensor(21)
CONST_RELOAD_S = 300
CONST_NO_MOTION_S = 120
#var init
timer = CONST_NO_MOTION_S
slow_timer = 0
motion_prev = False
amb_temp = "--"
rh = "--"
wifi_LED = False
separator = ":"
bright = False

#for i in range(128,255):
#    print(str(i) + '=' +chr(i))

#image = Image.new('1', (height, width))
#draw.rectangle((0,0,height, width), outline=0, fill=0)
#draw.text((0, 0), '2', font=font, fill=255)
#draw.text((0, 42), '2', font=font, fill=255)
#draw.text((12, 84), '\xB0', font=font, fill=255)
#image1 = image.rotate(90, expand = 1)
#print(image.size[0],image.size[1])
#disp.image(image1)
#disp.display()

while (True):
    # handle PiOLED------------------------
    # Draw a black filled box to clear the image.
    draw.rectangle((0,0,width,height), outline=0, fill=0)
    # Get Temperature from alarmpi
    if slow_timer == 0:
        amb_temp, rh, wifi_LED = GetLivingData(amb_temp, rh, wifi_LED)
    # Write two lines of text.
    draw.text((0, 0), amb_temp + separator + rh, font=font, fill=255)
    #image.rotate(90)
    
    # Display image.
    disp.image(image)
    disp.display()

    # motion sensor handling---------------
    #motion = pir.motion_detected
    bright = GPIO.input(20)
    motion = GPIO.input(21)
    if motion and (not motion_prev):
      timer = 255
      if (not bright):
        syslog.syslog("Valid Motion detected!")
        #subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=1", url + "kodi.php"])
        p = requests.post(url + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"1"})
    else:
      if (not motion) and motion_prev:
          timer = CONST_NO_MOTION_S
    
    if 0 < timer < 255:
      timer -= 1
      if timer == 0:
          syslog.syslog("No motion timeout!")
          #check if request is allowed
          if GetKodiStatus() == False:
              p = requests.post(url + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"2"})
              #subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=2", url + "kodi.php"])
          else:
              timer = CONST_NO_MOTION_S
              
    motion_prev = motion
    if slow_timer > 0:
        slow_timer -= 1
    else:
        slow_timer = CONST_RELOAD_S
    if separator == ":":
        separator = "."
    else:
        separator = ":"
    time.sleep(1)
