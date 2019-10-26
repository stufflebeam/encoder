#ifndef CONFIG_H
#define CONFIG_H

#include "slave.h"

static const byte LED_COUNTS[] = {
  0,
  0,
#if PCB_VERSION == 3
  0,
#endif
  0,
  0,
  0
};

static const byte ENCODER_TYPES[] = {
  ENCODER_TYPE_RELATIVE,
  ENCODER_TYPE_RELATIVE,
  ENCODER_TYPE_RELATIVE,
#if PCB_VERSION == 3
  ENCODER_TYPE_RELATIVE,
#endif
  ENCODER_TYPE_RELATIVE,
  ENCODER_TYPE_RELATIVE
};

#if PCB_VERSION == 3
static const int LED_COUNT_L = LED_COUNTS[BOARD_L2] + LED_COUNTS[BOARD_L1];
static const int LED_COUNT_M = LED_COUNTS[BOARD_M1] + LED_COUNTS[BOARD_M2];
static const int LED_COUNT_R = LED_COUNTS[BOARD_R1] + LED_COUNTS[BOARD_R2];
#else
static const int LED_COUNT_LM = LED_COUNTS[BOARD_L2] + LED_COUNTS[BOARD_L1] + LED_COUNTS[BOARD_M];
static const int LED_COUNT_R = LED_COUNTS[BOARD_R1] + LED_COUNTS[BOARD_R2];
#endif

#define BOARD_FEATURES_L2 (NO_BOARD)
#define BOARD_FEATURES_L1 (NO_BOARD)
#if PCB_VERSION == 3
#define BOARD_FEATURES_M1 (BOARD_FEATURE_ENCODER | BOARD_FEATURE_BUTTON)
#define BOARD_FEATURES_M2 (BOARD_FEATURE_ENCODER | BOARD_FEATURE_BUTTON)
#else
#define BOARD_FEATURES_M (BOARD_FEATURE_ENCODER | BOARD_FEATURE_BUTTON)
#endif
#define BOARD_FEATURES_R1 (NO_BOARD)
#define BOARD_FEATURES_R2 (NO_BOARD)

static const byte BOARD_FEATURES[] = {
  BOARD_FEATURES_L2,
  BOARD_FEATURES_L1,

#if PCB_VERSION == 3
  BOARD_FEATURES_M1,
  BOARD_FEATURES_M2,
#else
  BOARD_FEATURES_M,
#endif
  
  BOARD_FEATURES_R1,
  BOARD_FEATURES_R2,
};

//#define USART_DEBUG_ENABLED
//#define I2C_DEBUG_ENABLED
//#define PORT_STATE_DEBUG
//#define INTERRUPT_DEBUG
//#define ENCODER_PIN_DEBUG
//#define SKIP_FEATURE_VALIDATION // TODO: Why does feature validation fail at the moment?

#endif // CONFIG_H
