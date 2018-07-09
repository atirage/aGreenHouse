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

from envirophat import weather

URL = "http://192.168.0.150/monitor/current.php"

STOPPED_TMR = 0xFFFF
h = 0.1 # needs to be < 1
h_1 = int(1/h) 

def GetImgOffset(t):
    global T
    if t <= T/2:
        return (64 * 2 * t) / T
    else:
        return (2 * 64 * (T - t)) / T

def CheckTimeIn(h0, m0, h1, m1):
    start = datetime.time(h0, m0)
    end = datetime.time(h1, m1)
    timenow = datetime.datetime.now().time()
    if (start <= timenow <= end):
        return True
    else:
        return False 

def GetLivingData(ambT, ambRH, wifiLED, mask = 0x07):
    try:
        r = requests.get(URL, auth=("atirage", "januar14"))
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
                    if mask & 0x01:
                        #look for amb temp
                        k = line.find("\u00BAC")
                        if k != -1:
                            if line.find("Living", k - 13, k - 1):
                                l = line.rfind("\"", k - 13, k - 1)
                                mask &= 0xFE 
                                ambT = line[l + 1 : k] + "\xB0C"
                    if mask & 0x02:
                        #look for RH
                        k = line.find("%")
                        if k != -1:
                            if line.find("Living", k - 13, k -1):
                                l = line.rfind("\"", k - 13, k -1)
                                mask &= 0xFD
                                ambRH = line[l + 1 : k] + "%"
                    if mask & 0x04:
                        #look for wifiLED state
                        k = line.find("Lobby")
                        if k != -1:
                            if line.find("ON", k + 5, k + 15) != -1:
                                mask &= 0xFB
                                wifiLED = True
                            elif line.find("OFF", k + 5, k + 15) != -1:
                                mask &= 0xFB
                                wifiLED = False
                    start = j
                else:
                    start = end
                if mask == 0:
                    start = end
    except (ConnectionError, Timeout):
        pass
    return ambT, ambRH, wifiLED

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
image = Image.new('1', (width, 3 * height))
# Get drawing object to draw on image.
draw = ImageDraw.Draw(image)
# Load font.
font = ImageFont.truetype('/home/pi/.fonts/OpenSans-Regular.ttf', size=30)

#timing constants
CONST_RELOAD_S = (5 * 60 * h_1)    #5min
CONST_2_S = 2 * h_1                #2s

#var init
hold = CONST_2_S - 1
slow_timer = 0
amb_temp = "--"
rh = "--"
atm = "--"
wifi_LED = False
T = 20
t = 0
y = 0

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
    if slow_timer == 0:
        atm = round(weather.pressure()/101325, 2)
        # Get data from alarmpi
        amb_temp, rh, wifi_LED = GetLivingData(amb_temp, rh, wifi_LED)
    # handle PiOLED------------------------
    # Draw a black filled box to clear the image.
    draw.rectangle((0, 0, width, 3*height), outline=0, fill=0)
    # Write the lines of text.
    draw.text((20, 0), amb_temp, font=font, fill=255)
    draw.text((20, height), rh, font=font, fill=255)
    draw.text((0, 2*height), str(atm) + " atm", font=font, fill=255)
    #draw.text((0, 2*height), str(bright), font=font, fill=255)
    if hold == 0:
        if t < T:
            t += 1
        else:
            t = 0
        y = GetImgOffset(t)
        if y % 32 == 0:
            hold = CONST_2_S - 1
    else:
        hold = (hold + 1) % CONST_2_S
    # Display image.
    #image.rotate(90)
    image_tmp = image.crop((0, y, width, y + height))
    disp.image(image_tmp)
    disp.display()

    if slow_timer > 0:
        slow_timer -= 1
    else:
        slow_timer = CONST_RELOAD_S
    time.sleep(h)
    
#for i in range(128,255):
#    print(str(i) + '=' +chr(i))
