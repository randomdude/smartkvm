FROM ubuntu:xenial

RUN apt-get update
RUN apt-get upgrade -y

RUN apt-get install -y build-essential linux-libc-dev wget

# The teensyduino installer requires all this crap.
RUN apt-get install -y libfontconfig1 libx11-6 libxft2

COPY * /root/

WORKDIR /root/
CMD /bin/bash /root/build.sh

