ESP-NOW Autopairing Project
============================

This project is based on the master's thesis of the master's degree in Mechatronics Engineering.

The functions that were in charge, for example, of the ESP-NOW connection to the gateway device, as well as the configuration or presetting process and the OTA update process, have been included in a class within the "AUTOpairing.h" library inside core/inc. On the other hand, the operation of the ADC converter has been modeled in the "ADConeshot.h" library. The structure is simpler and allows to modularize the code in different classes that allow to model the behavior of the device in a more efficient way.

- The "AUTOpairing.h" library has been designed to host a class that compose the auto pairing operation of the device to be connected to the gateway and the functions that model the configuration and connection to the server that hosts the file to perform the FOTA update. 

- The "ADConeshot.h" library models the behavior of the analog-to-digital converter. It allows to be configured, to select the channel and several parameters depending on the needs that are required. 
This modeling in classes allows, in short, to modularize and to realize a simpler code, whose functionality is reduced exclusively to what is intended to be realized. The functional code is hidden from the user, so that the user only has to configure the different functional parameters required.

For better code performance, the device configuration file must be properly configured. First, the debugging levels of the code must be decreased, as well as making sure that the debugging level is the minimum (1). 
```
# Log output
#
# CONFIG_LOG_DEFAULT_LEVEL_NONE is not set
CONFIG_LOG_DEFAULT_LEVEL_ERROR=y
# CONFIG_LOG_DEFAULT_LEVEL_WARN is not set
# CONFIG_LOG_DEFAULT_LEVEL_INFO is not set
# CONFIG_LOG_DEFAULT_LEVEL_DEBUG is not set
# CONFIG_LOG_DEFAULT_LEVEL_VERBOSE is not set
CONFIG_LOG_DEFAULT_LEVEL=1
CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT=y
# CONFIG_LOG_MAXIMUM_LEVEL_DEBUG is not set
# CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE is not set
CONFIG_LOG_MAXIMUM_LEVEL=1
CONFIG_LOG_COLORS=y
CONFIG_LOG_TIMESTAMP_SOURCE_RTOS=y
# CONFIG_LOG_TIMESTAMP_SOURCE_SYSTEM is not set
# end of Log output
```
Another option that will improve performance is the one that manages the time in FreeRTOS, i.e. the number of ticks/ms. It must be set to 1000, since it may be originally set to 100.
```
CONFIG_FREERTOS_HZ=1000
```

*Code in this repository is in the Public Domain (or CC0 licensed, at your option.)
Unless required by applicable law or agreed to in writing, this
software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.*
