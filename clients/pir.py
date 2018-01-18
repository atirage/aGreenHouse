from gpiozero import MotionSensor
from time import sleep
import subprocess

pir = MotionSensor(21)
timer = 255
motion_prev = False;

while (True):
    motion = pir.motion_detected
    if motion && !motion_prev:
      print("Motion detected!")
      timer = 255
      subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=1", "http://192.168.0.150/monitor/kodi.php"])
    else:
      if !pir.motion_detected && motion_prev:
        timer = 120
    
    if 0 < timer < 255:
      timer--
      if timer == 0
           subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=2", "http://192.168.0.150/monitor/kodi.php"])
    motion_prev = motion
    sleep(1)
