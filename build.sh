
# Install the Arduino and Teensy tools
wget -q https://downloads.arduino.cc/arduino-1.8.8-linux64.tar.xz
tar -Jxf arduino-1.8.8-linux64.tar.xz
./arduino-1.8.8/install.sh >/dev/null
wget -q https://www.pjrc.com/teensy/td_145/TeensyduinoInstall.linux64
chmod +x TeensyduinoInstall.linux64
./TeensyduinoInstall.linux64 --dir=`pwd`/arduino-1.8.8

# Now we can build the host-side C stuff
./genkeys.sh
./genkeys-teensy.sh `pwd`/arduino-1.8.8
gcc host.c -o host
chmod +x ./host

# And then the teensy code.
./arduino-1.8.8/arduino --board teens:avr:teensy2:speed=16,usb=hid,keys=en-gb -v -pref build.path=usbext-build usbext/usbext.ino

ls host usbext-build/usbext.ino.hex