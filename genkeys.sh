
echo > keys-linux.h

echo struct keyNameAndCode linuxKeys[] = >> keys-linux.h
echo { >> keys-linux.h

grep define /usr/include/linux/input-event-codes.h | egrep "^#define KEY_[A-Z0-9_]*\s*(0x)?[0-9a-fA-F]*(\s*/\*.*\*/(\w*?))?$" | awk '{ print "\t{ .name = \"" $2 "\", .code = " $3 " }," }' >> keys-linux.h

echo -e \\t{ .name = NULL, .code = 0 } >> keys-linux.h
echo }\; >> keys-linux.h
