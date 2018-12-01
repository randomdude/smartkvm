echo > keys-teensy.h

/bin/echo -e struct teensykey\\n{\\nchar* name\;\\n\
unsigned short code\;\\n\
}\;\\n >> keys-teensy.h
/bin/echo -e struct teensykey teensykeys[] = \\n{ >> keys-teensy.h

#egrep \#define\ \(MODIFIER\)\?KEY.\*\(\ \*[0-9a-fA-Fx]\*\ \*\|\ \*[0-9a-fA-Fx]\*\ \) teensy-keylayouts.h | tr "\t" " " | \
grep \#define\ [A-Z_]*\.*\(\ \*[0-9a-fA-Fx]\*\ \*\|\ \*[0-9a-fA-Fx]\*\ \) teensy-keylayouts.h | tr "\t" " " | \
sed -s 's/#define \([A-Z0-9_]*\) *\([^ ][A-Fa-f0-9x \|]* *.\)$/{ .name = \"\1\", .code = \2 },/g' >> keys-teensy.h

echo { NULL, 0 } >> keys-teensy.h
echo }\; >> keys-teensy.h
