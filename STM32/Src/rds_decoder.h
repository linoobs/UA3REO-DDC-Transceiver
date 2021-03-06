#ifndef RDS_DECODER_h
#define RDS_DECODER_h

#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "functions.h"
#include "lcd.h"

#if (defined(LAY_800x480))
#define RDS_DECODER_STRLEN 57 // length of decoded string
#else
#define RDS_DECODER_STRLEN 30 // length of decoded string
#endif

#define RDS_FILTER_STAGES 5
#define RDS_FREQ 57000
#define RDS_LOW_FREQ 1187.5f
#define RDS_FILTER_WIDTH 2000
#define RDS_DECIMATOR 16
#define RDS_STR_MAXLEN 34

// Public variables
extern char RDS_Decoder_Text[RDS_DECODER_STRLEN + 1];

// Public methods
extern void RDSDecoder_Init(void);                   // initialize the CW decoder
extern void RDSDecoder_Process(float32_t *bufferIn); // start CW decoder for the data block

#endif
