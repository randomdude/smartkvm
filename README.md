# About #

Got more than one monitor? More than one machine? Want a dual-head KVM that doesn't cost the earth and supports fancy features? If so, then this is for you!

This project builds on 'keyboardMouseExtender', which allows you to send keyboard and mouse data via RS284 to a host. This version is enhanced with features that make it more useful to multi-head setups. It'll even control more than two KVMs - simply add a USB Teensy 2.0 board for each KVM.

# Use

Hit SCROLL LOCK twice, followed by a two-digit head number, and then RETURN. Kapow, both KVMs will be set to the head you desire! You can also map your KVMs arbitrarily - so, for example, selecting inputs 1-4 could put your main monitor on machines 1-4, but set your secondary monitor to input 9. Or you could set input 1 to select input 2 on the first KVM and input 3 on the second KVM. The world's your oyster.

# Setup

Set up a single head as usual (see 'keyboardMouseExtender'). Once that works, attach a second Teensy in parallel with the first. Connect it to a second KVM via USB. Place a jumper on pin 0 of the Teensy, and set it to 0 for one head, and 1 for another. Open usbext.ino, and configure KVM mappings:
```
unsigned int heads[][16] = {
  {  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16 },
  {  1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 },
};
```

By default, the code will communicate with a KVM and change head by sending the sequence of two SCROLL LOCK presses, and then the number of the head desired. If your KVM is different, also change:
```
// If your KVM uses a keystroke other than two taps of SCROLL LOCK to change head, change this.
#define KVM_CMD_SEQ_LEN 2
unsigned int kvm_cmd_seq[KVM_CMD_SEQ_LEN] = {KEY_SCROLL_LOCK, KEY_SCROLL_LOCK }
```

With that done, build as per below.

# Building (hardware) #

Acquire:
 * Two MAX485-based TTL-to-rs485 boards
 * One Teensy 2.0 for each KVM

On the keyboard side, attach a TTL-to-RS485 board to the serial UART of your host PC. I used the ttl-level uart of a raspberry pi. Note that the data flow is one-directional, so you only need to connect the pin going out of the pi to DI. Connect power and gnd, and then connect a 10K resistor with one end at vcc and the other end connecting to both RE and DE, which will place this board in transmit mode. Lay two long wires from this board your server room and attach them to the A and B rs485 outputs.

In the server room, take your other RS485-to-TTL reciever board. Jumper RE and DE to 0v (to put the board in receive mode). Connect gnd and vcc to the power pins on the Teensy (the top ones). Connect the RO pin to pin 7 ('D2') on your Teensy. Wire a jumper on pin 0 (B2) of the teensy (10k pull up and jumper to ground). Repeat this for all your Teensy boards, wiring them all to the same vcc/gnd. Finally, set the jumper you installed to open on head 0 and closed on head 1. Et voila!

# Building (software) #

## Easy mode - use Docker ##

Just use Docker to build/run the container. Binaries will be in the container at /root/host (for the Linux-based host side) and /root/usbext-build/usbext.ino.hex (for the Teensy firmware).

```bash
docker build -t tmp .
docker run --name tmp tmp
docker cp tmp:/root/host ./host
docker cp tmp:/root/usbext-build/usbext.ino.hex .
```

If you have problems, take a look at the Jenkinsfile - the procedure in there should always work, so if it doesn't, raise a bug.

## PITA mode - do it yourself ##

First, install:

* The Arduno IDE: https://www.arduino.cc/en/Main/Software, or `choco install -y arduino`
* Teensy tools: https://www.pjrc.com/teensy/td_download.html

Open the .ino in the Arduino IDE, build it, and upload the resulting .hex file to your Teensy.

Then, build the Linux host sw. You'll need to build from Linux.

* Run 'genkeys.sh', to generate keys-linux.h
* Run 'genkeys-teensy.sh', passing it your arduino install path, which will togenerate keys-teensy.h
* Compile it:
  gcc host.c -o host
  chmod +x host
* Run it:
  ./host

Sorted. If it doesn't work, take a look at the Dockerfile to work out what's wrong, and raise a bug either to fix the Dockerfile or update this documentation.
