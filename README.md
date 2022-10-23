# Simple HTTPD Server Example

The Example consists of HTTPD server demo with demostration of URI handling :
    1. URI \ for GET command returns Device start page
    2. URI \hello for GET command returns "Hello World!" message
    3. URI \echo for POST command echoes back the POSTed message

## How to use example

### Hardware Required

* A development board with ESP32/ESP32-S2/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming

### Configure the project

```
idf.py menuconfig
```
* Open the project configuration menu (`idf.py menuconfig`) to configure Wi-Fi or Ethernet.

To update index.h by index.html:
```
 python make.py
 cd main
 xxd -i index.htm > index.h
 ```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)