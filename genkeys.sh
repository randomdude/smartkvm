grep define /usr/include/linux/input-event-codes.h | grep KEY_ | awk '{ print "KEYS[" $3 "] = \"" $2 "\";" }' > keys.h
