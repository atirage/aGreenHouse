# aGreenHouse
  Remotely monitor and control certain parameters of a habitat.
First instance was used to control a medium sized greenhouse growing tomatoes using hydroponic culture, but it can be easily used, for say a house/room, so the term "green"(eco) still fits. :)

  The target of this project was to create a tool which could remotely monitor and control certain parameters of a habitat, for ex. monitor air and water temperature, control irrigation pumps and ventilation. The basic idea was to use a Raspberry Pi running ArchLinux ARM for this purpose.
Here are the main functions to be realized, we’ll go through each of them in detail:
●	Sensor acquisition and actuator control
●	Configuration, sensor data and activation history storage
●	Sensor data and activation history visualisation
●	Remote access

# Sensor acquisition and actuator control
  First, let’s start with the list of used sensor and actuator types:
-	1wire water temperature sensor
-	dht22 ambient temperature and humidity sensor
-	water flow sensor
-	relay board
  Basically, everything comes down to accessing the GPIO of the Raspberry. To achieve this the excellent wiringPi library was chosen, which allows user space access through the Linux GPIO driver, but also direct port access by mapping the port address space into the memory map of the running process (using mmap).
There is one exception however, the 1wire temp sensor: in this case a kernel module exists which implements the 1wire protocol and on top of that a specific driver for this device (w1_gpio and w1_therm). This means that we read the temperature in the classic Linux style by opening and reading from a file provided by the above mentioned driver.
  Another tricky thing worth to mention is the reading of dht22. This sensor provides the data after it receives a start sequence, sending the 1s and 0s in a time encoded manner, meaning that we have to measure how long the input pin is at 1 or 0 and we are talking about tens of µseconds. This is not easy to achieve in Linux user space, so the right way should be to write a kernel driver for this device. Nevertheless, after some googling, I found a user space solution as well: using set_setscheduler one can increase the thread priority to a level that the scheduler will not kick it out until the thread relinquishes the processor. In this way we can reliably measure the input waveform in a busy while loop. The drawback is that during the measurement(it lasts ~5 ms) we get a 100% CPU load.
Having covered the HW access, we can move to the next level: we need to decide how to read/control N/M sensors/actuators.  
  A multi-threaded application written in C was the choice, creating a generalized (considering a set of configuration options) function capable of handling one type of device. These functions can be “instantiated” as threads to handle the physically present devices.
Obviously, we don’t want to modify the code if we need to add one or more devices of the already covered types. Also, we need to provide override access for safety reasons and since it’s not intended to have a GUI, IPC was the choice (FIFO pipes) for this purpose. 
All the above leads us to the need of having a static configuration which specifies where the device is attached, how often it is needed to be accessed, etc. 
  Having covered all this, now we can define a basic program flow for our application:
1.	read/validate static configuration
2.	start a thread with corresponding configuration for each found device
3.	enter a wait state waiting for external commands 

# Configuration, sensor data and activation history storage
  To achieve the remote access the obvious choice was a web interface, based on the internet access provided by a 3G USB modem.
The acquisition part is pretty straight-forward: a process writes to an SQLite DB and the web page fetches the data via a php script, visualisation happens using Google charts. For the control part there’s a php form which communicates via IPC(FIFO) with the above mentioned process to perform the actual control.

