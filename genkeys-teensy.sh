
if [[ $# -ne 1 ]]; then
	echo "Usage: $0 <path to arduino installation>"
	echo "eg. $0 /cygdrive/c/Program\ Files\ \(x86\)/Arduino/"
	exit -1
fi

inpath="${1}/hardware/teensy/avr/cores/teensy/keylayouts.h"

echo > keys-teensy.h

echo struct keyNameAndCode teensyKeys[] = >> keys-teensy.h
echo { >> keys-teensy.h

grep \#define\ [A-Z_]*\.*\(\ \*[0-9a-fA-Fx]\*\ \*\|\ \*[0-9a-fA-Fx]\*\ \) "${inpath}" | tr "\t" " " | \
sed -s 's/#define \([A-Z0-9_]*\) *\([^ ][A-Fa-f0-9x \|]* *.\)$/\t{ .name = \"\1\", .code = \2 },/g' >> keys-teensy.h

echo -e \\t{ NULL, 0 } >> keys-teensy.h
echo }\; >> keys-teensy.h
