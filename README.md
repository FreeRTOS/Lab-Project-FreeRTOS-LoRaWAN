Reference implementation of LoRaWAN applications on FreeRTOS

# Class A demo

Class-A offers low-powered ALOHA based communincation between the end device and LoRa Network Server. Its the most common use-case and should be implemented by all end-devices supporting LoRaWAN. TheThings Network (TTN) [diagrams and summaries](https://www.thethingsnetwork.org/docs/lorawan/classes.html) shows the communication between end-device and Network Server in Class A. All communications are initiated by an end-device sending uplink message at any time to the server. End-device opens two receiver slot windows after a successful uplink. Server can choose to send any downlink packets during this window. Both end-device and server can alss attache MAC layer commands (used for control activities) along with their uplink and downlink messages. Both uplink and downlink messages can be either confirmed (an ACK is required from the other party) or unconfirmed (no-ACK required).


Demo shows a working example of a common class A application. It spawns two tasks:

**LoRaMAC task:** This is a higher priority background task which initializes LoRaMAC stack and waits for any events from Radio layer or MaC layer. All events generated from radio layer is through interrupts. Events are passed using FreeRTOS task notifications which unblocks the LoRaMAC task. Since the task serves interrupt events it runs at a higher priority than other task.

**Note:** Since the radio interrupt handler uses FreeRTOS API for task notifications, the priority for radio interrupts should be set less than or equal to  `configMAX_SYSCALL_INTERRUPT_PRIORITY` as mentioned in FreeRTOS doc [here](https://www.freertos.org/a00110.html#kernel_priority). This means an interrupt can be delayed due to FreeRTOS kernel code execution.

**LoRaWAN Class A task:** Task behaves like a common Class A application. It sends an uplink message periodically at an interval configured to follow the fair access policy defined for a LoRaWAN network and region. There are also other parameters like data rate, SF, Bandwidth, payload length etc. which needs to be tuned based on how far is the device from gateway, how many devices are connecting to gateway, application requirements etc. If MAC layer indicates that an uplink needs to be send to flush out any pending responses to MAC commands from server, then it sends an empty uplink immediately. If a frame loss is detected by MAC layer, then it triggers a re-join procedure to reset the frame counters.

All events from MAC layer to application are exchaged using light weight task notifications. LoRaWAN allows multiple requests for the server to be piggy-backed to an uplink message. The responses to these requests are received by application in order using a FreeRTOS queue. A downlink queue exists in case application wants to receive multiple items without sending an actual payload uplink (by sending empty uplinks).

**Low Power Mode:** An important feature of class A based communication is it consumes less power which leads to prolonged batery life. Low power mode for the demo can be enabled using FreeRTOS tickless idle feature as describe [here](https://www.freertos.org/low-power-tickless-rtos.html). Tickless idle mode can be enabled by providing a board specific implementation for `portSUPPRESS_TICKS_AND_SLEEP()` macro and setting `configUSE_TICKLESS_IDLE` to the appropirate value in `FreeRTOSConfig.h`. Enabling tickless mode allows MCU to sleep when the tasks are idle, but be waken up by an interrupt from the radio. 

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
