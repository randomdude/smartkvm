wget https://downloads.arduino.cc/arduino-1.8.8-linux64.tar.xz
tar -Jxf arduino-1.8.8-linux64.tar.xz
./arduino-1.8.8/install.sh >/dev/null

wget https://www.pjrc.com/teensy/td_145/TeensyduinoInstall.linux64
chmod +x TeensyduinoInstall.linux64
./TeensyduinoInstall.linux64 --dir=`pwd`/arduino-1.8.8

./genkeys.sh
./genkeys-teensy.sh `pwd`/aruino-1.8.8

gcc host.c -o host
chmod +x ./host

tar -czvf - host
