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

#include "keys-teensy.h"

int serial = -1;

unsigned char* KEYS[KEY_MAX];

void initkeyArray()
{
	for (unsigned int n=0; n<KEY_MAX; n++)
		KEYS[n] = "(unknown)";
	#include "keys.h"
}

struct termios tioold;

void initSerial()
{
	struct termios tionew;

	tcgetattr(serial, &tioold);
	memcpy(&tionew, &tioold, sizeof(struct termios));
	tionew.c_cflag |= B9600;
	tionew.c_cflag |= CS8;
	tionew.c_cflag |= CREAD;
	tionew.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
	tionew.c_oflag &= ~OPOST;
	tionew.c_iflag &= ~(INPCK | PARMRK | IXON | IXOFF | IXANY | BRKINT | IGNCR);
	tionew.c_iflag |= (IGNPAR | IGNBRK );
	tionew.c_cc[VTIME] = 10;
	tionew.c_cc[VMIN] = 1;
	tcsetattr(serial, TCSANOW, &tionew);
}

void restoreSerial()
{
	// eh, why bother
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
	termnew = terminalState->termold;
	termnew.c_lflag &= ~(ECHO | ICANON | ISIG | ISTRIP | INLCR | IGNCR | IXON | IXOFF );
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

int doEvent(int kbd)
{
	unsigned char toSend[2];

	struct input_event kbdEvent;
	memset(&kbdEvent, 0, sizeof(struct input_event));
	int bytesRead = read(kbd, &kbdEvent, sizeof(struct input_event));
	if (bytesRead != sizeof(struct input_event))
	{
		printf("short read of kbd: %d of %d bytes\n", bytesRead, sizeof(struct input_event));
		// todo
	}
//	printf("%hu %hu 0x%08lx\n", kbdEvent.type, kbdEvent.code, kbdEvent.value);

	if (kbdEvent.type == INPUT_PROP_POINTER)
	{
		// idk lol
		return 0;
	}
	else if (kbdEvent.type == EV_MSC)
	{
		// idk lol
		return 0;
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
			up = 0;
		else if (kbdEvent.value == 0x00)
			up = 1;
		else
			return -1;

		char* chName = KEYS[kbdEvent.code];
		// Translate to the keycodes that the Teensy uses
		// fix up modifier keys, since the teensy syntax is slightly different
	if (strcmp(chName, "KEY_RIGHTSHIFT") == 0)
		chName = "MODIFIERKEY_RIGHT_SHIFT";
	else if (strcmp(chName, "KEY_LEFTSHIFT") == 0)
		chName = "MODIFIERKEY_LEFT_SHIFT";
	else if (strcmp(chName, "KEY_RIGHTCTRL") == 0)
		chName = "MODIFIERKEY_RIGHT_CTRL";
	else if (strcmp(chName, "KEY_LEFTCTRL") == 0)
		chName = "MODIFIERKEY_LEFT_CTRL";
	else if (strcmp(chName, "KEY_RIGHTALT") == 0)
		chName = "MODIFIERKEY_RIGHT_ALT";
	else if (strcmp(chName, "KEY_LEFTALT") == 0)
		chName = "MODIFIERKEY_LEFT_ALT";
	else if (strcmp(chName, "KEY_CAPSLOCK") == 0)
		chName = "KEY_CAPS_LOCK";
	else if (strcmp(chName, "KEY_GRAVE") == 0)
		chName = "KEY_TILDE";
	else if (strcmp(chName, "KEY_PAGEUP") == 0)
		chName = "KEY_PAGE_UP";
	else if (strcmp(chName, "KEY_PAGEDOWN") == 0)
		chName = "KEY_PAGE_DOWN";
	else if (strcmp(chName, "KEY_DOT") == 0)
		chName = "KEY_PERIOD";
	else if (strcmp(chName, "KEY_LEFTBRACE") == 0)
		chName = "KEY_LEFT_BRACE";
	else if (strcmp(chName, "KEY_RIGHTBRACE") == 0)
		chName = "KEY_RIGHT_BRACE";
	else if (strcmp(chName, "KEY_APOSTROPHE") == 0)
		chName = "KEY_QUOTE";
	else if (strcmp(chName, "KEY_SCROLLLOCK") == 0)
		chName = "KEY_SCROLL_LOCK";
	else if (strcmp(chName, "KEY_SYSREQ") == 0)	// Not sure about this one.. hm.
		chName = "KEY_PRINTSCREEN";
	else if (strcmp(chName, "KEY_NUMLOCK") == 0)
		chName = "KEY_NUM_LOCK";
	else if (strcmp(chName, "KEY_KP1") == 0)
		chName = "KEYPAD_1";
	else if (strcmp(chName, "KEY_KP2") == 0)
		chName = "KEYPAD_2";
	else if (strcmp(chName, "KEY_KP3") == 0)
		chName = "KEYPAD_3";
	else if (strcmp(chName, "KEY_KP4") == 0)
		chName = "KEYPAD_4";
	else if (strcmp(chName, "KEY_KP5") == 0)
		chName = "KEYPAD_5";
	else if (strcmp(chName, "KEY_KP6") == 0)
		chName = "KEYPAD_6";
	else if (strcmp(chName, "KEY_KP7") == 0)
		chName = "KEYPAD_7";
	else if (strcmp(chName, "KEY_KP8") == 0)
		chName = "KEYPAD_8";
	else if (strcmp(chName, "KEY_KP9") == 0)
		chName = "KEYPAD_9";
	else if (strcmp(chName, "KEY_KP0") == 0)
		chName = "KEYPAD_0";
	else if (strcmp(chName, "KEY_KPPLUS") == 0)
		chName = "KEYPAD_PLUS";
	else if (strcmp(chName, "KEY_KPENTER") == 0)
		chName = "KEYPAD_ENTER";
	else if (strcmp(chName, "KEY_KPMINUS") == 0)
		chName = "KEYPAD_MINUS";
	else if (strcmp(chName, "KEY_KPASTERISK") == 0)
		chName = "KEYPAD_ASTERIX";		// (sic)
	else if (strcmp(chName, "KEY_KPSLASH") == 0)
		chName = "KEYPAD_SLASH";
	else if (strcmp(chName, "KEY_KPDOT") == 0)
		chName = "KEYPAD_PERIOD";
	else if (strcmp(chName, "KEY_LEFTMETA") == 0)
		chName = "MODIFIERKEY_LEFT_GUI";
	else if (strcmp(chName, "KEY_RIGHTTMETA") == 0)
		chName = "MODIFIERKEY_RIGHT_GUI";
	unsigned short teensykey;
	for(unsigned int idx=0; ; idx++)
	{
		if (teensykeys[idx].name == NULL)
		{
			printf("Key not found: '%s'\n", chName);
			return -1;
		}

		if (strcmp(teensykeys[idx].name, chName) == 0)
		{
			teensykey = teensykeys[idx].code;
			break;
		}
	}
//	printf("%s: 0x%03lx (%s), teensy code 0x%04llx\n", up ? "up  " : "down", ch, chName, teensykey);

	// for now, let the user bail by hitting ESCAPE
	if (kbdEvent.code == KEY_ESC)
		return 1;

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
		printf("%hu %hu 0x%04lx\n", kbdEvent.type, kbdEvent.code, kbdEvent.value);
		return -1;
	}

	return 0;
}

struct terminalState origTerm;
int terminalStripped = 0;

void sigIntHandlerFunction(int s)
{
	if (s != -1)
		printf("Exiting on signal %d\n", s);
	if (terminalStripped)
		restoreTerminal(&origTerm);
	if (serial != -1)
	{
		restoreSerial();
		close(serial);
	}
//	if (kbd != -1)
//		close(kbd);
//	if (mouse != -1)
//		close(mouse);
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

int main(void)
{
	initkeyArray();

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigIntHandlerFunction;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGTERM, &sigIntHandler, NULL);

	unsigned int* monitoredFDs;
	unsigned int monitoredFDCount;
	if (!openInputFDs(&monitoredFDs, &monitoredFDCount))
		return -1;

	stripTerminal(&origTerm);
	terminalStripped = 1;

	serial = open("/dev/ttyAMA0", O_RDWR); //| O_NOCTTY);
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
				timeToQuit = doEvent(monitoredFDs[n]);
				if (timeToQuit)
					sigIntHandlerFunction(-1);
			}
		}
	}
}
