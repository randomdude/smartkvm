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

#include <linux/input.h>
#include </usr/include/linux/kd.h>
#include </usr/include/linux/vt.h>
#include </usr/include/linux/input-event-codes.h>

#include "keys-teensy.h"

unsigned char* KEYS[KEY_MAX];

void initkeyArray()
{
	for (unsigned int n=0; n<KEY_MAX; n++)
		KEYS[n] = "(unknown)";
	#include "keys.h"
}

struct termios tioold;

void initSerial(int serial)
{
	struct termios tionew;

	tcgetattr(serial, &tioold);
	memset(&tionew, 0, sizeof(struct termios));
	tionew.c_cflag = B9600 | CS8 | CREAD;
	tionew.c_lflag = 0;
	tionew.c_oflag = 0;
	tionew.c_cc[VTIME] = 0;
	tionew.c_cc[VMIN] = 1;
	tcsetattr(serial, TCSANOW, &tionew);
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

int doKeyEvent(int kbd, int serial)
{
	// Get the next char, as linux keycode, and find its name
	struct input_event kbdEvent;
	memset(&kbdEvent, 0, sizeof(struct input_event));
	int bytesRead = read(kbd, &kbdEvent, sizeof(struct input_event));
	if (bytesRead != sizeof(struct input_event))
	{
		printf("short read of kbd: %d of %d bytes\n", bytesRead, sizeof(struct input_event));
		// todo
	}

	if (kbdEvent.type != EV_KEY)
		return 0;
	short up;
	if (kbdEvent.value == 0x01)
		up = 0;
	else if (kbdEvent.value == 0x00)
		up = 1;
	else
		return 0;
	short ch = kbdEvent.code;
	char* chName = KEYS[ch];
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
			// todo: error handling
			printf("Not found: %s\n", chName);
			ch = KEY_ESC;
			break;
		}
		if (strcmp(teensykeys[idx].name, chName) == 0)
		{
			teensykey = teensykeys[idx].code;
			break;
		}
	}
//	printf("%s: 0x%03lx (%s), teensy code 0x%04llx\n", up ? "up  " : "down", ch, chName, teensykey);

	// for now, let the user bail by hitting ESCAPE
	if (ch == KEY_ESC)
		return 1;

	// and send it.
	unsigned char toSend[2];
	if (up)
		toSend[0] = 0x02;
	else
		toSend[0] = 0x01;
	toSend[2] = teensykey & 0xff;
	toSend[1] = (teensykey >> 8) & 0xff;
	write(serial, toSend, 3);

	return 0;
}

int doMouseEvent(int mouse, int serial)
{
	struct input_event mouseEvent;
	memset(&mouseEvent, 0, sizeof(struct input_event));
	int bytesRead = read(mouse, &mouseEvent, sizeof(struct input_event));
	if (bytesRead != sizeof(struct input_event))
	{
		printf("short read of mouse: %d of %d bytes\n", bytesRead, sizeof(struct input_event));
		return 0;
	}

	unsigned char toSend[2];
	memset(toSend, 0, 3);
	if (mouseEvent.type == INPUT_PROP_POINTER)
	{
		// idk lol
		return 0;
	}
	else if (mouseEvent.type == EV_KEY)
	{
//		printf("%hu %hu 0x%04lx\n", mouseEvent.type, mouseEvent.code, mouseEvent.value);
		toSend[0] = 0x06;
		if (mouseEvent.code == BTN_LEFT)
		{
//			printf("Left button %s\n", mouseEvent.value ? "down" : "up");
			toSend[1] = 0x00;
		}
		else if (mouseEvent.code == BTN_RIGHT)
		{
//			printf("Right button %s\n", mouseEvent.value ? "down" : "up");
			toSend[1] = 0x01;
		}
		else if (mouseEvent.code == BTN_MIDDLE)
		{
//			printf("Middle button %s\n", mouseEvent.value ? "down" : "up");
			toSend[1] = 0x02;
		}
		else if (mouseEvent.code == BTN_SIDE)
		{
//			printf("Side button %s\n", mouseEvent.value ? "down" : "up");
			toSend[1] = 0x03;
		}
		else
		{
			printf("Mouse button %d: %s\n", mouseEvent.code, KEYS[mouseEvent.code]);
			return 0;
		}
	}
	else if (mouseEvent.type == EV_MSC)
	{
		return 0;
//		if (mouseEvent.code == MSC_SCAN)
//		{
//			printf("Keyboard scancode: 0x%08lx\n", mouseEvent.value);
//		}
//		else
//		{
//			printf("Unrecognised EV_MSC: %hu 0x%04lx\n", mouseEvent.type, mouseEvent.code, mouseEvent.value);
//		}
	}
	else if (mouseEvent.type == EV_REL)
	{
		if (mouseEvent.code == REL_X)
		{
//			printf("X = 0x%d\n", mouseEvent.value);
			toSend[0] = 0x03;
		}
		else if (mouseEvent.code == REL_Y)
		{
//			printf("Y = 0x%d\n", mouseEvent.value);
			toSend[0] = 0x04;
		}
		else if (mouseEvent.code == REL_WHEEL)
		{
//			printf("Scrollwheel = 0x%d\n", mouseEvent.value);
			toSend[0] = 0x05;
		}
		else
		{
			printf("Unrecognised EV_REL: %hu 0x%04lx\n", mouseEvent.type, mouseEvent.code, mouseEvent.value);
			return 0;
		}
	}
	else
	{
		printf("%hu %hu 0x%04lx\n", mouseEvent.type, mouseEvent.code, mouseEvent.value);
	}

	toSend[2] = mouseEvent.value;
//	printf("Sending bytes: 0x%02x 0x%02x 0x%02x\n", toSend[0], toSend[1], toSend[2]);
	write(serial, toSend, 3);
	sleep(0.1);

	return 0;
}

struct terminalState origTerm;
int terminalStripped = 0;

void sigIntHandlerFunction(int s)
{
	printf("Exiting on signal %d\n", s);
	if (terminalStripped)
		restoreTerminal(&origTerm);
	exit(-1);
}

int main(void)
{
	initkeyArray();

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigIntHandlerFunction;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGTERM, &sigIntHandler, NULL);

	stripTerminal(&origTerm);
	terminalStripped = 1;

	int mouse = open("/dev/input/by-id/usb-Kensington_Kensington_Expert_Mouse-event-mouse", O_NONBLOCK);
	if (mouse == -1)
	{
		printf("Can't open mouse");
		return -1;
	}
	printf("Mouse device opened OK\n");
	int kbd = open("/dev/input/by-id/usb-ROCCAT_ROCCAT_Suora-event-kbd", O_NONBLOCK);
	if (kbd == -1)
	{
		printf("Can't open kbd");
		return -1;
	}
	printf("Keyboard device opened OK\n");

	int serial = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY);
	if (serial == -1)
	{
		printf("Can't open serial port\n");
		return -1;
	}
	printf("Serial device opened OK\n");
	initSerial(serial);


	int timeToQuit = 0;
	while(1)
	{
		fd_set inputDevices;
		FD_ZERO(&inputDevices);
		FD_SET(mouse, &inputDevices);
		FD_SET(kbd, &inputDevices);
		int highestfd = mouse;
		if (kbd > mouse)
			highestfd = kbd;

		int s = select(highestfd + 1, &inputDevices, NULL, NULL, NULL);
		if (s == -1)
		{
			printf("Failed to select() on input FDs: errno %d\n", errno);
			// todo
		}
		if (FD_ISSET(mouse, &inputDevices))
		{
			if (doMouseEvent(mouse, serial))
				timeToQuit = 1;
		}
		if (FD_ISSET(kbd, &inputDevices))
		{
			if (doKeyEvent(kbd, serial))
				timeToQuit = 1;
		}

		if (timeToQuit)
		{
			restoreTerminal(&origTerm);
			tcsetattr(serial, TCSANOW, &tioold);
			close(serial);
			close(mouse);
			return 0;
		}
	}
}
