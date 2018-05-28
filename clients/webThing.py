from webthing import (Action, Event, Property, Value, SingleThing, Thing, WebThingServer)
import logging
import threading
import time
import uuid
import RPi.GPIO as GPIO
from envirophat import weather
from envirophat import light

h = 1

class EnvironSensor(Thing):
    """An environment(motion, pressure, temp, light) sensor which updates every few seconds."""
    global h

    def __init__(self):
        Thing.__init__(self,
                       'My Environ Sensor',
                       'environSensor',
                       'A web connected environment sensor')

        self.motion = Value(False)
        self.add_property(
            Property(self, 'motion', self.motion,
                     metadata={
							  'type': 'boolean',
							  'description': 'Whether the sensor is on',
							  }))
        self.light = Value(0)
        self.add_property(
            Property(self, 'light', self.light,
                     metadata={
							  'description': 'The level of light from 0-255',
							  'minimum': 0,
							  'maximum': 255,
							  }))
        logging.debug('starting the sensor update looping task')
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(21, GPIO.IN)
        self.update_sensors()

    def update_sensors(self):
      self.motion.notify_of_external_update(GPIO.input(21))
      #atm = round(weather.pressure()/101325, 2)
      self.light.notify_of_external_update(light.light())
      threading.Timer(h, self.update_sensors).start()

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
