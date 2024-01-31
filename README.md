<h1> Autoconfigurable wireless sensor network framework for data fusion at the edge </h1>

![Badge en Desarollo](https://img.shields.io/badge/STATUS-EN%20DESAROLLO-green)
![GitHub Org's stars](https://img.shields.io/github/stars/camilafernanda?style=social)

Class designed in C++ to provide automatic connection to multiple ESP32 devices using ESP-NOW. The programming of this infrastructure allows the automatic and fast connection of multiple devices of the Espressif family with minimum consumption. This infrastructure will allow the study of data acquired by the ADC as well as its data fusion at the edge, focusing on a decentralized paradigm.

## 🔨 Infrastructure functionalities 

- `Automatic pairing`: Thanks to the ESP-NOW connection protocol and using the designed class, we can quickly pair to a gateway or central node, as well as to other devices belonging to our personal area network in order to exchange messages or data.
- `Efficient connectivity to WiFI`: WiFi usage has been reduced to a minimum to save as much energy as possible. We have opted for the radio protocol built into the Espressif devices and have left the WiFi option for other services outside of messaging.
- `Fast and efficient coding`: The code is entirely programmed in C++ and compact in one class, using real-time programming tools (FreeRTOS) to optimize the device's resource savings. This feature facilitates programming since the programmer will only have to add some configuration information and put his code into operation.
- `OTA update capability`: The device is capable of receiving over-the-air updates, since when it receives a request via MQTT for an update, it performs the update routine.

## 📁 Project organization
