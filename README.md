# WiFi AP with simple httpd server and UART communication

The Example consists of HTTPD server providing next URI:
    1. GET URI \ returns Device start page
    2. GET URI \query\\* returns data from STM32 board (via UART) [STM32 Project](https://github.com/oktilon/stm-den)
    3. GET URI \led\\* command to switch LED pin ON/OFF

### Hardware Required

* A development board with ESP32/ESP32-S2/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming

### Update index.h file with web page

To update index.h from index.html:
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