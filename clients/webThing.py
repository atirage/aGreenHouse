#python 3.5
#temp_calibrated = temp - ((cpu_temp - temp)/factor)

from asyncio import sleep, CancelledError, get_event_loop
from webthing import (Action, Event, Property, Value, SingleThing, Thing, WebThingServer)
import syslog
import time
import uuid
import RPi.GPIO as GPIO
from envirophat import weather
from envirophat import light

h = 2 #2sec

class EnvironSensor(Thing):
    """An environment(motion, pressure, temp, light) sensor which updates every few seconds."""
    global h

    def __init__(self):
        Thing.__init__(self,
                       'My Environ Sensor Thing',
                       'environSensor',
                       'A web connected environment sensor')
        #pir motion sensor
        self.motion = Value(False)
        self.add_property(
            Property(self, 'motion', self.motion,
                     metadata={
                                'type': 'boolean',
                                'description': 'Whether motion is detected',
                              }))
        #light sensor
        self.light = Value(0)
        self.add_property(
            Property(self, 'light', self.light,
                     metadata={
                                'description': 'The level of light from 0-255',
                                'minimum': 0,
                                'maximum': 255,
                              }))
        #pressure sensor
        self.pressure = Value(0.0)
        self.add_property(
            Property(self, 'pressure', self.pressure,
                     metadata={
                                'description': 'The level of atmospheric pressure in atm',
                                'minimum': 0,
                                'maximum': 3.0,
                              }))
        #temperature sensor
        self.cpu_temp = Value(0.0)
        self.temp = Value(0.0)
        self.add_property(
            Property(self, 'temperature', self.temp,
                     metadata={
                                'description': 'The level of ambient temperature in C',
                                'minimum': -40.0,
                                'maximum': 100.0,
                              }))

        GPIO.setmode(GPIO.BCM)
        GPIO.setup(21, GPIO.IN)
        syslog.syslog('Starting the sensor update looping task')
        self.enviro_task = get_event_loop().create_task(self.update_PHATsensors())
        self.motion_task = get_event_loop().create_task(self.detect_motion())
        
    async def update_PHATsensors(self):
        try:
            while True:
                await asyncio.sleep(h)
                self.pressure.notify_of_external_update(round(weather.pressure()/101325, 2))
                self.light.notify_of_external_update(light.light())
                fd = open('/sys/class/thermal/thermal_zone0/temp', 'r')
                data = fd.read()
                fd.close()
                if data != 'NA':
                    self.cpu_temp = floor(int(data) / 100.0) #0.1deg
                amb_temp = floor(weather.temperature() * 10) #0.1deg
                self.temp.notify_of_external_update(round((amb_temp - ((self.cpu_temp - amb_temp) / 2.0)) / 10.0, 1))
        except CancelledError:
            pass
        
    async def detect_motion(self):
        try:
            while True:
                await get_event_loop().run_in_executor(None, GPIO.wait_for_edge, channel = 21, edge = GPIO.BOTH) 
                self.motion.notify_of_external_update(GPIO.input(21))
        except CancelledError:
            pass
        
    def cancel_tasks(self):
        self.enviro_task.cancel()
        self.motion_task.cancel()
        get_event_loop().run_until_complete(self.enviro_task)
        get_event_loop().run_until_complete(self.motion_task)

def run_server():
    sensors = EnvironSensor()

    # If adding more than one thing here, be sure to set the `name`
    # parameter to some string, which will be broadcast via mDNS.
    # In the single thing case, the thing's name will be broadcast.
    server = WebThingServer(SingleThing(sensors), port=8888)
    try:
        syslog.syslog('Starting the Webthing server')
        server.start()
    except KeyboardInterrupt:
        sensors.cancel_tasks()
        server.stop()
        syslog.syslog('Webthing server stopped')

if __name__ == '__main__':
    run_server()
