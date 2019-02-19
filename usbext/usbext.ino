#include "Keyboard.h"

#define HWSERIAL Serial1

int pipeStage;
char bytes[3];

int but1;
int but2;
int but3;
int but4;

void setup() {
  HWSERIAL.begin(9600);
  Keyboard.begin();
  pinMode(11, OUTPUT);
  pipeStage = 0;
  but1 = but2 = but3 = but4 = 0;
}

void loop()
{
  unsigned char incomingByte;

  if (HWSERIAL.available() > 0) 
  {
    incomingByte = HWSERIAL.read();
    bytes[pipeStage] = incomingByte;
    if (pipeStage == 0)
    {
      // ignore it if it's invalid, maybe we're desynced or something (todo: timeout)
      if ((incomingByte > 0x00) && (incomingByte <0x07))
        pipeStage = 1;
    }
    else if (pipeStage == 1)
    {
        pipeStage = 2;    
    }
    else if (pipeStage == 2)
    {
      if (bytes[0] == 0x01)
      {
        Keyboard.press((bytes[1] << 8) | bytes[2]);
//        Mouse.move(0x10, 0);
      }
      else if (bytes[0] == 0x02)
      {
        Keyboard.release((bytes[1] << 8) | bytes[2]);
      }
      else if (bytes[0] == 0x03)
      {
//        Keyboard.press('A');
        Mouse.move(bytes[2], 0);
      }
      else if (bytes[0] == 0x04)
      {
        Mouse.move(0, bytes[2]);
      }
      else if (bytes[0] == 0x05)
      {
        Mouse.scroll(bytes[2]);
      }
      else if (bytes[0] == 0x06)
      {
        if (bytes[1] == 0x00)
        {
          but1 = bytes[2];
        }
        else if (bytes[1] == 0x01)
        {
          but3 = bytes[2];
        }
        else if (bytes[1] == 0x02)
        {
          but2 = bytes[2];
        }
        else if (bytes[1] == 0x03)
        {
          but4 = bytes[2];
        }
        else
        {
          // todo..
        }
        Mouse.set_buttons(but1, but2, but3);
      }
      else
      {
        // todo: error handling
      }
      pipeStage = 0;
    }
  }
  
//  delay(1000);
//  digitalWrite(11, LOW);
}
