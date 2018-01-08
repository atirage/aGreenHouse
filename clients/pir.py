from gpiozero import MotionSensor
from time import sleep
import subprocess

pir = MotionSensor(21)

while (True):
    if pir.motion_detected:
      print("Motion detected!")
      subprocess.call(["wget", "-q", "-T =3", "-O/dev/null", "--user=atirage", "--password=januar14", "--post-data=Cmd=1", "http://192.168.0.150/monitor/kodi.php"])
      pir.wait_for_no_motion()
