# Zephyr OS USB Non-Standard Class Firmware README

## Description

Welcome to the Zephyr OS USB Non-Standard Class Firmware repository. This project showcases an example firmware implementation for a USB device using the Zephyr OS. The firmware establishes a non-standard USB class, enabling communication with the device via both control messages and bulk transfers.

## Build and flash
Project contain CMake build system, when it's configured to prepare flashing firmware to device `blackpill_f401ce`. To build type:
```bash
west build
```
To flash:
```bash 
west flash
```

## USB Configuration Structure

The heart of this firmware is the `usb_config` structure, meticulously crafted to define the USB interface and its associated endpoints:

```c
struct usb_config {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
    struct usb_ep_descriptor if0_in_ep;
} __packed;
```
* `if0`: This is the USB interface descriptor, responsible for describing the properties of the interface, such as the number of endpoints, the interface class, subclass, protocol, etc.
* `if0_out_ep`: This is the endpoint descriptor for the bulk OUT endpoint (data sent from host to device). It is configured with the address `0x01`.
* `if0_in_ep`: This is the endpoint descriptor for the bulk IN endpoint (data sent from device to host). It is configured with the address `0x81`.

## Communication Modes


### Control Message

The firmware supports communication with the device through control messages. This means that by sending specific setup packets, you can interact with the device. The control messages are leveraged to toggle the state of an LED on the device. The `wValue` field of the setup packet is utilized to set the LED state: sending `0` turns off the LED, while sending `1` turns it on. Additionally, control messages allow sending text messages to the device's buffer - field `data`.

### Bulk Transfer

The firmware also provides a bulk transfer mechanism that operates as a loopback application. When you send data to the bulk IN endpoint (`0x81`), the firmware processes the data and sends it back through the bulk OUT endpoint (`0x01`). Additionally, if you send `0` or `1` through the bulk IN endpoint, the firmware interprets this data as a command to turn off or on the LED on the device, respectively.
 
### LED and Debugging

The device includes a configured LED that can be controlled via control messages. The firmware is integrated with a console via UART to facilitate logging and debugging. This allows you to monitor the device's behavior and interaction with the host.

### Testing Script

Included in this repository is a testing script, tester.py, which enables easy interaction with the device:
```bash
usage: tester.py [-h] [-ctrl] [-blk] [-text TEXT_MSG]

USB communication script

options:
  -h, --help            show this help message and exit
  -ctrl, --control_msg  Send control messages
  -blk, --bulk_msg      Send bulk messages
  -text TEXT_MSG, --text_msg TEXT_MSG
                        Text to send in control messages
```