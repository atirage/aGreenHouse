#temp_calibrated = temp - ((cpu_temp - temp)/factor)

from webthing import (Action, Event, Property, Value, SingleThing, Thing, WebThingServer)
import logging
import threading
import time
import uuid
import RPi.GPIO as GPIO
from envirophat import weather
from envirophat import light

h = 2 #1sec

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
        #pressure sensor
        self.temp = Value(0.0)
        self.add_property(
            Property(self, 'temperature', self.temp,
                     metadata={
                                'description': 'The level of ambient temperature in C',
                                'minimum': -40.0,
                                'maximum': 100.0,
                              }))
        self.t = threading.Thread(target=self.detect_motion) 
        
        logging.debug('starting the sensor update looping task')
        self.t.start()
        self.update_PHATsensors()

    def update_PHATsensors(self):
        self.pressure.notify_of_external_update(round(weather.pressure()/101325, 2))
        self.light.notify_of_external_update(light.light())
        self.temp.notify_of_external_update(0.0)
        threading.Timer(h, self.update_PHATsensors).start()
        
    def detect_motion(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(21, GPIO.IN)
        while True:
            GPIO.wait_for_edge(21, GPIO.BOTH)
            self.motion.notify_of_external_update(GPIO.input(21))

def run_server():
    sensors = EnvironSensor()

    # If adding more than one thing here, be sure to set the `name`
    # parameter to some string, which will be broadcast via mDNS.
    # In the single thing case, the thing's name will be broadcast.
    server = WebThingServer(SingleThing(sensors), port=8888)
    try:
        logging.info('starting the server')
        server.start()
    except KeyboardInterrupt:
        logging.info('stopping the server')
        server.stop()
        logging.info('done')

if __name__ == '__main__':
    logging.basicConfig(level=10, format="%(asctime)s %(filename)s:%(lineno)s %(levelname)s %(message)s")
    run_server()
