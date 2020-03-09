#include "stm32h7xx_hal.h"
#include "main.h"
#include "fpga.h"
#include "functions.h"
#include "trx_manager.h"
#include "lcd.h"
#include "audio_processor.h"
#include "settings.h"
#include "wm8731.h"
#include "usbd_debug_if.h"

volatile uint32_t FPGA_samples = 0;
volatile bool FPGA_busy = false;
volatile bool FPGA_NeedSendParams = false;
volatile bool FPGA_NeedGetParams = false;
volatile bool FPGA_Buffer_underrun = false;

static GPIO_InitTypeDef FPGA_GPIO_InitStruct;
static bool FPGA_bus_direction = false; //false - OUT; true - in
uint_fast16_t FPGA_Audio_Buffer_Index = 0;
bool FPGA_Audio_Buffer_State = true; //true - compleate ; false - half
SRAM1 volatile float32_t FPGA_Audio_Buffer_RX1_Q[FPGA_AUDIO_BUFFER_SIZE] = {0};
SRAM1 volatile float32_t FPGA_Audio_Buffer_RX1_I[FPGA_AUDIO_BUFFER_SIZE] = {0};
SRAM1 volatile float32_t FPGA_Audio_Buffer_RX2_Q[FPGA_AUDIO_BUFFER_SIZE] = {0};
SRAM1 volatile float32_t FPGA_Audio_Buffer_RX2_I[FPGA_AUDIO_BUFFER_SIZE] = {0};
SRAM1 volatile float32_t FPGA_Audio_SendBuffer_Q[FPGA_AUDIO_BUFFER_SIZE] = {0};
SRAM1 volatile float32_t FPGA_Audio_SendBuffer_I[FPGA_AUDIO_BUFFER_SIZE] = {0};

uint_fast8_t FPGA_readPacket(void);
void FPGA_writePacket(uint_fast8_t packet);
void FPGA_clockFall(void);
void FPGA_clockRise(void);
void FPGA_fpgadata_sendiq(void);
void FPGA_fpgadata_getiq(void);
void FPGA_fpgadata_getparam(void);
void FPGA_fpgadata_sendparam(void);
void FPGA_setBusInput(void);
void FPGA_setBusOutput(void);
void FPGA_test_bus(void);
void FPGA_read_flash(void);

void FPGA_Init(void)
{
	FPGA_GPIO_InitStruct.Pin = FPGA_BUS_D0_Pin | FPGA_BUS_D1_Pin | FPGA_BUS_D2_Pin | FPGA_BUS_D3_Pin | FPGA_BUS_D4_Pin | FPGA_BUS_D5_Pin | FPGA_BUS_D6_Pin | FPGA_BUS_D7_Pin;
	FPGA_GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	FPGA_GPIO_InitStruct.Pull = GPIO_PULLUP;
	FPGA_GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOA, &FPGA_GPIO_InitStruct);

	FPGA_test_bus();

	FPGA_start_audio_clock();

	//FPGA_read_flash();
}

void FPGA_test_bus(void) //проверка шины
{
	bool err = false;
	FPGA_busy = true;
	for (uint_fast8_t b = 0; b <= 8; b++)
	{
		//STAGE 1
		//out
		FPGA_writePacket(0); //PIN test command
		//clock
		GPIOC->BSRR = FPGA_SYNC_Pin;
		FPGA_clockRise();
		//in
		//clock
		GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);

		//STAGE 2
		//out
		FPGA_writePacket(b);
		//clock
		FPGA_clockRise();
		//in
		if (FPGA_readPacket() != b)
		{
			char buff[32] = "";
			sprintf(buff, "[ERR] FPGA BUS Pin%d error", b);
			sendToDebug_strln(buff);
			sprintf(buff, "FPGA BUS Pin%d error", b);
			LCD_showError(buff, true);
			err = true;
		}
		//clock
		FPGA_clockFall();
	}
	FPGA_busy = false;
	if (!err)
		sendToDebug_strln("[OK] FPGA inited");
}

void FPGA_start_audio_clock(void) //запуск PLL для I2S и кодека, при включенном тактовом не программируется i2c
{
	FPGA_busy = true;
	//STAGE 1
	//out
	FPGA_writePacket(5);
	//clock
	GPIOC->BSRR = FPGA_SYNC_Pin;
	FPGA_clockRise();
	//in
	//clock
	GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);
	FPGA_busy = false;
}

void FPGA_stop_audio_clock(void) //остановка PLL для I2S и кодека, при включенном тактовом не программируется i2c
{
	FPGA_busy = true;
	//STAGE 1
	//out
	FPGA_writePacket(6);
	//clock
	GPIOC->BSRR = FPGA_SYNC_Pin;
	FPGA_clockRise();
	//in
	//clock
	GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);
	FPGA_busy = false;
}

void FPGA_fpgadata_stuffclock(void)
{
	uint_fast8_t FPGA_fpgadata_out_tmp8 = 0;
	FPGA_busy = true;
	//обмен данными

	//STAGE 1
	//out
	if (FPGA_NeedSendParams)
		FPGA_fpgadata_out_tmp8 = 1;
	else if (FPGA_NeedGetParams)
		FPGA_fpgadata_out_tmp8 = 2;

	if (FPGA_fpgadata_out_tmp8 != 0)
	{
		FPGA_writePacket(FPGA_fpgadata_out_tmp8);
		//clock
		GPIOC->BSRR = FPGA_SYNC_Pin;
		FPGA_clockRise();
		//in
		//clock
		GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);

		if (FPGA_NeedSendParams)
		{
			FPGA_fpgadata_sendparam();
			FPGA_NeedSendParams = false;
		}
		else if (FPGA_NeedGetParams)
		{
			FPGA_fpgadata_getparam();
			FPGA_NeedGetParams = false;
		}
	}
	FPGA_busy = false;
}

void FPGA_fpgadata_iqclock(void)
{
	uint_fast8_t FPGA_fpgadata_out_tmp8 = 0;
	FPGA_busy = true;
	VFO *current_vfo = CurrentVFO();
	//обмен данными

	//STAGE 1
	//out
	if (TRX_on_TX() && current_vfo->Mode != TRX_MODE_LOOPBACK)
		FPGA_fpgadata_out_tmp8 = 3;
	else
		FPGA_fpgadata_out_tmp8 = 4;

	FPGA_writePacket(FPGA_fpgadata_out_tmp8);
	//clock
	GPIOC->BSRR = FPGA_SYNC_Pin;
	FPGA_clockRise();
	//in
	//clock
	GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);

	if (TRX_on_TX() && current_vfo->Mode != TRX_MODE_LOOPBACK)
		FPGA_fpgadata_sendiq();
	else
		FPGA_fpgadata_getiq();

	FPGA_busy = false;
}

inline void FPGA_fpgadata_sendparam(void)
{
	uint_fast8_t FPGA_fpgadata_out_tmp8 = 0;
	VFO *current_vfo = CurrentVFO();
	VFO *secondary_vfo = SecondaryVFO();
	uint32_t TRX_freq_phrase = getPhraseFromFrequency(current_vfo->Freq + TRX_SHIFT);
	uint32_t TRX_freq_phrase2 = getPhraseFromFrequency(secondary_vfo->Freq + TRX_SHIFT);
	if (!TRX_on_TX())
	{
		switch (current_vfo->Mode)
		{
		case TRX_MODE_CW_L:
			TRX_freq_phrase = getPhraseFromFrequency(current_vfo->Freq + TRX_SHIFT + TRX.CW_GENERATOR_SHIFT_HZ);
			break;
		case TRX_MODE_CW_U:
			TRX_freq_phrase = getPhraseFromFrequency(current_vfo->Freq + TRX_SHIFT - TRX.CW_GENERATOR_SHIFT_HZ);
			break;
		default:
			break;
		}
		switch (secondary_vfo->Mode)
		{
		case TRX_MODE_CW_L:
			TRX_freq_phrase2 = getPhraseFromFrequency(secondary_vfo->Freq + TRX_SHIFT + TRX.CW_GENERATOR_SHIFT_HZ);
			break;
		case TRX_MODE_CW_U:
			TRX_freq_phrase2 = getPhraseFromFrequency(secondary_vfo->Freq + TRX_SHIFT - TRX.CW_GENERATOR_SHIFT_HZ);
			break;
		default:
			break;
		}
	}
	//STAGE 2
	//out PTT+PREAMP
	bitWrite(FPGA_fpgadata_out_tmp8, 0, (TRX_on_TX() && current_vfo->Mode != TRX_MODE_LOOPBACK));
	if (!TRX_on_TX())
		bitWrite(FPGA_fpgadata_out_tmp8, 1, TRX.ADC_Driver);
	bitWrite(FPGA_fpgadata_out_tmp8, 2, TRX.ADC_DITH);
	bitWrite(FPGA_fpgadata_out_tmp8, 3, TRX.ADC_SHDN);
	if (TRX_on_TX())
		bitWrite(FPGA_fpgadata_out_tmp8, 3, true);
	bitWrite(FPGA_fpgadata_out_tmp8, 4, TRX.ADC_RAND);
	bitWrite(FPGA_fpgadata_out_tmp8, 5, TRX.ADC_PGA);
	FPGA_writePacket(FPGA_fpgadata_out_tmp8);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 3
	//out RX1-FREQ
	FPGA_writePacket(((TRX_freq_phrase & (0XFF << 16)) >> 16));
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 4
	//OUT RX1-FREQ
	FPGA_writePacket(((TRX_freq_phrase & (0XFF << 8)) >> 8));
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 5
	//OUT RX1-FREQ
	FPGA_writePacket(TRX_freq_phrase & 0XFF);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 6
	//out RX2-FREQ
	FPGA_writePacket(((TRX_freq_phrase2 & (0XFF << 16)) >> 16));
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 7
	//OUT RX2-FREQ
	FPGA_writePacket(((TRX_freq_phrase2 & (0XFF << 8)) >> 8));
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 8
	//OUT RX2-FREQ
	FPGA_writePacket(TRX_freq_phrase2 & 0XFF);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 9
	//OUT CIC-GAIN
	FPGA_writePacket(TRX.CIC_GAINER_val);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 10
	//OUT CICCOMP-GAIN
	FPGA_writePacket(TRX.CICFIR_GAINER_val);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 11
	//OUT TX-CICCOMP-GAIN
	FPGA_writePacket(TRX.TXCICFIR_GAINER_val);
	FPGA_clockRise();
	FPGA_clockFall();

	//STAGE 12
	//OUT DAC-GAIN
	FPGA_writePacket(TRX.DAC_GAINER_val);
	FPGA_clockRise();
	FPGA_clockFall();
	
	//STAGE 13
	//OUT ADC OFFSET
	FPGA_writePacket(((CALIBRATE.adc_offset & (0XFF << 8)) >> 8));
	FPGA_clockRise();
	FPGA_clockFall();
	
	//STAGE 14
	//OUT ADC OFFSET
	FPGA_writePacket(CALIBRATE.adc_offset & 0XFF);
	FPGA_clockRise();
	FPGA_clockFall();
}

inline void FPGA_fpgadata_getparam(void)
{
	register uint_fast8_t FPGA_fpgadata_in_tmp8 = 0;

	//STAGE 2
	//clock
	FPGA_clockRise();
	//in
	FPGA_fpgadata_in_tmp8 = FPGA_readPacket();
	TRX_ADC_OTR = bitRead(FPGA_fpgadata_in_tmp8, 0);
	TRX_DAC_OTR = bitRead(FPGA_fpgadata_in_tmp8, 1);
	//clock
	FPGA_clockFall();

	//STAGE 3
	//clock
	FPGA_clockRise();
	//in
	FPGA_fpgadata_in_tmp8 = FPGA_readPacket();
	//clock
	FPGA_clockFall();
	//STAGE 4
	//clock
	FPGA_clockRise();
	//in
	TRX_ADC_MINAMPLITUDE = (FPGA_fpgadata_in_tmp8 << 8) | (FPGA_readPacket() & 0xFF);
	//clock
	FPGA_clockFall();

	//STAGE 5
	//clock
	FPGA_clockRise();
	//in
	FPGA_fpgadata_in_tmp8 = FPGA_readPacket();
	//clock
	FPGA_clockFall();
	//STAGE 6
	//clock
	FPGA_clockRise();
	//in
	TRX_ADC_MAXAMPLITUDE = (FPGA_fpgadata_in_tmp8 << 8) | (FPGA_readPacket() & 0xFF);
	//clock
	FPGA_clockFall();
}

inline void FPGA_fpgadata_getiq(void)
{
	q31_t FPGA_fpgadata_in_tmp32 = 0;
	float32_t FPGA_fpgadata_in_float32 = 0;
	FPGA_samples++;

	//STAGE 2 in Q RX1
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 = (FPGA_readPacket() & 0XFF) << 24;
	FPGA_clockFall();

	//STAGE 3
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 16;
	FPGA_clockFall();

	//STAGE 4
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 8;
	FPGA_clockFall();
	
	//STAGE 5
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 0;

	arm_q31_to_float(&FPGA_fpgadata_in_tmp32, &FPGA_fpgadata_in_float32, 1);
	if (TRX_IQ_swap)
	{
		if (NeedFFTInputBuffer)
			FFTInput_I[FFT_buff_index] = FPGA_fpgadata_in_float32;
		FPGA_Audio_Buffer_RX1_I[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	else
	{
		if (NeedFFTInputBuffer)
			FFTInput_Q[FFT_buff_index] = FPGA_fpgadata_in_float32;
		FPGA_Audio_Buffer_RX1_Q[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	FPGA_clockFall();

	//STAGE 6 in I RX1
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 = (FPGA_readPacket() & 0XFF) << 24;
	FPGA_clockFall();

	//STAGE 7
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 16;
	FPGA_clockFall();
	
	//STAGE 8
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 8;
	FPGA_clockFall();
	
	//STAGE 9
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 0;

	arm_q31_to_float(&FPGA_fpgadata_in_tmp32, &FPGA_fpgadata_in_float32, 1);
	if (TRX_IQ_swap)
	{
		if (NeedFFTInputBuffer)
			FFTInput_Q[FFT_buff_index] = FPGA_fpgadata_in_float32;
		FPGA_Audio_Buffer_RX1_Q[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	else
	{
		if (NeedFFTInputBuffer)
			FFTInput_I[FFT_buff_index] = FPGA_fpgadata_in_float32;
		FPGA_Audio_Buffer_RX1_I[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	FPGA_clockFall();

	//STAGE 10 in Q RX2
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 = (FPGA_readPacket() & 0XFF) << 24;
	FPGA_clockFall();

	//STAGE 11
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32|= (FPGA_readPacket() & 0XFF) << 16;
	FPGA_clockFall();
	
	//STAGE 12
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32|= (FPGA_readPacket() & 0XFF) << 8;
	FPGA_clockFall();
	
	//STAGE 13
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32|= (FPGA_readPacket() & 0XFF) << 0;
	
	arm_q31_to_float(&FPGA_fpgadata_in_tmp32, &FPGA_fpgadata_in_float32, 1);
	if (TRX_IQ_swap)
	{
		FPGA_Audio_Buffer_RX2_I[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	else
	{
		FPGA_Audio_Buffer_RX2_Q[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	FPGA_clockFall();

	//STAGE 14 in I RX2
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 = (FPGA_readPacket() & 0XFF) << 24;
	FPGA_clockFall();

	//STAGE 15
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 16;
	FPGA_clockFall();
	
	//STAGE 16
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 8;
	FPGA_clockFall();
	
	//STAGE 17
	FPGA_clockRise();
	FPGA_fpgadata_in_tmp32 |= (FPGA_readPacket() & 0XFF) << 0;
	
	arm_q31_to_float(&FPGA_fpgadata_in_tmp32, &FPGA_fpgadata_in_float32, 1);
	if (TRX_IQ_swap)
	{
		FPGA_Audio_Buffer_RX2_Q[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}
	else
	{
		FPGA_Audio_Buffer_RX2_I[FPGA_Audio_Buffer_Index] = FPGA_fpgadata_in_float32;
	}

	FPGA_Audio_Buffer_Index++;
	if (FPGA_Audio_Buffer_Index == FPGA_AUDIO_BUFFER_SIZE)
		FPGA_Audio_Buffer_Index = 0;

	if (NeedFFTInputBuffer)
	{
		FFT_buff_index++;
		if (FFT_buff_index == FFT_SIZE)
		{
			FFT_buff_index = 0;
			NeedFFTInputBuffer = false;
			FFT_buffer_ready = true;
		}
	}
	FPGA_clockFall();
}

inline void FPGA_fpgadata_sendiq(void)
{
	q31_t FPGA_fpgadata_out_q_tmp32 = 0;
	q31_t FPGA_fpgadata_out_i_tmp32 = 0;
	arm_float_to_q31((float32_t *)&FPGA_Audio_SendBuffer_Q[FPGA_Audio_Buffer_Index], &FPGA_fpgadata_out_q_tmp32, 1);
	arm_float_to_q31((float32_t *)&FPGA_Audio_SendBuffer_I[FPGA_Audio_Buffer_Index], &FPGA_fpgadata_out_q_tmp32, 1);
	FPGA_samples++;
	
	//STAGE 2 out Q
	FPGA_writePacket((FPGA_fpgadata_out_q_tmp32 >> 24) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();

	//STAGE 3
	FPGA_writePacket((FPGA_fpgadata_out_q_tmp32 >> 16) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();

	//STAGE 4
	FPGA_writePacket((FPGA_fpgadata_out_q_tmp32 >> 8) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();
	
	//STAGE 5
	FPGA_writePacket((FPGA_fpgadata_out_q_tmp32 >> 0) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();
	
	//STAGE 6 out I
	FPGA_writePacket((FPGA_fpgadata_out_i_tmp32 >> 24) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();

	//STAGE 7
	FPGA_writePacket((FPGA_fpgadata_out_i_tmp32 >> 16) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();

	//STAGE 8
	FPGA_writePacket((FPGA_fpgadata_out_i_tmp32 >> 8) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();
	
	//STAGE 9
	FPGA_writePacket((FPGA_fpgadata_out_i_tmp32 >> 0) & 0xFF);
	//clock
	FPGA_clockRise();
	//clock
	FPGA_clockFall();
	
	FPGA_Audio_Buffer_Index++;
	if (FPGA_Audio_Buffer_Index == FPGA_AUDIO_BUFFER_SIZE)
	{
		if (Processor_NeedTXBuffer)
		{
			FPGA_Buffer_underrun = true;
			FPGA_Audio_Buffer_Index--;
		}
		else
		{
			FPGA_Audio_Buffer_Index = 0;
			FPGA_Audio_Buffer_State = true;
			Processor_NeedTXBuffer = true;
		}
	}
	else if (FPGA_Audio_Buffer_Index == FPGA_AUDIO_BUFFER_SIZE / 2)
	{
		if (Processor_NeedTXBuffer)
		{
			FPGA_Buffer_underrun = true;
			FPGA_Audio_Buffer_Index--;
		}
		else
		{
			FPGA_Audio_Buffer_State = false;
			Processor_NeedTXBuffer = true;
		}
	}
}

inline void FPGA_setBusInput(void)
{
	for (uint32_t position = 0U; position < 8U; position++)
	{
		if (((uint32_t)(FPGA_GPIO_InitStruct.Pin) & (0x01U << position)) == (0x01U << position))
		{
			// Configure IO Direction mode (Input)
			uint32_t temp = GPIOA->MODER;
			temp &= ~(GPIO_MODER_MODE0 << (position * 2U));
			temp |= ((GPIO_MODE_INPUT & 0x00000003U) << (position * 2U));
			GPIOA->MODER = temp;
		}
	}
	FPGA_bus_direction = true;
}

inline void FPGA_setBusOutput(void)
{
	for (uint32_t position = 0U; position < 8U; position++)
	{
		if (((uint32_t)(FPGA_GPIO_InitStruct.Pin) & (0x01U << position)) == (0x01U << position))
		{
			// Configure IO Direction mode (Output)
			uint32_t temp = GPIOA->MODER;
			temp &= ~(GPIO_MODER_MODE0 << (position * 2U));
			temp |= ((GPIO_MODE_OUTPUT_PP & 0x00000003U) << (position * 2U));
			GPIOA->MODER = temp;
		}
	}
	FPGA_bus_direction = false;
}

inline void FPGA_clockRise(void)
{
	//HAL_GPIO_WritePin(FPGA_CLK_GPIO_Port, FPGA_CLK_Pin, GPIO_PIN_SET);
	FPGA_CLK_GPIO_Port->BSRR = FPGA_CLK_Pin;
}

inline void FPGA_clockFall(void)
{
	//HAL_GPIO_WritePin(FPGA_CLK_GPIO_Port, FPGA_CLK_Pin, GPIO_PIN_RESET);
	FPGA_CLK_GPIO_Port->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U);
}

inline uint_fast8_t FPGA_readPacket(void)
{
	if (!FPGA_bus_direction)
		FPGA_setBusInput();
	__asm("nop");
	return (FPGA_BUS_D0_GPIO_Port->IDR & 0xFF);
	/*
	return  (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D0_Pin) == FPGA_BUS_D0_Pin) << 0)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D1_Pin) == FPGA_BUS_D1_Pin) << 1)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D2_Pin) == FPGA_BUS_D2_Pin) << 2)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D3_Pin) == FPGA_BUS_D3_Pin) << 3)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D4_Pin) == FPGA_BUS_D4_Pin) << 4)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D5_Pin) == FPGA_BUS_D5_Pin) << 5)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D6_Pin) == FPGA_BUS_D6_Pin) << 6)
				| (((FPGA_BUS_D0_GPIO_Port->IDR & FPGA_BUS_D7_Pin) == FPGA_BUS_D7_Pin) << 7);
	*/
}

inline void FPGA_writePacket(uint_fast8_t packet)
{
	if (FPGA_bus_direction)
		FPGA_setBusOutput();
	FPGA_BUS_D0_GPIO_Port->BSRR = (packet & 0xFF) | 0xFF0000;
	/*
	FPGA_BUS_D0_GPIO_Port->BSRR =
		(bitRead(packet, 0) << 0 & FPGA_BUS_D0_Pin)
		| (bitRead(packet, 1) << 1 & FPGA_BUS_D1_Pin)
		| (bitRead(packet, 2) << 2 & FPGA_BUS_D2_Pin)
		| (bitRead(packet, 3) << 3 & FPGA_BUS_D3_Pin)
		| (bitRead(packet, 4) << 4 & FPGA_BUS_D4_Pin)
		| (bitRead(packet, 5) << 5 & FPGA_BUS_D5_Pin)
		| (bitRead(packet, 6) << 6 & FPGA_BUS_D6_Pin)
		| (bitRead(packet, 7) << 7 & FPGA_BUS_D7_Pin)
		| ((uint32_t)FPGA_BUS_D7_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D6_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D5_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D4_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D3_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D2_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D1_Pin << 16U)
		| ((uint32_t)FPGA_BUS_D0_Pin << 16U);
	*/
}

void FPGA_start_command(uint_fast8_t command) //выполнение команды к SPI flash
{
	FPGA_busy = true;

	//STAGE 1
	FPGA_writePacket(7); //FPGA FLASH READ command
	GPIOC->BSRR = FPGA_SYNC_Pin;
	FPGA_clockRise();
	GPIOC->BSRR = ((uint32_t)FPGA_CLK_Pin << 16U) | ((uint32_t)FPGA_SYNC_Pin << 16U);
	HAL_Delay(1);

	//STAGE 2
	FPGA_writePacket(command); //SPI FLASH READ STATUS COMMAND
	FPGA_clockRise();
	FPGA_clockFall();
	HAL_Delay(1);
}

uint_fast8_t FPGA_continue_command(uint_fast8_t writedata) //продолжение чтения и записи к SPI flash
{
	//STAGE 3 WRITE
	FPGA_writePacket(writedata);
	FPGA_clockRise();
	FPGA_clockFall();
	HAL_Delay(1);
	//STAGE 4 READ
	FPGA_clockRise();
	FPGA_clockFall();
	uint_fast8_t data = FPGA_readPacket();
	HAL_Delay(1);

	return data;
}

/*
Micron M25P80 Serial Flash COMMANDS:
06h - WRITE ENABLE
04h - WRITE DISABLE
9Fh - READ IDENTIFICATION
9Eh - READ IDENTIFICATION
05h - READ STATUS REGISTER
01h - WRITE STATUS REGISTER
03h - READ DATA BYTES
0Bh - READ DATA BYTES at HIGHER SPEED
02h - PAGE PROGRAM
D8h - SECTOR ERASE
C7h - BULK ERASE
B9h - DEEP POWER-DOWN
ABh - RELEASE from DEEP POWER-DOWN
*/
void FPGA_read_flash(void) //чтение flash памяти
{
	FPGA_busy = true;
	//FPGA_start_command(0xB9);
	FPGA_start_command(0xAB);
	//FPGA_start_command(0x04);
	FPGA_start_command(0x06);
	FPGA_start_command(0x05);
	//FPGA_start_command(0x03); // READ DATA BYTES
	//FPGA_continue_command(0x00); //addr 1
	//FPGA_continue_command(0x00); //addr 2
	//FPGA_continue_command(0x00); //addr 3

	for (uint_fast16_t i = 1; i <= 512; i++)
	{
		uint_fast8_t data = FPGA_continue_command(0x05);
		sendToDebug_hex(data, true);
		sendToDebug_str(" ");
		if (i % 16 == 0)
		{
			sendToDebug_str("\r\n");
			HAL_IWDG_Refresh(&hiwdg1);
			DEBUG_Transmit_FIFO_Events();
		}
		//HAL_IWDG_Refresh(&hiwdg1);
		//DEBUG_Transmit_FIFO_Events();
	}
	sendToDebug_newline();

	FPGA_busy = false;
}
