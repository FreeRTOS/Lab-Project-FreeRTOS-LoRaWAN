Reference implementation of LoRaWAN connectivity on FreeRTOS. The project is a FreeRTOS adaptation of Semtech's LoRaMAC Node implementation.
# Getting Statted

## Class A demo

Class-A offers low-powered communincation between the end device and LoRa Network Server. Its the most common use-case and should be implemented by all end-devices supporting LoRaWAN. TheThings Network (TTN) [diagrams and summaries](https://www.thethingsnetwork.org/docs/lorawan/classes.html) shows the communication between end-device and Network Server in Class A. All communications are initiated by an end-device sending uplink message at any time to the server. End-device opens two receiver slot windows after a successful uplink. Server can choose to send any downlink packets during this window. Both end-device and server can alss attache MAC layer commands (used for control activities) along with their uplink and downlink messages. Both uplink and downlink messages can be either confirmed (an ACK is required from the other party) or unconfirmed (no-ACK required).

Demo shows a working example of a common class A application. It spawns two tasks:

### LoRaMAC task
This is a higher priority background task which initializes LoRaMAC stack and waits for any events from Radio layer or MaC layer. All events generated from radio layer is through interrupts. Events are passed using FreeRTOS task notifications which unblocks the LoRaMAC task. Since the task serves interrupt events it runs at a higher priority than other task.

**Note:** Since the radio interrupt handler uses FreeRTOS API for task notifications, the priority for radio interrupts should be set less than or equal to  `configMAX_SYSCALL_INTERRUPT_PRIORITY` as mentioned in FreeRTOS doc [here](https://www.freertos.org/a00110.html#kernel_priority). This means an interrupt can be delayed due to FreeRTOS kernel code execution.

## LoRaWAN Class A task
Task behaves like a common Class A application. It sends an uplink message periodically at an interval configured to follow the fair access policy defined for a LoRaWAN network and region. There are also other parameters like data rate, SF, bandwidth, payload length etc. which needs to be tuned based on how far is the device from gateway, how many devices are connecting to gateway, application requirements etc. If MAC layer indicates that an uplink needs to be send to flush out any pending responses to MAC commands from server, then it sends an empty uplink immediately. If a frame loss is detected by MAC layer, then it triggers a re-join procedure to reset the frame counters.

All events from MAC layer to application are sent using light weight task notifications. LoRaWAN allows multiple requests for the server to be piggy-backed to an uplink message. The responses to these requests are received by application in order using a queue. A downlink queue exists in case application wants to read multiple payloads received at once, before sending an uplink payload.

## Low Power Mode
An important feature of class A based communication is it consumes less power which leads to prolonged batery life. Low power mode for the demo can be enabled using FreeRTOS tickless idle feature as describe [here](https://www.freertos.org/low-power-tickless-rtos.html). Tickless idle mode can be enabled by providing a board specific implementation for `portSUPPRESS_TICKS_AND_SLEEP()` macro and setting `configUSE_TICKLESS_IDLE` to the appropirate value in `FreeRTOSConfig.h`. Enabling tickless mode allows MCU to sleep when the tasks are idle, but be waken up by an interrupt from the radio. 

## Supported Platforms
Vendor | MCU | LoRa Radios | IDE 
|----|----|----|----
Nordic | NRF52840-DK | SX1262MB2CAS | Segger Embedded Studio (SES)
STMicroelectronics | STM32L4 Discovery Kit IoT Node  | SX1276MB1LAS | System Workbench for STM32

## Downloading the Code
The demo leverages open-source [FreeRTOS kernel and libraries](https://github.com/aws/amazon-freertos) and sligthly patched version of open source
[LoRaMac-node v4.4.4](https://github.com/Lora-net/LoRaMac-node/tree/v4.4.4).

1) Download the repository along with the dependent repositories:
```
git clone --recurse-submodules git@github.com:FreeRTOS/Lab-Project-FreeRTOS-LoRaWAN.git
```
2) Apply the patch on top of LoRaMac-node v4.4.4

```
cd Lab-Project-FreeRTOS-LoRaWAN
git apply --whitespace=fix FreeRTOS-LoRaMac-node-v4_4_4.patch
```

## Setting up IDE and Project

### Nordic NRF52840

1) Download and install Segger Embedded Studio IDE for your operating system, by visting the page [here](https://www.segger.com/downloads/embedded-studio/)

2) Open the IDE, choose `File` from menu and select `Open Solution`. Choose `Lab-Project-FreeRTOS-LoRaWAN\demos\classA\Nordic_NRF52\classa_demo.emProject` and click `Open`.

### STMicroElectronics STM32L4 Discovery Kit IoT Node
1) Download and install System Workbench IDE for your operating system, by visting the page [here](https://www.st.com/en/development-tools/sw4stm32.html)

2) Open the IDE, choose `File` from menu and then select `Open Projects From File System`.

3) Choose directory path `Lab-Project-FreeRTOS-LoRaWAN\demos\classA\STM32L475_Discovery`, then choose the selected project and click on `Finish`.


## Create a TTN account, TTN Appication, and register your device
1) Navigate to [TTN home page](https://www.thethingsnetwork.org/).
2) Create a free account by clicking on `Signup` at the top-right, and following its steps.
3) Login to your TTN account. You can do this at TTN homepage at top-right.
4) Click on your account icon at the top-right, and in the submenu select `Console`, then select `Applications`.
6) From here, you can follow TTN's guide for [Adding a TTN Device and Creating a TTN Application](https://www.thethingsnetwork.org/docs/devices/registration.html).

## Configure device credentials

Class A demo uses Over The Air Activation for which three credentials `Device EUI` , `Join EUI` and `Application Key` are required. For demonstration purposes, these credentials are configured statically in the code, however for production use case this can be replaced by a secure element.

To configure these credentials statically in code, goto the file `demos/classA/common/credentials.c` and update the following variables.

Variable | Description | Value Format
----|----|----
`devEUI` | DevEui already registered with TTN or one assigned by TTN. | Array of 8 byte hex values in big-endian ordering
`joinEUI`| Application EUI for TTN Application.| Array of 8 byte hex values in big-endian ordering
`appkey` | Application Key for TTN Application.| Array of 16 byte hex values in big-endian ordering

## Build and Run the Code

### Nordic NRF52840-DK
1) Plug in the SX1262MB2CAS shield on the top of Nordic board.
2) Connect the appropriate antenna for the frequency plan to the shield.
3) Connect your nrf52840-dk board to a PC using usb cable.
4) In SES `Build` Menu, select `Build classa_demo`.
5) In SES `Debug` Menu, select `Go`. This will flash and run the demo in debug mode.

There is a breakpoint at the beginning of the program. When you're ready, you can continue exectution.
The UART output will be shown in the IDE's `Debug Terminal`.

### STMicroelectronics STM32L4 Discovery Kit IoT Node
1) Plug in the SX1276MB1LAS shield on the top of STM32L4 Discovery board.
2) Connect the appropriate antenna for the frequency plan to the shield.
3) Connect your STM32L4 board to a PC using usb cable.
4) From System WorkBench IDE, choose `Project` and then `Build All` or press `CTRL-B`.
5) Once build is completed successfully, click on `Debug` button. This will flash and run the demo in debug mode.
6) It halts at a breakpoint in `main`. Click on `Resume` button.
7) You can view output on the UART console using any serial terminal application.

## View your device traffic in TTN
From TTN `Applications` page, select your application. In the application submenu, click on `Data`. 
Here you see all valid traffic interfacing your TTN Application.
