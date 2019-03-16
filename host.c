#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include <linux/input.h>
#include </usr/include/linux/kd.h>
#include </usr/include/linux/vt.h>
#include </usr/include/linux/input-event-codes.h>

int serial = -1;
int exitOnEscape = 1;

struct keyNameAndCode
{
	char* name;
	short code;
};

#include "keys-teensy.h"
#include "keys-linux.h"

struct keyNameFixup {
	char* src;
	char* dest;
};

struct keyNameFixup keyNameFixups[] =
{
	{ "GRAVE", "TILDE" 		},
	{ "DOT", "PERIOD" 		},
	{ "KPDOT", "KPPERIOD" 		},
	{ "LEFTGUI", "LEFTMETA" 	},
	{ "RIGHTGUI", "RIGHTMETA" 	},
	{ "APOSTROPHE", "QUOTE" 	},
	{ "KPASTERIX", "KPASTERISK" 	},
	{ "SYSRQ", "PRINTSCREEN"	},
	{ "COMPOSE", "MENU"		},
	{ "102ND", "NONUSBS"		},

	{ NULL, NULL }
};

struct keyLookup
{
	unsigned short src;
	unsigned short dest;
};

int doEvent(int kbd, struct keyLookup* keyCache, struct input_event kbdEvent);


struct keyNameAndCode* cleanKeyNames(struct keyNameAndCode* toClean)
{
	int toCleanCount = 0;
	struct keyNameAndCode* nextKey = toClean;
	while(nextKey->name != NULL)
	{
		toCleanCount++;
		nextKey++;
	}

	// Strip out any underscores. This will have the effect that
	// (for example) LEFT_SHIFT becomes LEFTSHIFT, which is what
	// keys.h contains.
	struct keyNameAndCode* keysStripped = malloc(sizeof(struct keyNameAndCode) * toCleanCount);
	for (unsigned int n=0; n<toCleanCount; n++)
	{
		keysStripped[n].name = malloc(strlen(toClean[n].name) + 1);
		keysStripped[n].code = toClean[n].code;
		unsigned int srcPos = 0;
		unsigned int dstPos = 0;
		unsigned char letter;
		while((letter = toClean[n].name[srcPos]) != 0)
		{
			if (letter == '_')
			{
				srcPos++;
				continue;
			}
			keysStripped[n].name[dstPos] = letter;
			srcPos++;
			dstPos++;
		}
		keysStripped[n].name[dstPos] = 0;
	}

	// Some start-of-string-based adjustments.
	// Snip off leading substrings.
	for (unsigned int n=0; n<toCleanCount; n++)
	{
		char* toRemoveList[] = { "MODIFIERKEY", "KEY", NULL };
		char** toRemove = toRemoveList;
		while(*toRemove != NULL)
		{
			if (strncmp(keysStripped[n].name, *toRemove, strlen(*toRemove)) == 0)
			{
				char* newName = malloc( (strlen(keysStripped[n].name) - strlen(*toRemove)) + 1);
				strcpy(newName, &keysStripped[n].name[strlen(*toRemove)]);
				free(keysStripped[n].name);
				keysStripped[n].name = newName;
			}
			toRemove++;
		}
	}
	// We also need to change leading 'KEYPAD' to 'KP'. Since we've already nixed leading 'KEY', all that remains
	// will be the 'PAD'.
	for (unsigned int n=0; n<toCleanCount; n++)
	{
		if (strncmp(keysStripped[n].name, "PAD", strlen("PAD")) == 0)
		{
			char* newName = malloc( (strlen(keysStripped[n].name) - strlen("PAD")) + strlen("KP") + 1);
			strcpy(&newName[           0], "KP");
			strcpy(&newName[strlen("KP")], &keysStripped[n].name[strlen("PAD")]);
			free(keysStripped[n].name);
			keysStripped[n].name = newName;
		}
	}

	// Finally, a few hardcoded things that just won't match otherwise.
	for (unsigned int n=0; n<toCleanCount; n++)
	{
		struct keyNameFixup* fixup = keyNameFixups;
		while(fixup->src != NULL)
		{
			if (strcmp(keysStripped[n].name, fixup->src) == 0)
			{
				free(keysStripped[n].name);
				keysStripped[n].name = malloc(strlen(fixup->dest) + 1);
				strcpy(keysStripped[n].name, fixup->dest);
			}
			fixup++;
		}
	}
	return keysStripped;
}

struct keyLookup* initkeyArray()
{
	struct keyNameAndCode* cleanedLinuxKeys = cleanKeyNames(linuxKeys);
	struct keyNameAndCode* cleanedTeensyKeys = cleanKeyNames(teensyKeys);

	int teensyKeyCount = 0;
	struct keyNameAndCode* tmp = cleanedTeensyKeys;
	while(tmp->name != NULL)
	{
		teensyKeyCount++;
		tmp++;
	}
	struct keyLookup* toReturn = malloc(sizeof(struct keyLookup) * teensyKeyCount);
	memset(toReturn, 0, sizeof(struct keyLookup) * (teensyKeyCount+1));
	int resultsSoFar = 0;

	struct keyNameAndCode* teensyEntry = cleanedTeensyKeys;
	int matched = 0;
	int failed = 0;
	while(teensyEntry->name != NULL)
	{
		struct keyNameAndCode* linuxEntry = cleanedLinuxKeys;
		while(linuxEntry->name != NULL)
		{
			if (strcmp(teensyEntry->name, linuxEntry->name) == 0)
			{
				// is it already in the results?
				int alreadyThere = 0;
				for (unsigned int n = 0; n < resultsSoFar; n++)
				{
					if (toReturn[n].src == linuxEntry->code)
					{
						printf("Supressing map %hu ('%s')->%hu ('%s') because it is already mapped to %hu\n", teensyEntry->code, teensyEntry->name, linuxEntry->code, linuxEntry->name, toReturn[n].dest);
						alreadyThere = 1;
						break;
					}
				}
				if (!alreadyThere)
				{
					toReturn[resultsSoFar].src = linuxEntry->code;
					toReturn[resultsSoFar].dest  = teensyEntry->code;
					resultsSoFar++;
				}
				matched++;
				break;
			}
			linuxEntry++;
		}
		if (linuxEntry->name == NULL)
		{
			printf("Failed to match Teensy keycode %s\n", teensyEntry->name);
			failed++;
		}
		teensyEntry++;
	}
	printf("Matched %d keycodes, failed %d\n", matched, failed);

	return toReturn;
}

struct termios tioold;

void initSerial()
{
	struct termios tionew;

	tcgetattr(serial, &tioold);
	memcpy(&tionew, &tioold, sizeof(struct termios));
	tionew.c_cflag |= B9600 | CS8 | CREAD;

	tionew.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);

	tionew.c_oflag &= ~OPOST;

	tionew.c_iflag &= ~(INPCK | PARMRK | IXON | IXOFF | IXANY | BRKINT | INLCR | IGNCR | ISTRIP);
	tionew.c_iflag |= (IGNPAR | IGNBRK );

	tionew.c_cc[VTIME] = 10;
	tionew.c_cc[VMIN] = 1;
	tcsetattr(serial, TCSANOW, &tionew);
}

void restoreSerial()
{
	tcsetattr(serial, TCSANOW, &tioold);
}

struct terminalState
{
	struct termios termold;
	struct kbd_repeat repold;
	long oldkbmode;
};

void stripTerminal(struct terminalState* terminalState)
{
	struct termios termnew;
	tcgetattr(STDIN_FILENO, &terminalState->termold);
	memcpy(&termnew, &terminalState->termold, sizeof(struct termios));
	termnew.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG );
	termnew.c_iflag &= ~(INPCK  | PARMRK | IXON | IXOFF | IXANY | BRKINT | INLCR | IGNCR | ISTRIP );
	termnew.c_iflag |=  (IGNPAR | IGNBRK);
	termnew.c_oflag |= (OPOST);
	tcsetattr(STDIN_FILENO, TCSANOW, &termnew);

	ioctl(STDIN_FILENO, KDGKBMODE, &terminalState->oldkbmode);
	ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);

	ioctl(STDIN_FILENO, EVIOCGREP, &terminalState->repold);
	struct kbd_repeat repeat;
	repeat.delay = 10000;
	repeat.period = 10000;
	ioctl(STDIN_FILENO, EVIOCSREP, &repeat);
}

void restoreTerminal(struct terminalState* terminalState)
{
	ioctl(STDIN_FILENO, EVIOCSREP, terminalState->repold);
	ioctl(STDIN_FILENO, KDSKBMODE, terminalState->oldkbmode);
	tcsetattr(STDIN_FILENO, TCSANOW, &terminalState->termold);
}

int doEventForDevice(int kbd, struct keyLookup* keyCache)
{
	// Read one event from the given FD, and proces it
	struct input_event kbdEvent;
	memset(&kbdEvent, 0, sizeof(struct input_event));
	int bytesRead = read(kbd, &kbdEvent, sizeof(struct input_event));
	if (bytesRead != sizeof(struct input_event))
	{
		printf("short read of kbd: %d of %d bytes\n", bytesRead, sizeof(struct input_event));
		return 1;
	}
	int res = doEvent(kbd, keyCache, kbdEvent);
	if (res == -1)
	{
		char* typeName;
		switch (kbdEvent.type)
		{
			case EV_SYN:
				typeName = "EV_SYN";
				break;
			case EV_KEY:
				typeName = "EV_KEY";
				break;
			case EV_REL:
				typeName = "EV_REL";
				break;
			case EV_ABS:
				typeName = "EV_ABS";
				break;
			case EV_MSC:
				typeName = "EV_MSC";
				break;
			case EV_SW:
				typeName = "EV_SW";
				break;
			case EV_LED:
				typeName = "EV_LED";
				break;
			case EV_SND:
				typeName = "EV_SND";
				break;
			case EV_REP:
				typeName = "EV_REP";
				break;
			case EV_FF:
				typeName = "EV_FF";
				break;
//			This one isn't defined on my Raspbian 4.14.50+ box.
//			case EV_POWER:
//				typeName = "EV_POWER";
//				break;
			case EV_FF_STATUS:
				typeName = "EV_FF_STATUS";
				break;
			default:
				typeName = "(unknown)";
		}

		printf("Error processing input event: .type=%hu ('%s') .code=%hu .value=0x%08lx\n", kbdEvent.type, typeName, kbdEvent.code, kbdEvent.value);
		return 0;
	}
	return res;

}

int doEvent(int kbd, struct keyLookup* keyCache, struct input_event kbdEvent)
{
	unsigned char toSend[2];

	if (kbdEvent.type == EV_SYN)
	{
		// idk, ignore it
		return 0;
	}
	else if (kbdEvent.type == EV_MSC)
	{
		if (kbdEvent.code == MSC_SCAN)
		{
			// Ignore scancode events
			return 0;
		}
		else
		{
			return -1;
		}
	}
	else if (kbdEvent.type == EV_KEY)
	{
		// First, is it a mouse button? If so, deal with that.
		int isMouseButton = 1;
		switch(kbdEvent.code)
		{
			case BTN_LEFT:
				toSend[1] = 0x00;
				break;
			case BTN_RIGHT:
				toSend[1] = 0x01;
				break;
			case BTN_MIDDLE:
				toSend[1] = 0x02;
				break;
			case BTN_SIDE:
				toSend[1] = 0x03;
				break;
			default:
				isMouseButton = 0;
				break;
		}
		if (isMouseButton)
		{
			if (kbdEvent.value == 0)
				toSend[2] = 0x00;
			else if (kbdEvent.value == 1)
				toSend[2] = 0x01;
			else
				return -1;
			toSend[0] = 0x06;
			write(serial, toSend, 3);
			return 0;
		}

		// OK, not a mouse button. It's probably a keyboard key.
		short up;
		if (kbdEvent.value == 0x01)
		{
			up = 0;
		}
		else if (kbdEvent.value == 0x00)
		{
			up = 1;
		}
		else if (kbdEvent.value == 0x02)
		{
			// This is generated when a key is repeating, due to
			// it being held down (see input/event-codes.txt).
			// I'm going to ignore it here and let the host OS
			// on the remote machine interpret repeating keys.
			// Not sure if this is best, or if I should try to
			// standardize repeat settings over remote machines
			// by having the local machine control repeating..
			return 0;
		}

		struct keyLookup* cursor = keyCache;
		unsigned short teensykey;
		while(cursor->src != 0)
		{
			if (cursor->src == kbdEvent.code)
			{
				teensykey = cursor->dest;
				break;
			}
			cursor++;
		}
		// Error out if not found
		if (cursor->src == 0)
		{
			char* linuxKeyName;
			struct keyNameAndCode* notFound = linuxKeys;
			while(notFound->name != NULL)
			{
				if (kbdEvent.code == notFound->code)
					break;
				notFound++;
			}
			printf("Cannot locate mapping for Linux keycode %hu '%s'\n", kbdEvent.code, notFound->name);
			return -1;
		}

		// If enabled, let the user bail by hitting ESCAPE
		if (exitOnEscape)
		{
			if (kbdEvent.code == KEY_ESC)
				return 1;
		}

		// send the new key value.
		if (up)
			toSend[0] = 0x02;
		else
			toSend[0] = 0x01;
		toSend[2] = teensykey & 0xff;
		toSend[1] = (teensykey >> 8) & 0xff;
		write(serial, toSend, 3);
	}
	else if (kbdEvent.type == EV_REL)
	{
		// Relative mouse movement.
		if (kbdEvent.code == REL_X)
		{
			toSend[0] = 0x03;
		}
		else if (kbdEvent.code == REL_Y)
		{
			toSend[0] = 0x04;
		}
		else if (kbdEvent.code == REL_WHEEL)
		{
			toSend[0] = 0x05;
		}
		else
		{
			printf("Unrecognised EV_REL: %hu 0x%04lx\n", kbdEvent.type, kbdEvent.code, kbdEvent.value);
			return -1;
		}
		toSend[2] = kbdEvent.value;
		toSend[1] = 0;
		write(serial, toSend, 3);
	}
	else
	{
		return -1;
	}

	return 0;
}

struct terminalState origTerm;
int terminalStripped = 0;

void sigIntHandlerFunction(int s)
{
	if (terminalStripped)
		restoreTerminal(&origTerm);
	if (serial != -1)
	{
		restoreSerial();
		close(serial);
	}
	// We leak FDs of opened /dev/input files here. No worries, the kernel will close them :$

	if (s != -1)
		printf("Exiting on signal %d\n", s);

	exit(-1);
}

int openInputFDs(unsigned int**monitoredFDs, unsigned int* monitoredFDCount)
{
	*monitoredFDCount = 0;

	// Open anything matching /dev/input/event*
	DIR* d;
	struct dirent* fileEntry;
	char* inputDirectoryName = "/dev/input";
	d = opendir(inputDirectoryName);
	if (!d)
	{
		int s = errno;
		printf("Failed to open directory %s, errno %d\n", inputDirectoryName, errno);
		return s;
	}
	while( (fileEntry = readdir(d)) != NULL)
	{
		if (memcmp(fileEntry->d_name, "event", strlen("event")) != 0)
			continue;
		(*monitoredFDCount)++;
	}
	rewinddir(d);
	*monitoredFDs = (int*)malloc(sizeof(int) * (*monitoredFDCount));
	memset(*monitoredFDs, 0, sizeof(int) * (*monitoredFDCount));
	unsigned int n = 0;
	int atLeastOneFailure = 0;
	while( (fileEntry = readdir(d)) != NULL)
	{
		if (memcmp(fileEntry->d_name, "event", strlen("event")) != 0)
			continue;
		errno = 0;

		int fullPathLen = strlen(inputDirectoryName) + 1 + strlen(fileEntry->d_name) + 1;	// directory, slash, filename, terminating null
		char* fullPathName = malloc(fullPathLen);
		snprintf(fullPathName, fullPathLen, "%s/%s", inputDirectoryName, fileEntry->d_name);
		(*monitoredFDs)[n] = open(fullPathName, O_NONBLOCK);
		free(fullPathName);

		if ((*monitoredFDs)[n] == -1)
		{
			printf("Failed to open device '%s', errno %d\n", fileEntry->d_name, errno);
			atLeastOneFailure = 1;
		}
		else
		{
			printf("Monitoring %s as FD %d (%d)\n", fileEntry->d_name, n, (*monitoredFDs)[n]);
		}
		n++;
	}
	if (atLeastOneFailure)
	{
		for(unsigned int n=0; n<(*monitoredFDCount); n++)
		{
			if ((*monitoredFDs)[n] != -1)
				close((*monitoredFDs)[n]);
		}
		free(*monitoredFDs);
		return 0;
	}

	return 1;
}

int main(int argc, char** argv)
{
	int malformedCommandLine = 0;
	if (argc == 2)
	{
		if (strcmp(argv[1], "-noescape") == 0)
		{
			exitOnEscape = 0;
		}
		else
		{
			malformedCommandLine = 1;
		}
	}
	if (argc > 2)
		malformedCommandLine= 1;
	if (malformedCommandLine)
	{
		printf("Keyboard/mouse to RS232 redirector, host end\n");
		printf("Usage: %s [-noescape]", argv[0]);
		printf("\t-noescape: do not exit on ESCAPE\n");
		printf("\n\
This tool will take over your terminal quite aggressively - even\n\
CTRL-ALT-DEL will no longer be recognised. In an emergency, you \n\
can kill it from a non-local session, or if that fails, use the \n\
magic sysreq key (if configured in your kernel) to restore the  \n\
terminal mode (alt-sysreq, r). Then you can ctrl-alt-f2 and kill\n\
this tool without messing up your terminal.\n");
		return -1;
	}

	struct keyLookup* keyLookupCache = initkeyArray();

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigIntHandlerFunction;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGTERM, &sigIntHandler, NULL);
	sigaction(SIGSEGV, &sigIntHandler, NULL);

	unsigned int* monitoredFDs;
	unsigned int monitoredFDCount;
	if (!openInputFDs(&monitoredFDs, &monitoredFDCount))
		return -1;

	stripTerminal(&origTerm);
	terminalStripped = 1;

	serial = open("/dev/ttyAMA0", O_RDWR);
	if (serial == -1)
	{
		printf("Can't open serial port\n");
		sigIntHandlerFunction(-1);
	}
	printf("Serial device opened OK\n");
	initSerial();

	int timeToQuit = 0;
	while(1)
	{
		fd_set inputDevices;
		FD_ZERO(&inputDevices);
		int highestfd = 0;
		for(unsigned int n=0; n<monitoredFDCount; n++)
		{
			FD_SET(monitoredFDs[n], &inputDevices);
			if (monitoredFDs[n] > highestfd)
				highestfd = monitoredFDs[n];
		}

		int s = select(highestfd + 1, &inputDevices, NULL, NULL, NULL);
		if (s == -1)
		{
			printf("Failed to select() on input FDs: errno %d\n", errno);
			sigIntHandlerFunction(-1);
		}
		for(unsigned int n=0; n<monitoredFDCount; n++)
		{
			if (FD_ISSET(monitoredFDs[n], &inputDevices))
			{
				timeToQuit = doEventForDevice(monitoredFDs[n], keyLookupCache);
				if (timeToQuit)
					sigIntHandlerFunction(-1);
			}
		}
	}
}
