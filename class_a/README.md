# Class-A Demo

Class-A offers low-powered ALOHA based communincation between the end device and LoRa Network Server. Its the most common use-case and should be implemented by all end-devices supporting LoRaWAN. TheThings Network (TTN)  [diagrams and summaries](https://www.thethingsnetwork.org/docs/lorawan/classes.html) shows the communication between end-device and Network Server in Class A. 
In the default demo state, the device will uplink a _confirmed_ status byte every 5 seconds, and continue to re-send until receiving a server _Confirmed_ response.
Meanwhile, any downlink data received from the server will be printed out from device.

# Dependencies
Usage | Item
----|----
MCU| nrf42840-dk 
Radio | sx1262mb2cas (915 MHz)
IDE | Segger Embedded Studio (SES)


# Running the Demo
### Download and View the Code
The demo leverages open-source [FreeRTOS Libraries](https://github.com/aws/amazon-freertos) and 
a slightly altered fork of [LoraMac-node stack](https://github.com/dachalco/LoRaMac-node).

1) Download this repo as well as used libraries:
```
git clone --recurse-submodules git@github.com:ravibhagavandas/FreeRTOS-LoRaWAN.git
```

2) Now enter the SES IDE, select `Open Solution`, and open `FreeRTOS-LoRaWAN/projects/nordic_nrf52/lorawan_demo.emProject`.

### Create a TTN account, TTN Appication, and Add Your Device
1) Navigate to [TTN home page](https://www.thethingsnetwork.org/).
2) Create a free account by clicking on `Signup` at the top-right, and following its steps.
3) Login to your TTN account. You can do this at TTN homepage at top-right.
4) Click on your account icon at the top-right, and in the submenu select `Console`, then select `Applications`.
6) From here, you can follow TTN's guide for [Adding a TTN Device and Creating a TTN Application](https://www.thethingsnetwork.org/docs/devices/registration.html).

### Configure Application Settings

Variable/Macro | Default | Description 
----|----|----
`DEV_EUI` | EMPTY | DevEui given or assigned by TTN. Big-Endian
`JOIN_EUI`| EMPTY | AppEui for TTN Application Endpoint
`APP_NWK_KEY` | EMPTY | AppKey for TTN Application Endpoint

Before building the code, you'll need to supply your TTN device parameters for the above macros.

### Build and Run the Code
1) Connect your nrf52840-dk.
2) In SES `Build` Menu, select `Build lorawan_demo`.
3) In SES `Debug` Menu, select `Go`. This will flash and run the demo in debug mode.

There is a breakpoint at the beginning of the program. When you're ready, you can continue exectution.
The UART output will be shown in the IDE's `Debug Terminal`.

### View your device traffic in TTN
From TTN `Applications` page, select your application. In the application submenu, click on `Data`. 
Here you see all valid traffic interfacing your TTN Application.
