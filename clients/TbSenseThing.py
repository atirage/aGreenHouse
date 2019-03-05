#python 3.5

from asyncio import sleep, CancelledError, get_event_loop
from functools import partial
from webthing import (Action, Event, Property, Value, SingleThing, Thing, WebThingServer)
from bluepy.btle import *
import struct
import syslog
import time
import uuid

h = 5 #5sec

class Thunderboard:

   def __init__(self, dev):
      self.dev  = dev
      self.char = dict()
      self.name = ''
      self.session = ''
      self.coinCell = False

      # Get device name and characteristics

      scanData = dev.getScanData()

      for (adtype, desc, value) in scanData:
         if (desc == 'Complete Local Name'):
            self.name = value

      ble_service = Peripheral()
      ble_service.connect(dev.addr, dev.addrType)
      characteristics = ble_service.getCharacteristics()

      for k in characteristics:
         if k.uuid == '2a6e':
            self.char['temperature'] = k
            
         elif k.uuid == '2a6f':
            self.char['humidity'] = k
            
         elif k.uuid == '2a76':
            self.char['uvIndex'] = k
            
         elif k.uuid == '2a6d':
            self.char['pressure'] = k
            
         elif k.uuid == 'c8546913-bfd9-45eb-8dde-9f8754f4a32e':
            self.char['ambientLight'] = k

         elif k.uuid == 'c8546913-bf02-45eb-8dde-9f8754f4a32e':
            self.char['sound'] = k

         elif k.uuid == 'efd658ae-c401-ef33-76e7-91b00019103b':
            self.char['co2'] = k

         elif k.uuid == 'efd658ae-c402-ef33-76e7-91b00019103b':
            self.char['voc'] = k

         elif k.uuid == 'ec61a454-ed01-a5e8-b8f9-de9ec026ec51':
            self.char['power_source_type'] = k

   
   def readTemperature(self):
      value = self.char['temperature'].read()
      value = struct.unpack('<H', value)
      value = value[0] / 100
      return value

   def readHumidity(self):
      value = self.char['humidity'].read()
      value = struct.unpack('<H', value)
      value = value[0] / 100
      return value

   def readAmbientLight(self):
      value = self.char['ambientLight'].read()
      value = struct.unpack('<L', value)
      value = value[0] / 100
      return value

   def readUvIndex(self):
      value = self.char['uvIndex'].read()
      value = ord(value)
      return value

   def readCo2(self):
      value = self.char['co2'].read()
      value = struct.unpack('<h', value)
      value = value[0]
      return value

   def readVoc(self):
      value = self.char['voc'].read()
      value = struct.unpack('<h', value)
      value = value[0]
      return value

   def readSound(self):
      value = self.char['sound'].read()
      value = struct.unpack('<h', value)
      value = value[0] / 100
      return value

   def readPressure(self):
      value = self.char['pressure'].read()
      value = struct.unpack('<L', value)
      value = value[0] / 1000
      return value



class ExtEnvironSensor(Thing):
    """An external environment sensor which updates every few seconds."""
    global h

    def __init__(self, tbsense):
        Thing.__init__(self,
                       'My Environ Sensor Thing',
                       ['TemperatureSensor', 'MultiLevelSensor']#, 'MultiLevelSensor', 'MultiLevelSensor'],
                       'A web connected environment sensor')
        
        self.tbsense = tbsense
        
        #temperature sensor
        self.temp = Value(0.0)
        self.add_property(
            Property(self, 'temperature', self.temp,
                     metadata={
                                '@type': 'TemperatureProperty',
                                'title': 'Outside Temperature',
                                'type': 'number',
                                'description': 'The level of outside temperature in C',
                                'minimum': -40.0,
                                'maximum': 100.0,
                                'unit': 'Celsius',
                                'readOnly': True,
                              }))
        
        #humidity sensor
        self.humidity = Value(0.0)
        self.add_property(
            Property(self, 'pressure', self.humidity,
                     metadata={
                                '@type': 'LevelProperty',
                                'title': 'Humidity',
                                'type': 'number',
                                'description': 'The level of rel. humidity',
                                'minimum': 0,
                                'maximum': 100,
                                'unit': '%',
                                'readOnly': True,
                              }))

        syslog.syslog('Starting the sensor update looping task')
        self.enviro_task = get_event_loop().create_task(self.update_TbSense())

    async def update_TbSense(self):
        try:
            while True:
                self.temp = self.tbsense.readTemperature()
                self.humidity = self.tbsense.readHumidity()
                await sleep(h)
        except CancelledError:
            pass

    def cancel_tasks(self):
        self.enviro_task.cancel()
        get_event_loop().run_until_complete(self.enviro_task)

def getThunderboard():
    devices = Scanner(0).scan(3)
    tbsense = None
    for dev in devices:
        scanData = dev.getScanData()
        for (adtype, desc, value) in scanData:
            if desc == 'Complete Local Name' and 'Thunder Sense #' in value:
                    deviceId = int(value.split('#')[-1])
                    tbsense = Thunderboard(dev)
                    break
    return tbsense

def run_server():
    syslog.syslog('Looking for a Thunderboard ...')
    while (True):
        tbsense = getThunderboard()
        if tbsense is not None:
            break
        syslog.syslog('Not found, retrying after 5sec ...')
        sleep(5)
    
    sensors = ExtEnvironSensor(tbsense)

    # If adding more than one thing here, be sure to set the `name`
    # parameter to some string, which will be broadcast via mDNS.
    # In the single thing case, the thing's name will be broadcast.
    server = WebThingServer(SingleThing(sensors), port=8899)
    try:
        syslog.syslog('Starting the Webthing server on: ' + str(server.hosts))
        server.start()
    except KeyboardInterrupt:
        sensors.cancel_tasks()
        server.stop()
        syslog.syslog('Webthing server stopped')

if __name__ == '__main__':
    run_server()
