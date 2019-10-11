#include <Adafruit_NeoPixel.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <EEPROM.h>

#define USART_DEBUG_ENABLED 1
#define SKIP_FEATURE_VALIDATION

#include "config.h"
#include "shared.h"
#include "slave.h"

const uint16_t PixelCount = 8;
#if PCB_VERSION == 3
const uint8_t PixelPin = LEDL;
#else
const uint8_t PixelPin = LED1;
#endif

Adafruit_NeoPixel pixels(PixelCount, PixelPin, NEO_GRB + NEO_KHZ800);

volatile int address;

byte switchStates;
byte previousSwitchStates;

#if PCB_VERSION != 3 // TODO
byte touchStates;
byte previousTouchStates;
#endif

byte padStates[] =  {
  0b00001111,
  0b00001111,
  0b00001111
};
byte previousPadStates[] = {
  0b00001111,
  0b00001111,
  0b00001111
};

#include "feature_validation.h"

RotaryEncoder* encoders[BOARD_COUNT] = {
  #if BOARD_FEATURES_L2 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_L2][0], ENCODER_PINS[BOARD_L2][1])
  #else
  0
  #endif
  ,
  #if BOARD_FEATURES_L1 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_L1][0], ENCODER_PINS[BOARD_L1][1])
  #else
  0
  #endif
  ,
#if PCB_VERSION == 3
  #if BOARD_FEATURES_M1 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_M1][0], ENCODER_PINS[BOARD_M1][1])
  #else
  0
  #endif
  ,
  #if BOARD_FEATURES_M2 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_M2][0], ENCODER_PINS[BOARD_M2][1])
  #else
  0
  #endif
#else
  #if BOARD_FEATURES_M & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_M][0], ENCODER_PINS[BOARD_M][1])
  #else
  0
  #endif
#endif
  
  ,
  #if BOARD_FEATURES_R1 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_R1][0], ENCODER_PINS[BOARD_R1][1])
  #else
  0
  #endif
  ,
  #if BOARD_FEATURES_R2 & BOARD_FEATURE_ENCODER
  new RotaryEncoder(ENCODER_PINS[BOARD_R2][0], ENCODER_PINS[BOARD_R2][1])
  #else
  0
  #endif
};

int positions[] = {
  0, 
  0,
  0,
#if PCB_VERSION == 3
  0,
#endif
  0, 
  0
};

#if USART_DEBUG_ENABLED
byte states[] = {
  LOW, LOW, 
  LOW, LOW,
  LOW, LOW,
#if PCB_VERSION == 3
  LOW, LOW,
#endif
  LOW, LOW, 
  LOW, LOW
};
#endif

byte interrupter = 255;

void(* reset) (void) = 0;

void setup() {
  noInterrupts();
  pixels.begin();
  interrupts();

  // TODO: validate board feature combinations
  address = EEPROM.read(0);

  Serial.begin(115200);
  Serial.println("Boot");
  Serial.print("Address: ");
  Serial.println(address);
  
 if (address == 255 || address < 10) {
    Serial.print("Address: ");
    Serial.println(address);
    Serial.println("Requesting address from master");
    Wire.begin();
    Wire.requestFrom(1, 1);
    while (Wire.available()) {
      address = Wire.read();
      Serial.print("Received address: ");
      Serial.println(address);
    }

    if (address == 255) {
      Serial.println("Did not receive address from master. Resetting.");
      reset();
    }
  
    Serial.print("Starting with address: ");
    Serial.println(address);
    Wire.begin(address);

    EEPROM.write(0, address);
  
    delay(900);
    sendMessage(DEBUG_RECEIVED_ADDRESS, address, CONTROL_TYPE_DEBUG);
  } else {
    Wire.begin(address);
    sendMessage(DEBUG_BOOT, 1, CONTROL_TYPE_DEBUG);
  }

  #ifndef USART_DEBUG_ENABLED
  Serial.end();
  #endif

  for (byte i = 0; i < BOARD_COUNT; ++i) {
    const byte boardFeatures = BOARD_FEATURES[i];

    if (boardFeatures & BOARD_FEATURE_ENCODER) {
      pinMode(ENCODER_PINS[i][0], INPUT_PULLUP);
      pinMode(ENCODER_PINS[i][1], INPUT_PULLUP);
    }

    if (boardFeatures & BOARD_FEATURE_BUTTON) {
      pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    if (boardFeatures & BOARD_FEATURE_POT) {
      // TODO: anything needed here?
    }

#if PCB_VERSION != 3 // TODO
    if (boardFeatures & BOARD_FEATURE_TOUCH) {
      pinMode(TOUCH_PINS[i], INPUT);
    }
#endif

#if PCB_VERSION != 3 // TODO
    if (BOARD_FEATURES[i] & BOARD_FEATURE_PADS) {
      for (byte j = 0; j < 4; ++j) {
        #if USART_DEBUG_ENABLED
        Serial.print("Configuring pin as input: ");
        Serial.println(PAD_PINS[i][j]);
        #endif
        pinMode(PAD_PINS[i][j], INPUT_PULLUP);
      }
    } 
#endif
  }

  // TODO: Move interrupt initializations to loop above
  PCICR |= (1 << PCIE0) | (1 << PCIE1) | (1 << PCIE2);
  
  #ifndef USART_DEBUG_ENABLED
  if (BOARD_FEATURES[BOARD_L2] & BOARD_FEATURE_ENCODER) {
    PCMSK2 |= (1 << ENCL2A_INT) | (1 << ENCL2B_INT);
  }
  #endif

  if (BOARD_FEATURES[BOARD_L1] & BOARD_FEATURE_ENCODER) {
    PCMSK2 |= (1 << ENCL1A_INT) | (1 << ENCL1B_INT);
  }

#if PCB_VERSION == 3
  if (BOARD_FEATURES[BOARD_M1] & BOARD_FEATURE_ENCODER) {
    PCMSK2 |= (1 << ENCM1B_INT);
    PCMSK2 |= (1 << ENCM1A_INT);
  }
  if (BOARD_FEATURES[BOARD_M2] & BOARD_FEATURE_ENCODER) {
    PCMSK2 |= (1 << ENCM2B_INT);
    PCMSK2 |= (1 << ENCM2A_INT);
  }
#else
  if (BOARD_FEATURES[BOARD_M] & BOARD_FEATURE_ENCODER) {
    PCMSK1 |= (1 << ENC1B_INT);
    PCMSK2 |= (1 << ENC1A_INT);
  }
#endif
  
  if (BOARD_FEATURES[BOARD_R1] & BOARD_FEATURE_ENCODER) {
    PCMSK0 |= (1 << ENCL1A_INT) | (1 << ENCL1B_INT);
  }

  if (BOARD_FEATURES[BOARD_R1] & BOARD_FEATURE_ENCODER) {
    PCMSK0 |= (1 << ENCR2A_INT) | (1 << ENCR2B_INT);
  }

  if (BOARD_FEATURES[BOARD_L1] & (BOARD_FEATURE_BUTTON
#if PCB_VERSION != 3 // TODO
  | BOARD_FEATURE_TOUCH
#endif
  )) {
    PCMSK1 |= 1 << SWL_INT;
  }

#if PCB_VERSION == 3
  if (BOARD_FEATURES[BOARD_M1] & BOARD_FEATURE_BUTTON) {
    PCMSK1 |= 1 << SWM_INT; // TODO: check this
  }
#else
  if (BOARD_FEATURES[BOARD_M] & BOARD_FEATURE_BUTTON) {
    PCMSK1 |= 1 << SWM_INT;
  }
  if (BOARD_FEATURES[BOARD_M] & BOARD_FEATURE_TOUCH) {
    PCMSK1 |= 1 << TOUCH_INT;
  }
#endif

  if (BOARD_FEATURES[BOARD_R1] & (BOARD_FEATURE_BUTTON 
#if PCB_VERSION != 3 // TODO
  | BOARD_FEATURE_TOUCH
#endif
  )) {
    PCMSK1 |= 1 << SWR_INT;
  }

  if (BOARD_FEATURES[BOARD_L1] & BOARD_FEATURE_PADS) {
    enablePCINT(SWL);
    enablePCINT(ENCL1A);
    enablePCINT(ENCL1B);
    enablePCINT(ENCL2A);
  }

#if PCB_VERSION != 3 // TODO
  if (BOARD_FEATURES[BOARD_M] & BOARD_FEATURE_PADS) {
    enablePCINT(SWM);
    enablePCINT(ENC1B);
    enablePCINT(ENC1A);
    enablePCINT(TOUCH); // TODO: fix  POT -> TOUCH on board
  }
#endif
  
  if (BOARD_FEATURES[BOARD_R1] & BOARD_FEATURE_PADS) {
    PCMSK0 |= (1 << ENCR1A_INT) | (1 << ENCR1B_INT) | (1 << ENCR2A_INT);
    PCMSK1 |= 1 << SWR_INT;
  }

  // TODO: initialize according to enabled buttons
  switchStates = previousSwitchStates = PINC & SW_INTS_MASK; // TODO: construct mask according to enabled buttons
  // TODO: initialize touch states
}

void loop() {
  // TODO: check touch
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_BUTTON)
  if (previousSwitchStates != switchStates) {
    #if USART_DEBUG_ENABLED
    Serial.print("SWITCHES: ");
    Serial.println(switchStates);
    #endif
    byte changed = previousSwitchStates ^ switchStates;
    previousSwitchStates = switchStates;
    #if USART_DEBUG_ENABLED
    Serial.print("Changed: ");
    Serial.println(changed);
    #endif
    if (changed) {
      for (byte i = 0; i < 3; ++i) {
        byte switchMask = (1 << SW_INTS[i]);
        if (changed & switchMask) {
          handleButtonChange(i, (switchStates & switchMask) ? 0 : 1);
        }
      }
    }
  }
#endif

#if PCB_VERSION != 3 // TODO
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_TOUCH)
  if (previousTouchStates != touchStates) {
    #if USART_DEBUG_ENABLED
    Serial.print("TOUCHES: ");
    Serial.println(touchStates);
    #endif
    byte changed = previousTouchStates ^ touchStates;
    previousTouchStates = touchStates;
    #if USART_DEBUG_ENABLED
    Serial.print("Changed: ");
    Serial.println(changed);
    #endif
    // TODO:
    if (changed) {
      for (byte i = 0; i < 3; ++i) {
        byte switchMask = (1 << SW_INTS[i]);
        if (changed & switchMask) {
          handleButtonChange(i, (switchStates & switchMask) ? 0 : 1);
        }
      }
    }
  }
#endif
#endif

#if PCB_VERSION != 3 // TODO
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_PADS)
  for (byte board = 0; board < 3; board++) {
    const byte padStateIndex = board - 1;
    const byte boardPadStates = padStates[padStateIndex];
    const byte previousBoardPadStates = previousPadStates[padStateIndex];
    if (previousBoardPadStates != boardPadStates) {
      #if USART_DEBUG_ENABLED
      Serial.print("BOARD: ");
      Serial.println(board);
      #endif
      byte changed = previousBoardPadStates ^ boardPadStates;
      previousPadStates[padStateIndex] = boardPadStates;
      #if USART_DEBUG_ENABLED
      Serial.print("Changed: ");
      Serial.println(changed);
      #endif
      if (changed) {
        for (byte i = 0; i < 4; ++i) {
          byte padMask = (1 << i);
          if (changed & padMask) {
            byte pinState = (boardPadStates & padMask) ? 1 : 0;
            noInterrupts();
            pixels.setPixelColor((board == BOARD_L1 ? 4 : 0) + i, pixels.Color(0, pinState ? 0 : 70, 0));
            pixels.show();
            interrupts();
            handleButtonChange(i, pinState);
          }
        }
      }
    }
  }
#endif
#endif

#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_ENCODER) || ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_POT) // TODO: separate encoder and pot?
  for (int i = 0; i < BOARD_COUNT; ++i) {
    int position;
    byte positionChanged = false;
    
    if (BOARD_FEATURES[i] & BOARD_FEATURE_ENCODER) {
      position = (*encoders[i]).getPosition(); // TODO: use getDirection or create separate relative encoder feature
      positionChanged = position != positions[i];
      
      #if USART_DEBUG_ENABLED
      byte stateA = digitalRead(ENCODER_PINS[i][0]);
      byte stateB = digitalRead(ENCODER_PINS[i][1]);
  
      if (stateA != states[2*i] || stateB != states[2*i+1]) {
        Serial.print("Interrupter: ");
        Serial.println(interrupter);
        Serial.print("Pins ");
        Serial.println(i);
        Serial.println("A B");
        Serial.print(stateA);
        Serial.print(" ");
        Serial.println(stateB);
        states[2*i] = stateA;
        states[2*i+1] = stateB;
      }
      #endif
    } else if (BOARD_FEATURES[i] & BOARD_FEATURE_POT) {
      positionChanged = position != positions[i] && (position == 0 || position == 127 || POT_CHANGE_THRESHOLD < abs(positions[i] - position));
      // Resolution restricted to 7-bits for MIDI compatibility
      position = analogRead(POT_PINS[i]) >> 3;
    } else {
      continue;
    }
    
    if (positionChanged) {
      handlePositionChange(i, position);
      positions[i] = position;
    }
  }
  #endif

  #if USART_DEBUG_ENABLED
  if (Serial.available()) {        // If anything comes in Serial,
    Serial.write(Serial.read());   // read it and send it out
  }
  #endif
}

void handlePositionChange(byte input, byte state) { // state cannot be byte if full resolution of ADC is used (max value = 1023)
  sendMessage(input, state, CONTROL_TYPE_POSITION);
}

void handleButtonChange(byte input, byte state) {
  sendMessage(input, state, CONTROL_TYPE_BUTTON);
}

void sendMessage(byte input, byte value, ControlType type) {
  Wire.beginTransmission(1);
  byte message[] = {address, input, type, value};
  Wire.write(message, 4);
  Wire.endTransmission();
}

#if PCB_VERSION != 3 // TODO
inline byte readPadPin(byte board, byte pin) {
  Serial.print("Reading: ");
  Serial.println(PAD_PINS[board][pin]);
  Serial.print("Value: ");
  Serial.println(digitalRead(PAD_PINS[board][pin]));
  return (digitalRead(PAD_PINS[board][pin]) == LOW ? 0 : 1) << pin;
}

inline void updatePadStates() {
  for (byte board = 1; board < 4; ++board) { // Pads and buttons not available on leftmost and rightmost boards
    if (BOARD_FEATURES[board] & BOARD_FEATURE_PADS) {
      const byte padStateIndex = board - 1;
      padStates[padStateIndex] = readPadPin(board, 3) | readPadPin(board, 2) | readPadPin(board, 1) | readPadPin(board, 0);
      #if USART_DEBUG_ENABLED
      Serial.print("BOARD PADS ");
      Serial.println(board);
      Serial.print("States: ");
      Serial.println(padStates[padStateIndex]);
      #endif
    }
  }
}
#endif

inline void updateSwitchStates() {
  switchStates = PINC &
    (!!(BOARD_FEATURES[1] & BOARD_FEATURE_BUTTON) << SW_INTS[1]) |
    (!!(BOARD_FEATURES[2] & BOARD_FEATURE_BUTTON) << SW_INTS[2]) |
    (!!(BOARD_FEATURES[3] & BOARD_FEATURE_BUTTON) << SW_INTS[3]);
}

#if PCB_VERSION != 3 // TODO
inline void updateTouchStates() {
  for (byte i = 1; i < 4; ++i) { // Pads and buttons not available on leftmost and rightmost boards
    if (BOARD_FEATURES[i] & BOARD_FEATURE_TOUCH) {
      touchStates |= TOUCH_PINS[i];
    }
  }
}
#endif

ISR(PCINT0_vect) {
#if USART_DEBUG_ENABLED
  interrupter = 0;
#endif

#if HAS_FEATURE(R2, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_R2]).tick();
#endif

#if PCB_VERSION == 3

#if HAS_FEATURE(M2, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_M2]).tick();
#endif

#else

#if HAS_FEATURE(R1, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_R1]).tick();
#endif

#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_PADS)
  updatePadStates();
#endif

#endif
}

ISR(PCINT1_vect) {
#if USART_DEBUG_ENABLED
  interrupter = 1;
#endif

#if PCB_VERSION == 3

#if HAS_FEATURE(L1, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_L1]).tick();
#endif

#else

#if HAS_FEATURE(M, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_M]).tick();
#endif
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_PADS)
  updatePadStates();
#endif

#endif

#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_BUTTON)
  updateSwitchStates();
#endif
}

ISR(PCINT2_vect) {
#if USART_DEBUG_ENABLED
  interrupter = 2;
#endif

#if PCB_VERSION == 3

#if HAS_FEATURE(M1, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_M1]).tick();
#endif
#if HAS_FEATURE(L2, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_M2]).tick();
#endif
#if HAS_FEATURE(R1, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_R1]).tick();
#endif

#else

#if HAS_FEATURE(M, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_M]).tick();
#endif
#if HAS_FEATURE(L1, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_L1]).tick();
#endif
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_PADS)
  updatePadStates();
#endif

#endif


#ifndef USART_DEBUG_ENABLED
#if HAS_FEATURE(L2, BOARD_FEATURE_ENCODER)
  (*encoders[BOARD_L2]).tick();
#endif
#endif

#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_BUTTON)
  updateSwitchStates();
#endif

#if PCB_VERSION != 3 // TODO
#if ANY_BOARD_HAS_FEATURE(BOARD_FEATURE_TOUCH)
  updateTouchStates();
#endif
#endif
}
