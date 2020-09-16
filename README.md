# Car-Watcher - Contiki-ng OS application
Car-Watcher is application that takes care of all your vehicles that you rent/lend to other people.  
Contiki-ng Car-Watcher client application is collecting vehicle data and sending it to car_watcher platform where it is processed and saved.  


## This project consists of 3 folders:  
- contiki_ng - contains Car-Watcher Contiki-ng app code
- nodejs_test_client - contains code for Nodejs app to test functionalities of Contiki-ng app
- rabbitmq - contains Dockerfile for RabbitMQ image that is used as a broker between Contiki-ng app and backend

## Contiki-ng app setup
Use Docker for running the app.  
Download the official image from Docker Hub ( https://hub.docker.com/r/contiker/contiki-ng/tags ).

Clone official repository for Contiki-ng ( https://github.com/contiki-ng/contiki-ng.git ) and run the following command to start a contiki docker container ( as described in docs https://github.com/contiki-ng/contiki-ng/wiki/Docker ): docker run --privileged --sysctl net.ipv6.conf.all.disable_ipv6=0 --mount type=bind,source=$CNG_PATH,destination=/home/user/contiki-ng -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v /dev/bus/usb:/dev/bus/usb -ti contiker/contiki-ng  

Copy app code in examples folder ( .contiki-ng/examples/app ), attach to docker container and navigate to application folder. To build application code run: `make target=native`. After build, run `sudo car_watcher.native` to start the application.

## About Car-Watcher
Contiki Car-Watcher app collects data about vehicle and sends it as JSON to rabbitmq. Example of data sent:  
```js
{
  "event": 1,
  "data": {
    "Platform": 'native',
    "Seq": 1,
    "sens_data": {
      "temperature": 115,
      "speed": 113,
      "rpm": 3335,
      "gas": 26,
      "battery_charge": 10.4
    }
  }
}
```
All defined sensors are located in sensors folder. sensor.h file contains interface definition that every sensor should implement in order to be used in Car-Watcher app
```cpp
typedef struct isensor
{
    void    (*init)(void);          // callback for initialization of a sensor
    float   (*value)(void);         // callback for reading value of a sensor
    char*   (*sensor_name)(void);   // callback that reaturns unique name of a sensor in a system
    sens_constr* constraint;        // constraint for values of sensor reading 
} isensor;
```
Also sensor.h contains definition for some basic operations to work with defined sensors.

Contiki Car-Watcher can communicate with some server using some broker that supports MQTT protocol (in this example rabbitmq). Messages sent to Contiki client should have the following form: 
```js
{
    event: msg_num
    // Add aditional data fields if needed
};
```
Every message needs to have message number field set that determines what Contiki client should do. List of all currently supported message numbers is defined in car-watcher.h file which include start/stop sending readings data, sending a warning to driver, changing reading interval.

For receiving messages, it registers a `mqtt_event` callback function that is used for handling mqtt messages.
```cpp
mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event, MAX_TCP_SEGMENT_SIZE);
```

After receiving a message, Car-Watcher processes and confirms that message was received. In case of more demanding processing a job_request is created and message is confirmed. 
```cpp
typedef struct job_request
{
    void (*func)(void *);
    void* argv;
} job_request;
```
All created job_requests are stored in job_list. Main process executes this list after every event (it is possible to define etimer that will trigger events for this processing).

Upon startup, Contiki client initializes  sensors, mqtt configuration and job list, connects to broker, defines callback for received messages and starts sending data.

## Setup Rabbitmq broker
Go to rabbitmq folder and build Dockerfile contained in that folder `docker build -t rabbitmq_mqtt .` .
Run rabbitmq_start.sh shell script to create docker container for created image.

## Setup Nodejs test client
Go to nodejs_test_client folder and run `npm install`.
Run `npm run` or `node index.js` to start test client.

## Further development
- improve isensor interface to support various types of data readings (currently only supports float)
- define etiemr for job_list processing