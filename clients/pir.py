#from gpiozero import MotionSensor
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

#import subprocess
import time
import requests
import RPi.GPIO as GPIO
import Adafruit_GPIO.SPI as SPI
import Adafruit_SSD1306

url = "http://192.168.0.150/monitor/"

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
 
# Draw a black filled box to clear the image.
#draw.rectangle((0,0,width,height), outline=0, fill=0)

# Load font.
font = ImageFont.truetype('/home/pi/.fonts/OpenSans-Regular.ttf', size=28)

# set up PIR -------------------------------
#GPIO.setmode(GPIO.BOARD)
GPIO.setmode(GPIO.BCM)
GPIO.setup(23, GPIO.IN)
GPIO.setup(21, GPIO.IN)
#pir = MotionSensor(21)

timer = 255
slow_timer = 0 
motion_prev = False;
amb_temp = "--"
rh= "--"
bright = false

#for i in range(128,255):
#    print(str(i) + '=' +chr(i))

while (True):
    # handle PiOLED------------------------
    # Draw a black filled box to clear the image.
    draw.rectangle((0,0,width,height), outline=0, fill=0)
 
    # Get Temperature from alarmpi
    if slow_timer == 0:
        r = requests.get(url + "current.php", auth=("atirage", "januar14"))
        rv = r.status_code
    else:
        rv = 404 #dummy
    if rv == 200:
        #find ambient temperature
        lines = (r.content).split('\n')
        for line in lines:
            i = line.find("\u00BAC")
            if i != -1:
                substr = line[i-13:i-1]
                if substr.find("Living"):
                    #found
                    amb_temp = line[i-3:i-1]+"\xB0C"
            i = line.find("%")
            if i != -1:
                substr = line[i-13:i-1]
                if substr.find("Living"):
                    #found
                    rh = line[i-3:i-1]+"%"
    # Write two lines of text.
    #draw.text((x, top), amb_temp,  font=font, fill=255)
    draw.text((0, 0), amb_temp + ' ' + rh, font=font, fill=255)
    #image.rotate(90)
    
    # Display image.
    disp.image(image)
    disp.display()

    # motion sensor handling---------------
    #motion = pir.motion_detected
    bright = GPIO.input(23)
    motion = GPIO.input(21)
    print(bright)
    if motion and (not motion_prev) and (not bright):
      print("Motion detected!")
      timer = 255
      #subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=1", url + "kodi.php"])
      p = requests.post(url + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"1"})
    else:
      if (not motion) and motion_prev:
          timer = 120
    
    if 0 < timer < 255:
      timer -= 1
      if timer == 0:
          print("No motion timeout!")
          p = requests.post(url + "kodi.php", auth=("atirage", "januar14"), data = {"Cmd":"2"})
          #subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=2", url + "kodi.php"])
    motion_prev = motion
    if slow_timer > 0:
        slow_timer -= 1
    else:
        slow_timer = 300
    time.sleep(1)
