#include "Keyboard.h"

// Adjust this according to your needs. It defines which KVM inputs are selected when the user requests
// a given head. Each entry in the 'heads' array is a flat list of mappings - an example:
//  unsigned int heads[][16] = {
//   {  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16 },
//   {  1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 },
//  };
// Here we have two KVMs. The first has sixteen ports, and maps each to the same number (so if the user
// selects port 1, the first KVM will go to port 1, etc). The second, however, maps only eight ports, 
// so if the user selects head 9, KVM two will display input 1.
// If you map any values to zero, then that entry will leave the port as it was previously.

unsigned int heads[][16] = {
  {  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16 },
  {  0, 0, 3, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

// If your KVM uses a keystroke other than two taps of SCROLL LOCK to change head, change this.
#define KVM_CMD_SEQ_LEN 2
unsigned int kvm_cmd_seq[KVM_CMD_SEQ_LEN] = {KEY_SCROLL_LOCK, KEY_SCROLL_LOCK };

// We use the hardware serial on the Teensy - pin 7, marked D2.
#define HWSERIAL Serial1

// The last few keypresses, used to detect the commands used to change heads.
unsigned int keystrokes[5];

// Are we currently recieving a command? Set once the user has hit the attention sequence (SCROLL LOCK + SCROLL LOCK)
// while we wait for the index of the head to move to.
bool recievingCommand;
unsigned int commandBytePos;

// The ID of the head we are currently running on, populated at startup by jumper setting
int thisHeadID;

int pipeStage;

int but1;
int but2;
int but3;
int but4;

void setup()
{
  // Set the clock rate
  cli();
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
#define CPU_16MHz       0x00
  CPU_PRESCALE(CPU_16MHz);
  sei();
  
  // Init serial input and USB device
  HWSERIAL.begin(9600);
  Keyboard.begin();
  pinMode(11, OUTPUT);

  pipeStage = 0;
  but1 = but2 = but3 = but4 = 0;
  recievingCommand = false;
  commandBytePos = 0;

  // Read the jumper that defines which head this Teensy is connected to. This will be used to 
  // select the mapping of which heads are selected.
  pinMode(0, INPUT);
  if (digitalRead(0) == HIGH)
    thisHeadID = 0;
  else
    thisHeadID = 1;
}

// Translate a requested head into the desired head for the current KVM
unsigned int mapHead(unsigned int newHead)
{
  if (newHead == 0 || newHead > 16)
    return 0;

  return heads[thisHeadID][newHead - 1];
}

// Given a value, from 0 to 9, return KEY_0 through KEY_9.
unsigned int decimalToKey(unsigned int srcVal)
{
  unsigned int toRet = srcVal + KEY_1 - 1;
  if (toRet == KEY_1 - 1)
    toRet = KEY_0;
  
  return toRet;
}

// Given a keycode, from KEY_0 to KEY_9, return ints 0-9.
unsigned int keyToDecimal(unsigned int srcVal)
{
  unsigned int toRet = (srcVal - KEY_1 + 1);
  if (toRet == 10)
    toRet= 0;
  
  return toRet;
}

// Send the keystroke sequence to a KVM to select head.
void switchhead(unsigned int newHead)
{
  unsigned int keysToSend[KVM_CMD_SEQ_LEN + 2];

  // Assemble the keystrokes we will send to the KVM. First, the command sequence:
  memcpy(keysToSend, kvm_cmd_seq, sizeof(unsigned int) * KVM_CMD_SEQ_LEN);

  // And then two digits representing the desired head.

  keysToSend[KVM_CMD_SEQ_LEN + 0] = decimalToKey(newHead / 10);
  keysToSend[KVM_CMD_SEQ_LEN + 1] = decimalToKey(newHead % 10);
  
  // Now send each.
  for(unsigned int n=0; n<4; n++)
  {
    // My KVM doesn't like leading zeros - if we move to head 5, for example, it doesn't want us to type '05' but just 5.
    // Skip if this is the case.
    if ( (n == KVM_CMD_SEQ_LEN + 0) && (keysToSend[n] == KEY_0))
      continue;
    
    // Otherwise, press and release that key.
    delay(500);
    Keyboard.press(keysToSend[n]);
    delay(100);
    Keyboard.release(keysToSend[n]);
  }
}

// Given a key (and its direction, up or down), send the value to the connected computer. If we have an attention sequence (ie,
// two SCROLL LOCK presses) then process the command.
void sendKeyStroke(unsigned int key, bool upNotDown)
{
  // Don't send SCROLL LOCK values on to the connected PC, and likewise, don't pass through keystrokes intended for the teensy
  // (ie, when the user is selecting a head).
  if ((key != KEY_SCROLL_LOCK) && (recievingCommand == false))
  {
    if (upNotDown)
      Keyboard.release(key);
    else
      Keyboard.press(key);
  }

  if (upNotDown == false)
  {
    // Record this keystroke at the end of our 'keystrokes' fifo, shuffling everything else down one space
    for(unsigned int n=4; n>0; n--)
      keystrokes[n] = keystrokes[n-1];
    keystrokes[0] = key;

    // If we're not currently recieving a command, see if the user has hit the keystroke sequence to do so.
    if (!recievingCommand)
    {
      if ((keystrokes[0] == KEY_SCROLL_LOCK) &&
          (keystrokes[1] == KEY_SCROLL_LOCK) )
      {
        // Two SCROLL LOCK hits! A command follows. Note this down and return - we will be called with subsequent characters later.
        recievingCommand = true;
        commandBytePos = 0;
        return;
      }
    }
    
    // Otherwise, we are currently recieving a command. The keystroke is already stored in 'keystrokes'. Since commands are terminated
    // with either RETURN or an arrow key, check if the last thing the user pressed was one of those..
    commandBytePos++;
    if (keystrokes[0] == KEY_RETURN || keystrokes[0] == KEY_LEFT || keystrokes[0] == KEY_RIGHT)
    {
      // OK, the user has specified a new head! If the user hit RETURN, we will process this. If the user hit LEFT, we will only process it if we are head 1,
      // and if the user hit RIGHT, if we are head 0. This lets the user toggle heads individually.
      if (  (keystrokes[0] == KEY_LEFT  and thisHeadID != 1) ||
            (keystrokes[0] == KEY_RIGHT and thisHeadID != 0)   )
      {
        recievingCommand = false;
        return;
      }
      
      // The new head ID it may be one or two characters, so find those two digits.
      unsigned int headIDLow = 0;
      unsigned int headIDHigh = 0;
      if (commandBytePos > 1 )
          headIDLow = keyToDecimal(keystrokes[1]);
      if (commandBytePos > 2 )
          headIDHigh = keyToDecimal(keystrokes[2]);
      // combine the two digits..
      unsigned int headID = ((headIDHigh * 10) + headIDLow);
      // map the input value on to the current KVM..
      headID = mapHead(headID);
      // and send it.
      if (headID > 0)
        switchhead(headID);
      
      recievingCommand = false;
      return;
    }

    // If the user hasn't hit ENTER, but has typed a load of other stuff, just give up on the command.
    if (commandBytePos == 4)
      recievingCommand = false;
  }
}

void processPacket(unsigned char* bytes)
{
  switch(bytes[0])
  {
    // command 1, keyboard key down
    case 0x01:
        sendKeyStroke((bytes[1] << 8) | bytes[2], false);
      break;
    // command 2, keyboard key up
    case 0x02:
      sendKeyStroke((bytes[1] << 8) | bytes[2], true);
      break;
    
    // The rest is stuff pertaining to the mouse. 
    // First, a relative move on the X axis:
    case 0x03:
      Mouse.move(bytes[2], 0);
      break;
    // A relative move on Y
    case 0x04:
      Mouse.move(0, bytes[2]);
      break;
    // The scrollwheel
    case 0x05:
      Mouse.scroll(bytes[2]);
      break;
      
    // Button presses. The button will be in the second byte.
    case 0x06:
      if (bytes[1] == 0x00)
        but1 = bytes[2];
      else if (bytes[1] == 0x01)
        but3 = bytes[2];
      else if (bytes[1] == 0x02)
        but2 = bytes[2];
      else if (bytes[1] == 0x03)
        but4 = bytes[2];
      
      Mouse.set_buttons(but1, but2, but3);
      break;
  default:
    // todo: error handling
    break;
  }
}

void loop()
{
  unsigned char bytes[3];

  while(true)
  {
    unsigned char incomingByte;
    
    // Read a three-byte 'packet'.
    if (HWSERIAL.available() > 0)
    {
      incomingByte = HWSERIAL.read();
      bytes[pipeStage] = incomingByte;
  
      if (pipeStage == 0)
      {
        // ignore it if it's an unrecognised command byte. Maybe we're desynced or something (todo: timeout)
        if ((incomingByte < 0x00) || (incomingByte > 0x07))
        {
          pipeStage = 0;
          continue;
        }
      }
  
      // If we have a full packet, process it.
      if (pipeStage == 2)
      {
        processPacket(bytes);
        pipeStage = 0;
        continue;
      }
  
      pipeStage++;
    }
  }
}
