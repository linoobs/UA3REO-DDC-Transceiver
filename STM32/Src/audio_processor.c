#include "audio_processor.h"
#include "wm8731.h"
#include "audio_filters.h"
#include "agc.h"
#include "settings.h"
#include "usbd_audio_if.h"
#include "noise_reduction.h"
#include "noise_blanker.h"
#include "auto_notch.h"
#include "decoder.h"
#include "vad.h"
#include "trx_manager.h"
#include "sd.h"
#include "vocoder.h"
#include "rf_unit.h"

// Public variables
volatile uint32_t AUDIOPROC_samples = 0;	  // audio samples processed in the processor
volatile bool Processor_NeedRXBuffer = false; // codec needs data from processor for RX
volatile bool Processor_NeedTXBuffer = false; // codec needs data from processor for TX

float32_t APROC_Audio_Buffer_RX1_accum_Q[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0}; // copy of the working part of the FPGA buffers for processing
float32_t APROC_Audio_Buffer_RX1_accum_I[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};
float32_t APROC_Audio_Buffer_RX2_accum_Q[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};
float32_t APROC_Audio_Buffer_RX2_accum_I[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};

float32_t APROC_Audio_Buffer_RX1_Q[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0}; // decimated and ready to demod buffer
float32_t APROC_Audio_Buffer_RX1_I[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};
float32_t APROC_Audio_Buffer_RX2_Q[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};
float32_t APROC_Audio_Buffer_RX2_I[FPGA_RX_IQ_BUFFER_HALF_SIZE] = {0};

float32_t APROC_Audio_Buffer_TX_Q[FPGA_TX_IQ_BUFFER_HALF_SIZE] = {0};
float32_t APROC_Audio_Buffer_TX_I[FPGA_TX_IQ_BUFFER_HALF_SIZE] = {0};
volatile float32_t Processor_RX1_Power_value;	   // RX signal magnitude
volatile float32_t Processor_RX2_Power_value;	   // RX signal magnitude
volatile float32_t Processor_TX_MAX_amplitude_OUT; // TX uplift after ALC
bool NeedReinitReverber = false;
bool APROC_IFGain_Overflow = false;
float32_t APROC_TX_clip_gain = 1.0f;
float32_t APROC_TX_ALC_IN_clip_gain = 1.0f;

// Private variables
static int32_t APROC_AudioBuffer_out[AUDIO_BUFFER_SIZE] = {0};																				   // output buffer of the audio processor
static float32_t DFM_RX1_i_prev = 0, DFM_RX1_q_prev = 0, DFM_RX2_i_prev = 0, DFM_RX2_q_prev = 0, DFM_RX1_emph_prev = 0, DFM_RX2_emph_prev = 0; // used in FM detection and low / high pass processing
bool DFM_RX1_Squelched = false, DFM_RX2_Squelched = false;
static float32_t DFM_RX1_SquelchRate = 1.0f, DFM_RX2_SquelchRate = 1.0f;
static float32_t current_if_gain = 0.0f;
static float32_t volume_gain = 0.0f;
IRAM2 static float32_t Processor_Reverber_Buffer[AUDIO_BUFFER_HALF_SIZE * AUDIO_MAX_REVERBER_TAPS] = {0};
static bool preprocessor_buffer_ready = false;

// Prototypes
static void doRX_HILBERT(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																							  // Hilbert filter for phase shift of signals
static void doRX_LPF_IQ(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																							  // Low-pass filter for I and Q
static void doRX_LPF_I(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																								  // LPF filter for I
static void doRX_GAUSS_IQ(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																							  // Gauss filter for I
static void doRX_HPF_I(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																								  // HPF filter for I
static void doRX_DNR(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																								  // Digital Noise Reduction
static void doRX_AGC(AUDIO_PROC_RX_NUM rx_id, uint16_t size, uint_fast8_t mode, bool stereo);																			  // automatic gain control
static void doRX_NOTCH(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																								  // notch filter
static void doRX_NoiseBlanker(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																						  // impulse noise suppressor
static void doRX_SMETER(AUDIO_PROC_RX_NUM rx_id, float32_t *buff, uint16_t size, bool if_gained);																					  // s-meter
static void doRX_COPYCHANNEL(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																						  // copy I to Q channel
static void DemodulateFM(float32_t *data_i, float32_t *data_q, AUDIO_PROC_RX_NUM rx_id, uint16_t size, bool wfm, float32_t dbm);											  // FM demodulator
static void ModulateFM(uint16_t size, float32_t amplitude);																									  // FM modulator
static void doRX_EQ(uint16_t size);																															  // receiver equalizer
static void doMIC_EQ(uint16_t size, uint8_t mode);																											  // microphone equalizer
static void doVAD(uint16_t size);																															  // voice activity detector
static void doRX_IFGain(AUDIO_PROC_RX_NUM rx_id, uint16_t size);																							  //IF gain
static void doRX_DecimateInput(AUDIO_PROC_RX_NUM rx_id, float32_t *in_i, float32_t *in_q, float32_t *out_i, float32_t *out_q, uint16_t size, uint8_t factor); //decimate RX samplerate input stream
static void doRX_FreqTransition(AUDIO_PROC_RX_NUM rx_id, uint16_t size, float32_t freq_diff);
static void APROC_SD_Play(void);
static bool APROC_SD_PlayTX(void);
static void doRX_DemodSAM(AUDIO_PROC_RX_NUM rx_id, float32_t *i_buffer, float32_t *q_buffer, float32_t *out_buffer_l, float32_t *out_buffer_r, int16_t blockSize);

// initialize audio processor
void initAudioProcessor(void)
{
	InitAudioFilters();
	DECODER_Init();
	NeedReinitReverber = true;
	ADPCM_Init();
}

// RX decimator and preprocessor
void preProcessRxAudio(void)
{
	if (!FPGA_RX_buffer_ready)
		return;
	if (preprocessor_buffer_ready)
		return;

	AUDIOPROC_samples++;

	// copy buffer from FPGA
	float32_t *FPGA_Audio_Buffer_RX1_I_current = !FPGA_RX_Buffer_Current ? (float32_t *)&FPGA_Audio_Buffer_RX1_I_A : (float32_t *)&FPGA_Audio_Buffer_RX1_I_B;
	float32_t *FPGA_Audio_Buffer_RX1_Q_current = !FPGA_RX_Buffer_Current ? (float32_t *)&FPGA_Audio_Buffer_RX1_Q_A : (float32_t *)&FPGA_Audio_Buffer_RX1_Q_B;
	float32_t *FPGA_Audio_Buffer_RX2_I_current = !FPGA_RX_Buffer_Current ? (float32_t *)&FPGA_Audio_Buffer_RX2_I_A : (float32_t *)&FPGA_Audio_Buffer_RX2_I_B;
	float32_t *FPGA_Audio_Buffer_RX2_Q_current = !FPGA_RX_Buffer_Current ? (float32_t *)&FPGA_Audio_Buffer_RX2_Q_A : (float32_t *)&FPGA_Audio_Buffer_RX2_Q_B;

	//Get and decimate input
	uint32_t need_decimate_rate = TRX_GetRXSampleRate / TRX_SAMPLERATE;
	static uint32_t audio_buffer_in_index = 0;
	if (audio_buffer_in_index + (FPGA_RX_IQ_BUFFER_HALF_SIZE / need_decimate_rate) > FPGA_RX_IQ_BUFFER_HALF_SIZE)
		audio_buffer_in_index = 0;

	if (CurrentVFO->Mode == TRX_MODE_WFM)
	{
		//calc s-meter
		doRX_SMETER(AUDIO_RX1, FPGA_Audio_Buffer_RX1_I_current, FPGA_RX_IQ_BUFFER_HALF_SIZE, false);
		//demodulate wfm before decimation
		DemodulateFM(FPGA_Audio_Buffer_RX1_I_current, FPGA_Audio_Buffer_RX1_Q_current, AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, true, TRX_RX1_dBm);
		//RDS Decoder
		DECODER_PutSamples(FPGA_Audio_Buffer_RX1_I_current, FPGA_RX_IQ_BUFFER_HALF_SIZE);
	}
	doRX_DecimateInput(AUDIO_RX1, FPGA_Audio_Buffer_RX1_I_current, FPGA_Audio_Buffer_RX1_Q_current, &APROC_Audio_Buffer_RX1_accum_I[audio_buffer_in_index], &APROC_Audio_Buffer_RX1_accum_Q[audio_buffer_in_index], FPGA_RX_IQ_BUFFER_HALF_SIZE, need_decimate_rate);

	if (TRX.Dual_RX)
	{
		if (SecondaryVFO->Mode == TRX_MODE_WFM)
		{
			//calc s-meter
			doRX_SMETER(AUDIO_RX2, FPGA_Audio_Buffer_RX2_I_current, FPGA_RX_IQ_BUFFER_HALF_SIZE, false);
			//demodulate wfm before decimation
			DemodulateFM(FPGA_Audio_Buffer_RX2_I_current, FPGA_Audio_Buffer_RX2_Q_current, AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, true, TRX_RX2_dBm);
		}
		doRX_DecimateInput(AUDIO_RX2, FPGA_Audio_Buffer_RX2_I_current, FPGA_Audio_Buffer_RX2_Q_current, &APROC_Audio_Buffer_RX2_accum_I[audio_buffer_in_index], &APROC_Audio_Buffer_RX2_accum_Q[audio_buffer_in_index], FPGA_RX_IQ_BUFFER_HALF_SIZE, need_decimate_rate);
	}

	FPGA_RX_buffer_ready = false; // start processing of new buffer

	audio_buffer_in_index += FPGA_RX_IQ_BUFFER_HALF_SIZE / need_decimate_rate;
	if (audio_buffer_in_index >= FPGA_RX_IQ_BUFFER_HALF_SIZE)
		audio_buffer_in_index = 0;
	else
		return;

	preprocessor_buffer_ready = true;
}

// start audio processor for RX
void processRxAudio(void)
{
	if (!Processor_NeedRXBuffer)
		return;
	if (!preprocessor_buffer_ready)
		return;

	if (SD_PlayInProcess)
	{
		APROC_SD_Play();
		return;
	}

	//get data from preprocessor
	dma_memcpy(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_accum_I, sizeof(APROC_Audio_Buffer_RX1_I));
	dma_memcpy(APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_accum_Q, sizeof(APROC_Audio_Buffer_RX1_Q));
	dma_memcpy(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_accum_I, sizeof(APROC_Audio_Buffer_RX2_I));
	dma_memcpy(APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_accum_Q, sizeof(APROC_Audio_Buffer_RX2_Q));
	preprocessor_buffer_ready = false;

	//Process DC corrector filter
	if (CurrentVFO->Mode != TRX_MODE_AM && CurrentVFO->Mode != TRX_MODE_SAM && CurrentVFO->Mode != TRX_MODE_NFM && CurrentVFO->Mode != TRX_MODE_WFM)
	{
		//dc_filter(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX1_I);
		//dc_filter(APROC_Audio_Buffer_RX1_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX1_Q);
	}
	if (TRX.Dual_RX && SecondaryVFO->Mode != TRX_MODE_AM && SecondaryVFO->Mode != TRX_MODE_SAM && SecondaryVFO->Mode != TRX_MODE_NFM && SecondaryVFO->Mode != TRX_MODE_WFM)
	{
		//dc_filter(APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX2_I);
		//dc_filter(APROC_Audio_Buffer_RX2_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX2_Q);
	}

	//IQ Phase corrector https://github.com/df8oe/UHSDR/wiki/IQ---correction-and-mirror-frequencies
	float32_t teta1_new = 0;
	float32_t teta3_new = 0;
	static float32_t teta1 = 0;
	static float32_t teta3 = 0;
	for (uint16_t i = 0; i < FPGA_RX_IQ_BUFFER_HALF_SIZE; i++)
	{
		teta1_new += APROC_Audio_Buffer_RX1_Q[i] * (APROC_Audio_Buffer_RX1_I[i] < 0.0f ? -1.0f : 1.0f);
		teta3_new += APROC_Audio_Buffer_RX1_Q[i] * (APROC_Audio_Buffer_RX1_Q[i] < 0.0f ? -1.0f : 1.0f);
	}
	teta1_new = teta1_new / (float32_t)FPGA_RX_IQ_BUFFER_HALF_SIZE;
	teta3_new = teta3_new / (float32_t)FPGA_RX_IQ_BUFFER_HALF_SIZE;
	teta1 = 0.003f * teta1_new + 0.997f * teta1;
	teta3 = 0.003f * teta3_new + 0.997f * teta3;
	if (teta3 > 0.0f)
		TRX_IQ_phase_error = asinf(teta1 / teta3);

	VAD_Muting = false;
	if (CurrentVFO->Mode != TRX_MODE_IQ)
		doRX_IFGain(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);

	switch (CurrentVFO->Mode) // first receiver
	{
	case TRX_MODE_CW:
		doRX_LPF_IQ(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_GAUSS_IQ(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_FreqTransition(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, TRX.CW_Pitch);
		arm_sub_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
		doRX_NOTCH(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NoiseBlanker(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DECODER_PutSamples(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doVAD(FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_LSB:
		doRX_HILBERT(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_sub_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
		doRX_HPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_LPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NOTCH(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NoiseBlanker(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DECODER_PutSamples(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doVAD(FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_DIGI_L:
		doRX_HILBERT(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_sub_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
		doRX_LPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DECODER_PutSamples(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_USB:
		doRX_HILBERT(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_add_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // sum of I and Q - USB
		doRX_HPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_LPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NOTCH(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NoiseBlanker(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DECODER_PutSamples(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doVAD(FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_DIGI_U:
		doRX_HILBERT(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_add_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // sum of I and Q - USB
		doRX_LPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DECODER_PutSamples(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_AM:
		doRX_LPF_IQ(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_mult_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_mult_f32(APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_add_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		//arm_vsqrt_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, FPGA_AUDIO_BUFFER_HALF_SIZE);
		for (uint_fast16_t i = 0; i < FPGA_RX_IQ_BUFFER_HALF_SIZE; i++)
			APROC_Audio_Buffer_RX1_I[i] = sqrtf_f32i(APROC_Audio_Buffer_RX1_I[i]);
		//arm_sqrt_f32(APROC_Audio_Buffer_RX1_I[i], &APROC_Audio_Buffer_RX1_I[i]);
		arm_scale_f32(APROC_Audio_Buffer_RX1_I, 0.5f, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		dc_filter(APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX1_I);
		doRX_NOTCH(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_NoiseBlanker(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doVAD(FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_SAM:
		doRX_LPF_IQ(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DemodSAM(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		doVAD(FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, true);
		break;
	case TRX_MODE_NFM:
		doRX_LPF_IQ(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
		DemodulateFM(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, false, TRX_RX1_dBm); //48khz iq
		doRX_IFGain(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_WFM:
		doRX_LPF_I(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_DNR(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		doRX_AGC(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE, CurrentVFO->Mode, false);
		doRX_COPYCHANNEL(AUDIO_RX1, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		break;
	case TRX_MODE_IQ:
	default:
		doRX_SMETER(AUDIO_RX1, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, false);
		break;
	}

	if (TRX.Dual_RX)
	{
		if (SecondaryVFO->Mode != TRX_MODE_IQ)
			doRX_IFGain(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);

		switch (SecondaryVFO->Mode) // second receiver
		{
		case TRX_MODE_CW:
			doRX_LPF_IQ(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_GAUSS_IQ(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_FreqTransition(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, TRX.CW_Pitch);
			arm_sub_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
			doRX_NOTCH(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NoiseBlanker(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_LSB:
			doRX_HILBERT(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_sub_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
			doRX_HPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_LPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NOTCH(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NoiseBlanker(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_DIGI_L:
			doRX_HILBERT(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_sub_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // difference of I and Q - LSB
			doRX_LPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_USB:
			doRX_HILBERT(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_add_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // sum of I and Q - USB
			doRX_HPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_LPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NOTCH(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NoiseBlanker(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_DIGI_U:
			doRX_HILBERT(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_add_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE); // sum of I and Q - USB
			doRX_LPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_AM:
			doRX_LPF_IQ(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_mult_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_mult_f32(APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			arm_add_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			//arm_vsqrt_f32(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I,FPGA_AUDIO_BUFFER_HALF_SIZE);
			for (uint_fast16_t i = 0; i < FPGA_RX_IQ_BUFFER_HALF_SIZE; i++)
				APROC_Audio_Buffer_RX2_I[i] = sqrtf_f32i(APROC_Audio_Buffer_RX2_I[i]);
			//arm_sqrt_f32(APROC_Audio_Buffer_RX2_I[i], &APROC_Audio_Buffer_RX2_I[i]);
			arm_scale_f32(APROC_Audio_Buffer_RX2_I, 0.5f, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			dc_filter(APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, DC_FILTER_RX2_I);
			doRX_NOTCH(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_NoiseBlanker(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_SAM:
			doRX_LPF_IQ(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_DemodSAM(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, true);
			break;
		case TRX_MODE_NFM:
			doRX_LPF_IQ(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			DemodulateFM(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, false, TRX_RX2_dBm);
			doRX_IFGain(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_WFM:
			doRX_LPF_I(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_DNR(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE);
			doRX_AGC(AUDIO_RX2, FPGA_RX_IQ_BUFFER_HALF_SIZE, SecondaryVFO->Mode, false);
			break;
		case TRX_MODE_IQ:
		default:
			doRX_SMETER(AUDIO_RX2, APROC_Audio_Buffer_RX2_I, FPGA_RX_IQ_BUFFER_HALF_SIZE, true);
			break;
		}
	}

	//Prepare data to DMA

	// addition of signals in double receive mode
	if (TRX.Dual_RX && TRX.Dual_RX_Type == VFO_A_PLUS_B)
	{
		arm_add_f32(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
		arm_scale_f32(APROC_Audio_Buffer_RX1_I, 0.5f, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);
	}

	// receiver equalizer
	if (CurrentVFO->Mode != TRX_MODE_DIGI_L && CurrentVFO->Mode != TRX_MODE_DIGI_U && CurrentVFO->Mode != TRX_MODE_IQ)
		doRX_EQ(FPGA_RX_IQ_BUFFER_HALF_SIZE);

	// muting
	if (TRX_Mute)
		arm_scale_f32(APROC_Audio_Buffer_RX1_I, 0.0f, APROC_Audio_Buffer_RX1_I, FPGA_RX_IQ_BUFFER_HALF_SIZE);

	// create buffers for transmission to the codec
	for (uint_fast16_t i = 0; i < FPGA_RX_IQ_BUFFER_HALF_SIZE; i++)
	{
		if (!TRX.Dual_RX)
		{
			arm_float_to_q31(&APROC_Audio_Buffer_RX1_I[i], &APROC_AudioBuffer_out[i * 2], 1); //left channel
			if (CurrentVFO->Mode == TRX_MODE_IQ)
				arm_float_to_q31(&APROC_Audio_Buffer_RX1_Q[i], &APROC_AudioBuffer_out[i * 2 + 1], 1); //right channel
			else
				arm_float_to_q31(&APROC_Audio_Buffer_RX1_I[i], &APROC_AudioBuffer_out[i * 2 + 1], 1); //right channel
		}
		else if (TRX.Dual_RX_Type == VFO_A_AND_B)
		{
			if (!TRX.selected_vfo)
			{
				arm_float_to_q31(&APROC_Audio_Buffer_RX1_I[i], &APROC_AudioBuffer_out[i * 2], 1);	  //left channel
				arm_float_to_q31(&APROC_Audio_Buffer_RX2_I[i], &APROC_AudioBuffer_out[i * 2 + 1], 1); //right channel
			}
			else
			{
				arm_float_to_q31(&APROC_Audio_Buffer_RX2_I[i], &APROC_AudioBuffer_out[i * 2], 1);	  //left channel
				arm_float_to_q31(&APROC_Audio_Buffer_RX1_I[i], &APROC_AudioBuffer_out[i * 2 + 1], 1); //right channel
			}
		}
		else if (TRX.Dual_RX_Type == VFO_A_PLUS_B)
		{
			arm_float_to_q31(&APROC_Audio_Buffer_RX1_I[i], &APROC_AudioBuffer_out[i * 2], 1); //left channel
			APROC_AudioBuffer_out[i * 2 + 1] = APROC_AudioBuffer_out[i * 2];				  //right channel
		}
	}

	//Send to USB Audio
	if (USB_AUDIO_need_rx_buffer && TRX_Inited)
	{
		uint8_t *USB_AUDIO_rx_buffer_current = USB_AUDIO_rx_buffer_b;
		if (!USB_AUDIO_current_rx_buffer)
			USB_AUDIO_rx_buffer_current = USB_AUDIO_rx_buffer_a;

		//drop LSB 32b->24b
		for (uint_fast16_t i = 0; i < (USB_AUDIO_RX_BUFFER_SIZE / BYTES_IN_SAMPLE_AUDIO_OUT_PACKET); i++)
		{
			USB_AUDIO_rx_buffer_current[i * BYTES_IN_SAMPLE_AUDIO_OUT_PACKET + 0] = (APROC_AudioBuffer_out[i] >> 8) & 0xFF;
			USB_AUDIO_rx_buffer_current[i * BYTES_IN_SAMPLE_AUDIO_OUT_PACKET + 1] = (APROC_AudioBuffer_out[i] >> 16) & 0xFF;
			USB_AUDIO_rx_buffer_current[i * BYTES_IN_SAMPLE_AUDIO_OUT_PACKET + 2] = (APROC_AudioBuffer_out[i] >> 24) & 0xFF;
		}
		USB_AUDIO_need_rx_buffer = false;
	}

	//OUT Volume
	float32_t volume_gain_new = volume2rate((float32_t)TRX_Volume / 1023.0f);
	volume_gain = 0.9f * volume_gain + 0.1f * volume_gain_new;

	//SD card send
	if (SD_RecordInProcess)
	{
		//FPGA_RX_IQ_BUFFER_HALF_SIZE - 192
		for (uint_fast16_t i = 0; i < FPGA_RX_IQ_BUFFER_HALF_SIZE; i++)
		{
			arm_float_to_q15(&APROC_Audio_Buffer_RX1_I[i], &VOCODER_Buffer[VOCODER_Buffer_Index], 1); //left channel
			VOCODER_Buffer_Index++;
			if (VOCODER_Buffer_Index == SIZE_ADPCM_BLOCK)
			{
				VOCODER_Buffer_Index = 0;
				VOCODER_Process();
			}
		}
	}

	//Volume Gain and SPI convert
	for (uint_fast16_t i = 0; i < (FPGA_RX_IQ_BUFFER_HALF_SIZE * 2); i++)
	{
		//Gain
		APROC_AudioBuffer_out[i] = (int32_t)((float32_t)APROC_AudioBuffer_out[i] * volume_gain);
		//Codec SPI
		APROC_AudioBuffer_out[i] = convertToSPIBigEndian(APROC_AudioBuffer_out[i]); //for 32bit audio
	}

	//Beep signal
	static float32_t beep_index = 0;
	static uint32_t beep_samples = 0;
	if (WM8731_Beeping)
	{
		float32_t old_signal = 0;
		int32_t out = 0;
		bool halted = false;
		float32_t amplitude = volume2rate((float32_t)TRX_Volume / 1023.0f) * 0.1f;
		for (uint32_t pos = 0; pos < AUDIO_BUFFER_HALF_SIZE; pos++)
		{
			float32_t signal = generateSin(amplitude, &beep_index, TRX_SAMPLERATE, 1500);
			arm_float_to_q31(&signal, &out, 1);
			APROC_AudioBuffer_out[pos * 2] = convertToSPIBigEndian(out);		 //left channel
			APROC_AudioBuffer_out[pos * 2 + 1] = APROC_AudioBuffer_out[pos * 2]; //right channel
			if (beep_samples >= 20 && old_signal < 0 && signal >= 0)
			{
				halted = true;
				break;
			}
			old_signal = signal;
		}
		beep_samples++;
		if (halted)
		{
			beep_index = 0;
			beep_samples = 0;
			WM8731_Beeping = false;
		}
	}

	//Mute codec
	if (WM8731_Muting)
	{
		for (uint32_t pos = 0; pos < AUDIO_BUFFER_HALF_SIZE; pos++)
		{
			APROC_AudioBuffer_out[pos * 2] = 0;		//left channel
			APROC_AudioBuffer_out[pos * 2 + 1] = 0; //right channel
		}
	}

	//Send to Codec DMA
	if (TRX_Inited)
	{
		Aligned_CleanDCache_by_Addr((uint32_t *)&APROC_AudioBuffer_out[0], sizeof(APROC_AudioBuffer_out));
		if (WM8731_DMA_state) //complete
		{
			HAL_MDMA_Start_IT(&hmdma_mdma_channel41_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[AUDIO_BUFFER_SIZE], CODEC_AUDIO_BUFFER_HALF_SIZE * 4, 1); //*2 -> left_right
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel41_sw_0);
		}
		else //half
		{
			HAL_MDMA_Start_IT(&hmdma_mdma_channel42_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[0], CODEC_AUDIO_BUFFER_HALF_SIZE * 4, 1); //*2 -> left_right
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel42_sw_0);
		}
	}

	Processor_NeedRXBuffer = false;
}

// start audio processor for TX
void processTxAudio(void)
{
	if (!Processor_NeedTXBuffer)
		return;
	AUDIOPROC_samples++;
	uint_fast8_t mode = CurrentVFO->Mode;

	if (getInputType() == TRX_INPUT_USB) //USB AUDIO
	{
		uint32_t buffer_index = USB_AUDIO_GetTXBufferIndex_FS() / BYTES_IN_SAMPLE_AUDIO_OUT_PACKET; //buffer 8bit, data 24 bit
		if ((buffer_index % BYTES_IN_SAMPLE_AUDIO_OUT_PACKET) == 1)
			buffer_index -= (buffer_index % BYTES_IN_SAMPLE_AUDIO_OUT_PACKET);
		readHalfFromCircleUSBBuffer24Bit(&USB_AUDIO_tx_buffer[0], &APROC_AudioBuffer_out[0], buffer_index, (USB_AUDIO_TX_BUFFER_SIZE / BYTES_IN_SAMPLE_AUDIO_OUT_PACKET));
	}
	else //AUDIO CODEC AUDIO
	{
		uint32_t dma_index = CODEC_AUDIO_BUFFER_SIZE - (uint16_t)__HAL_DMA_GET_COUNTER(hi2s3.hdmarx);
		if ((dma_index % 2) == 1)
			dma_index--;
		readFromCircleBuffer32((uint32_t *)&CODEC_Audio_Buffer_TX[0], (uint32_t *)&APROC_AudioBuffer_out[0], dma_index, CODEC_AUDIO_BUFFER_SIZE, AUDIO_BUFFER_SIZE);
	}

	//Play CQ message
	if (SD_PlayCQMessageInProcess && SD_PlayInProcess)
	{
		if (!APROC_SD_PlayTX()) //false - data not ready
			return;
	}

	//One-signal zero-tune generator
	if (TRX_Tune && !TRX.TWO_SIGNAL_TUNE)
	{
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			APROC_Audio_Buffer_TX_I[i] = 1.0f;
			APROC_Audio_Buffer_TX_Q[i] = 0.0f;
		}
	}

	//Two-signal tune generator
	if (TRX_Tune && TRX.TWO_SIGNAL_TUNE)
	{
		static float32_t two_signal_gen_index1 = 0;
		static float32_t two_signal_gen_index2 = 0;
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			float32_t point = generateSin(0.5f, &two_signal_gen_index1, TRX_SAMPLERATE, 1000);
			point += generateSin(0.5f, &two_signal_gen_index2, TRX_SAMPLERATE, 2000);
			APROC_Audio_Buffer_TX_I[i] = point;
			APROC_Audio_Buffer_TX_Q[i] = point;
		}
		//hilbert fir
		if (mode == TRX_MODE_LSB || mode == TRX_MODE_DIGI_L || mode == TRX_MODE_CW)
		{
			// + 45 deg to I data
			arm_fir_f32(&FIR_TX_Hilbert_I, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			// - 45 deg to Q data
			arm_fir_f32(&FIR_TX_Hilbert_Q, APROC_Audio_Buffer_TX_Q, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
		}
		else
		{
			// + 45 deg to Q data
			arm_fir_f32(&FIR_TX_Hilbert_Q, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			// - 45 deg to I data
			arm_fir_f32(&FIR_TX_Hilbert_I, APROC_Audio_Buffer_TX_Q, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
		}
	}

	//FM tone generator
	if (TRX_Tune && (mode == TRX_MODE_NFM || mode == TRX_MODE_WFM))
	{
		static uint32_t tone_counter = 100;
		tone_counter++;
		if (tone_counter >= 400)
			tone_counter = 0;
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			static float32_t fm_signal_gen_index = 0;
			float32_t point = 0.0f;
			if (tone_counter > 300)
				point = generateSin(1.0f, &fm_signal_gen_index, TRX_SAMPLERATE, 3500);
			else if (tone_counter > 200)
				point = generateSin(1.0f, &fm_signal_gen_index, TRX_SAMPLERATE, 2000);
			else if (tone_counter > 100)
				point = generateSin(1.0f, &fm_signal_gen_index, TRX_SAMPLERATE, 1000);

			APROC_Audio_Buffer_TX_I[i] = point;
			APROC_Audio_Buffer_TX_Q[i] = point;
		}
		ModulateFM(AUDIO_BUFFER_HALF_SIZE, 1.0f);
	}

	if (!TRX_Tune)
	{
		//Copy and convert buffer
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			APROC_Audio_Buffer_TX_I[i] = (float32_t)APROC_AudioBuffer_out[i * 2] / 2147483648.0f;
			APROC_Audio_Buffer_TX_Q[i] = (float32_t)APROC_AudioBuffer_out[i * 2 + 1] / 2147483648.0f;
		}

		if (getInputType() == TRX_INPUT_MIC)
		{
			//Mic Gain
			arm_scale_f32(APROC_Audio_Buffer_TX_I, TRX.MIC_GAIN, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			arm_scale_f32(APROC_Audio_Buffer_TX_Q, TRX.MIC_GAIN, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
			//Mic Equalizer
			if (mode != TRX_MODE_DIGI_L && mode != TRX_MODE_DIGI_U && mode != TRX_MODE_IQ)
				doMIC_EQ(AUDIO_BUFFER_HALF_SIZE, mode);
		}
		//USB Gain (24bit)
		if (getInputType() == TRX_INPUT_USB)
		{
			arm_scale_f32(APROC_Audio_Buffer_TX_I, 10.0f, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			arm_scale_f32(APROC_Audio_Buffer_TX_Q, 10.0f, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
		}

		//Process DC corrector filter
		dc_filter(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE, DC_FILTER_TX_I);
		dc_filter(APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE, DC_FILTER_TX_Q);
	}

	if (mode != TRX_MODE_IQ && !TRX_Tune)
	{
		//IIR HPF
		if (CurrentVFO->HPF_Filter_Width > 0)
			arm_biquad_cascade_df2T_f32_rolled(&IIR_TX_HPF_I, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
		//IIR LPF
		if (CurrentVFO->LPF_TX_Filter_Width > 0)
			arm_biquad_cascade_df2T_f32_rolled(&IIR_TX_LPF_I, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);

		//TX AGC (compressor)
		if (mode == TRX_MODE_AM || mode == TRX_MODE_SAM)
		{
			DoTxAGC(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE, 0.7f, mode);
			arm_scale_f32(APROC_Audio_Buffer_TX_I, ((float32_t)CALIBRATE.AM_MODULATION_INDEX / 200.0f) * APROC_TX_clip_gain, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
		}
		else
			DoTxAGC(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE, 1.00f * APROC_TX_clip_gain, mode);

		//double left and right channel
		dma_memcpy(&APROC_Audio_Buffer_TX_Q[0], &APROC_Audio_Buffer_TX_I[0], AUDIO_BUFFER_HALF_SIZE * 4);

		switch (mode)
		{
		case TRX_MODE_CW:
			for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
			{
				APROC_Audio_Buffer_TX_I[i] = TRX_GenerateCWSignal(1.0f);
				APROC_Audio_Buffer_TX_Q[i] = 0.0f;
			}
			DECODER_PutSamples(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE); //отправляем данные в цифровой декодер
			break;
		case TRX_MODE_USB:
		case TRX_MODE_DIGI_U:
			//hilbert fir
			// + 45 deg to Q data
			arm_fir_f32(&FIR_TX_Hilbert_Q, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			// - 45 deg to I data
			arm_fir_f32(&FIR_TX_Hilbert_I, APROC_Audio_Buffer_TX_Q, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
			break;
		case TRX_MODE_LSB:
		case TRX_MODE_DIGI_L:
			//hilbert fir
			// + 45 deg to I data
			arm_fir_f32(&FIR_TX_Hilbert_I, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			// - 45 deg to Q data
			arm_fir_f32(&FIR_TX_Hilbert_Q, APROC_Audio_Buffer_TX_Q, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
			break;
		case TRX_MODE_AM:
		case TRX_MODE_SAM:
			// + 45 deg to I data
			arm_fir_f32(&FIR_TX_Hilbert_I, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			// - 45 deg to Q data
			arm_fir_f32(&FIR_TX_Hilbert_Q, APROC_Audio_Buffer_TX_Q, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
			for (size_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
			{
				float32_t i_am = ((APROC_Audio_Buffer_TX_I[i] - APROC_Audio_Buffer_TX_Q[i]) + 1.0f * APROC_TX_clip_gain);
				float32_t q_am = ((APROC_Audio_Buffer_TX_Q[i] - APROC_Audio_Buffer_TX_I[i]) - 1.0f * APROC_TX_clip_gain);
				APROC_Audio_Buffer_TX_I[i] = i_am;
				APROC_Audio_Buffer_TX_Q[i] = q_am;
			}
			break;
		case TRX_MODE_NFM:
		case TRX_MODE_WFM:
			ModulateFM(AUDIO_BUFFER_HALF_SIZE, 1.0f);
			break;
		case TRX_MODE_LOOPBACK:
			DECODER_PutSamples(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE); //отправляем данные в цифровой декодер
			break;
		default:
			break;
		}
	}

	//Send TX data to FFT
	float32_t *FFTInput_I_current = FFT_buff_current ? (float32_t *)&FFTInput_I_A : (float32_t *)&FFTInput_I_B;
	float32_t *FFTInput_Q_current = FFT_buff_current ? (float32_t *)&FFTInput_Q_A : (float32_t *)&FFTInput_Q_B;
	for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
	{
		FFTInput_I_current[FFT_buff_index] = APROC_Audio_Buffer_TX_I[i];
		FFTInput_Q_current[FFT_buff_index] = APROC_Audio_Buffer_TX_Q[i];

		FFT_buff_index++;
		if (FFT_buff_index >= FFT_HALF_SIZE)
		{
			FFT_buff_index = 0;
			if (FFT_new_buffer_ready)
			{
				//sendToDebug_str("fft overrun");
			}
			else
			{
				FFT_new_buffer_ready = true;
				FFT_buff_current = !FFT_buff_current;
				FFTInput_I_current = FFT_buff_current ? (float32_t *)&FFTInput_I_A : (float32_t *)&FFTInput_I_B;
				FFTInput_Q_current = FFT_buff_current ? (float32_t *)&FFTInput_Q_A : (float32_t *)&FFTInput_Q_B;
			}
		}
	}

	//SD card send
	if (SD_RecordInProcess)
	{
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			arm_float_to_q15(&APROC_Audio_Buffer_TX_I[i], &VOCODER_Buffer[VOCODER_Buffer_Index], 1); //left channel
			VOCODER_Buffer_Index++;
			if (VOCODER_Buffer_Index == SIZE_ADPCM_BLOCK)
			{
				VOCODER_Buffer_Index = 0;
				VOCODER_Process();
			}
		}
	}

	//Loopback/DIGI mode self-hearing
	if ((!SD_RecordInProcess && mode == TRX_MODE_LOOPBACK) || mode == TRX_MODE_DIGI_L || mode == TRX_MODE_DIGI_U)
	{
		float32_t volume_gain_tx = volume2rate((float32_t)TRX_Volume / 1023.0f);
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			float32_t sample = APROC_Audio_Buffer_TX_I[i] * volume_gain_tx * db2rateV(TRX.AGC_GAIN_TARGET);
			arm_float_to_q31(&sample, &APROC_AudioBuffer_out[i * 2], 1);
			APROC_AudioBuffer_out[i * 2] = convertToSPIBigEndian(APROC_AudioBuffer_out[i * 2]); //left channel
			APROC_AudioBuffer_out[i * 2 + 1] = APROC_AudioBuffer_out[i * 2];					//right channel
		}
		
		Aligned_CleanDCache_by_Addr((uint32_t *)&APROC_AudioBuffer_out[0], sizeof(APROC_AudioBuffer_out));
		if (WM8731_DMA_state) //compleate
		{
			HAL_MDMA_Start(&hmdma_mdma_channel41_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel41_sw_0);
		}
		else //half
		{
			HAL_MDMA_Start(&hmdma_mdma_channel42_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[0], AUDIO_BUFFER_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel42_sw_0);
		}
	}

	//// RF PowerControl (Audio Level Control)

	// amplitude for the selected power and range
	float32_t RFpower_amplitude = log10f_fast(((float32_t)TRX.RF_Power * 0.9f + 10.0f) / 10.0f) * TRX_MAX_TX_Amplitude;
	if (RFpower_amplitude < 0.0f || TRX.RF_Power == 0)
		RFpower_amplitude = 0.0f;
	if ((mode == TRX_MODE_AM || mode == TRX_MODE_SAM) && !TRX_Tune)
		RFpower_amplitude = RFpower_amplitude * 0.7f;

	//Tune power regulator
	if (TRX_Tune)
	{
		static float32_t tune_power = 0;
		float32_t full_power = TRX_PWR_Forward + TRX_PWR_Backward;

		if (fabsf(full_power - (float32_t)CALIBRATE.TUNE_MAX_POWER) > 0.1f && !ATU_TunePowerStabilized) //histeresis
		{
			if (full_power < CALIBRATE.TUNE_MAX_POWER && tune_power < RFpower_amplitude)
				tune_power = 0.993f * tune_power + 0.007f * RFpower_amplitude;
			if ((TRX_PWR_Forward + TRX_PWR_Backward) > CALIBRATE.TUNE_MAX_POWER && tune_power < RFpower_amplitude)
				tune_power = 0.993f * tune_power;
		}
		else
			ATU_TunePowerStabilized = true;

		if (tune_power >= RFpower_amplitude * 0.99f)
		{
			tune_power = RFpower_amplitude;
			ATU_TunePowerStabilized = true;
		}

		RFpower_amplitude = tune_power;
	}

	//Apply gain
	arm_scale_f32(APROC_Audio_Buffer_TX_I, RFpower_amplitude * APROC_TX_ALC_IN_clip_gain, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
	arm_scale_f32(APROC_Audio_Buffer_TX_Q, RFpower_amplitude * APROC_TX_ALC_IN_clip_gain, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);

	// looking for a maximum in amplitude
	float32_t ampl_max_i = 0.0f;
	float32_t ampl_max_q = 0.0f;
	float32_t ampl_min_i = 0.0f;
	float32_t ampl_min_q = 0.0f;
	uint32_t tmp_index;
	arm_max_no_idx_f32(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE, &ampl_max_i);
	arm_max_no_idx_f32(APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE, &ampl_max_q);
	arm_min_f32(APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE, &ampl_min_i, &tmp_index);
	arm_min_f32(APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE, &ampl_min_q, &tmp_index);
	float32_t Processor_TX_MAX_amplitude_IN = ampl_max_i;
	if (ampl_max_q > Processor_TX_MAX_amplitude_IN)
		Processor_TX_MAX_amplitude_IN = ampl_max_q;
	if ((-ampl_min_i) > Processor_TX_MAX_amplitude_IN)
		Processor_TX_MAX_amplitude_IN = -ampl_min_i;
	if ((-ampl_min_q) > Processor_TX_MAX_amplitude_IN)
		Processor_TX_MAX_amplitude_IN = -ampl_min_q;

	// calculate the target gain
	Processor_TX_MAX_amplitude_OUT = Processor_TX_MAX_amplitude_IN;
	//println(Processor_TX_MAX_amplitude_IN, " ", RFpower_amplitude, " ", APROC_TX_ALC_IN_clip_gain);
	if (Processor_TX_MAX_amplitude_IN > 0.0f)
	{
		// DAC overload (clipping), sharply reduce the gain
		if (Processor_TX_MAX_amplitude_IN > MAX_TX_AMPLITUDE) //dac range overload or alc input over 1 volt
		{
			//correct gain
			if (APROC_TX_clip_gain > 0.0f)
				APROC_TX_clip_gain -= 0.001f;
			//clip
			float32_t ALC_need_gain_target = (RFpower_amplitude * 0.99f) / Processor_TX_MAX_amplitude_IN;
			println("ALC_CLIP ", Processor_TX_MAX_amplitude_IN / RFpower_amplitude, " NEED ", RFpower_amplitude, " CG ", APROC_TX_clip_gain);
			TRX_DAC_OTR = true;
			// apply gain
			arm_scale_f32(APROC_Audio_Buffer_TX_I, ALC_need_gain_target, APROC_Audio_Buffer_TX_I, AUDIO_BUFFER_HALF_SIZE);
			arm_scale_f32(APROC_Audio_Buffer_TX_Q, ALC_need_gain_target, APROC_Audio_Buffer_TX_Q, AUDIO_BUFFER_HALF_SIZE);
			Processor_TX_MAX_amplitude_OUT = Processor_TX_MAX_amplitude_IN * ALC_need_gain_target;
		}
		else if (APROC_TX_clip_gain < 1.0f && Processor_TX_MAX_amplitude_IN < RFpower_amplitude)
			APROC_TX_clip_gain += 0.0001f;

		// Input External ALC overload, over 1 volt
		if (TRX_ALC_IN > 1.0f)
		{
			//correct gain
			if (APROC_TX_ALC_IN_clip_gain > 0.0f)
				APROC_TX_ALC_IN_clip_gain -= 0.001f;
			//show info to console
			println("EXT_ALC_IN_OVR ", APROC_TX_ALC_IN_clip_gain);
			TRX_DAC_OTR = true;
		}
		else if (APROC_TX_ALC_IN_clip_gain < 1.0f)
			APROC_TX_ALC_IN_clip_gain += 0.001f;
	}

	if (RFpower_amplitude > 0.0f)
		TRX_ALC_OUT = TRX_ALC_OUT * 0.9f + (Processor_TX_MAX_amplitude_OUT / RFpower_amplitude) * 0.1f; //smooth
	else
		TRX_ALC_OUT = 0.0f;
	//RF PowerControl (Audio Level Control) Compressor END

	if (mode != TRX_MODE_LOOPBACK)
	{
		//CW SelfHear
		if (TRX.CW_SelfHear && (TRX.CW_KEYER || TRX_key_serial || TRX_key_dot_hard || TRX_key_dash_hard) && mode == TRX_MODE_CW && !TRX_Tune)
		{
			static float32_t cwgen_index = 0;
			float32_t amplitude = volume2rate((float32_t)TRX_Volume / 1023.0f);
			for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
			{
				float32_t point = generateSin(amplitude * (APROC_Audio_Buffer_TX_I[i] / RFpower_amplitude), &cwgen_index, TRX_SAMPLERATE, TRX.CW_Pitch);
				int32_t sample = 0;
				arm_float_to_q31(&point, &sample, 1);
				int32_t data = convertToSPIBigEndian(sample);
				if (WM8731_DMA_state)
				{
					CODEC_Audio_Buffer_RX[AUDIO_BUFFER_SIZE + i * 2] = data;
					CODEC_Audio_Buffer_RX[AUDIO_BUFFER_SIZE + i * 2 + 1] = data;
				}
				else
				{
					CODEC_Audio_Buffer_RX[i * 2] = data;
					CODEC_Audio_Buffer_RX[i * 2 + 1] = data;
				}
			}
			Aligned_CleanDCache_by_Addr((uint32_t *)&CODEC_Audio_Buffer_RX[0], sizeof(CODEC_Audio_Buffer_RX));
		}
		else if (TRX.CW_SelfHear && mode != TRX_MODE_DIGI_L && mode != TRX_MODE_DIGI_U)
		{
			dma_memset(CODEC_Audio_Buffer_RX, 0x00, sizeof CODEC_Audio_Buffer_RX);
			Aligned_CleanDCache_by_Addr((uint32_t *)&CODEC_Audio_Buffer_RX[0], sizeof(CODEC_Audio_Buffer_RX));
		}
		// delay before the RF signal is applied, so that the relay has time to trigger
		if ((HAL_GetTick() - TRX_TX_StartTime) < CALIBRATE.TX_StartDelay)
		{
			dma_memset((void *)&APROC_Audio_Buffer_TX_I[0], 0x00, sizeof(APROC_Audio_Buffer_TX_I));
			dma_memset((void *)&APROC_Audio_Buffer_TX_Q[0], 0x00, sizeof(APROC_Audio_Buffer_TX_Q));
		}
		//send to FPGA
		Aligned_CleanDCache_by_Addr((uint32_t *)&APROC_Audio_Buffer_TX_I[0], sizeof(APROC_Audio_Buffer_TX_I));
		Aligned_CleanDCache_by_Addr((uint32_t *)&APROC_Audio_Buffer_TX_Q[0], sizeof(APROC_Audio_Buffer_TX_Q));
		Aligned_CleanDCache_by_Addr((uint32_t *)&FPGA_Audio_SendBuffer_I[0], sizeof(FPGA_Audio_SendBuffer_I));
		Aligned_CleanDCache_by_Addr((uint32_t *)&FPGA_Audio_SendBuffer_Q[0], sizeof(FPGA_Audio_SendBuffer_Q));
		if (FPGA_Audio_Buffer_State) //Send to FPGA DMA
		{
			HAL_MDMA_Start(&hmdma_mdma_channel41_sw_0, (uint32_t)&APROC_Audio_Buffer_TX_I[0], (uint32_t)&FPGA_Audio_SendBuffer_I[AUDIO_BUFFER_HALF_SIZE], AUDIO_BUFFER_HALF_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel41_sw_0);
			HAL_MDMA_Start(&hmdma_mdma_channel41_sw_0, (uint32_t)&APROC_Audio_Buffer_TX_Q[0], (uint32_t)&FPGA_Audio_SendBuffer_Q[AUDIO_BUFFER_HALF_SIZE], AUDIO_BUFFER_HALF_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel41_sw_0);
		}
		else
		{
			HAL_MDMA_Start(&hmdma_mdma_channel42_sw_0, (uint32_t)&APROC_Audio_Buffer_TX_I[0], (uint32_t)&FPGA_Audio_SendBuffer_I[0], AUDIO_BUFFER_HALF_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel42_sw_0);
			HAL_MDMA_Start(&hmdma_mdma_channel42_sw_0, (uint32_t)&APROC_Audio_Buffer_TX_Q[0], (uint32_t)&FPGA_Audio_SendBuffer_Q[0], AUDIO_BUFFER_HALF_SIZE * 4, 1);
			SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel42_sw_0);
		}
		Aligned_CleanInvalidateDCache_by_Addr((uint32_t *)&FPGA_Audio_SendBuffer_I[0], sizeof(FPGA_Audio_SendBuffer_I));
		Aligned_CleanInvalidateDCache_by_Addr((uint32_t *)&FPGA_Audio_SendBuffer_Q[0], sizeof(FPGA_Audio_SendBuffer_Q));
	}

	Processor_NeedTXBuffer = false;
	Processor_NeedRXBuffer = false;
	USB_AUDIO_need_rx_buffer = false;
}

// Hilbert filter for phase shift of signals
static void doRX_HILBERT(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		arm_fir_f32(&FIR_RX1_Hilbert_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
		arm_fir_f32(&FIR_RX1_Hilbert_Q, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_Q, size);
	}
	else
	{
		arm_fir_f32(&FIR_RX2_Hilbert_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
		arm_fir_f32(&FIR_RX2_Hilbert_Q, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_Q, size);
	}
}

static void doRX_DecimateInput(AUDIO_PROC_RX_NUM rx_id, float32_t *in_i, float32_t *in_q, float32_t *out_i, float32_t *out_q, uint16_t size, uint8_t factor)
{
	if (factor == 1)
	{
		dma_memcpy(out_i, in_i, size * 4);
		dma_memcpy(out_q, in_q, size * 4);
	}
	else
	{
		if (rx_id == AUDIO_RX1)
		{
			arm_biquad_cascade_df2T_f32_rolled(&DECIMATE_IIR_RX1_AUDIO_I, in_i, in_i, size);
			arm_biquad_cascade_df2T_f32_rolled(&DECIMATE_IIR_RX1_AUDIO_Q, in_q, in_q, size);
			arm_fir_decimate_f32(&DECIMATE_FIR_RX1_AUDIO_I, in_i, out_i, size);
			arm_fir_decimate_f32(&DECIMATE_FIR_RX1_AUDIO_Q, in_q, out_q, size);
		}
		else
		{
			arm_biquad_cascade_df2T_f32_rolled(&DECIMATE_IIR_RX2_AUDIO_I, in_i, in_i, size);
			arm_biquad_cascade_df2T_f32_rolled(&DECIMATE_IIR_RX2_AUDIO_Q, in_q, in_q, size);
			arm_fir_decimate_f32(&DECIMATE_FIR_RX2_AUDIO_I, in_i, out_i, size);
			arm_fir_decimate_f32(&DECIMATE_FIR_RX2_AUDIO_Q, in_q, out_q, size);
		}
	}
}

// Low-pass filter for I and Q
static void doRX_LPF_IQ(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->LPF_RX_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_LPF_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_LPF_Q, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_Q, size);
		}
	}
	else
	{
		if (SecondaryVFO->LPF_RX_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_LPF_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_LPF_Q, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_Q, size);
		}
	}
}

// LPF filter for I
static void doRX_LPF_I(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->LPF_RX_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_LPF_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
		}
	}
	else
	{
		if (SecondaryVFO->LPF_RX_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_LPF_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
		}
	}
}

// Gauss filter for I
static void doRX_GAUSS_IQ(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (!TRX.CW_GaussFilter)
		return;
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->Mode == TRX_MODE_CW)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_GAUSS_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_GAUSS_Q, APROC_Audio_Buffer_RX1_Q, APROC_Audio_Buffer_RX1_Q, size);
		}
	}
	else
	{
		if (SecondaryVFO->Mode == TRX_MODE_CW)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_GAUSS_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_GAUSS_Q, APROC_Audio_Buffer_RX2_Q, APROC_Audio_Buffer_RX2_Q, size);
		}
	}
}

// HPF filter for I
static void doRX_HPF_I(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->HPF_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX1_HPF_I, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
		}
	}
	else
	{
		if (SecondaryVFO->HPF_Filter_Width > 0)
		{
			arm_biquad_cascade_df2T_f32_rolled(&IIR_RX2_HPF_I, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
		}
	}
}

// notch filter
static void doRX_NOTCH(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->ManualNotchFilter) // manual filter
			arm_biquad_cascade_df2T_f32_rolled(&NOTCH_RX1_FILTER, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
		else if (CurrentVFO->AutoNotchFilter) // automatic filter
		{
			for (uint32_t block = 0; block < (size / AUTO_NOTCH_BLOCK_SIZE); block++)
				processAutoNotchReduction(APROC_Audio_Buffer_RX1_I + (block * AUTO_NOTCH_BLOCK_SIZE), rx_id);
		}
	}
	else
	{
		if (SecondaryVFO->ManualNotchFilter)
			arm_biquad_cascade_df2T_f32_rolled(&NOTCH_RX2_FILTER, APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_I, size);
		else if (SecondaryVFO->AutoNotchFilter)
		{
			for (uint32_t block = 0; block < (size / AUTO_NOTCH_BLOCK_SIZE); block++)
				processAutoNotchReduction(APROC_Audio_Buffer_RX2_I + (block * AUTO_NOTCH_BLOCK_SIZE), rx_id);
		}
	}
}

// RX Equalizer
static void doRX_EQ(uint16_t size)
{
	if (TRX.RX_EQ_LOW != 0)
		arm_biquad_cascade_df2T_f32_rolled(&EQ_RX_LOW_FILTER, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
	if (TRX.RX_EQ_MID != 0)
		arm_biquad_cascade_df2T_f32_rolled(&EQ_RX_MID_FILTER, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
	if (TRX.RX_EQ_HIG != 0)
		arm_biquad_cascade_df2T_f32_rolled(&EQ_RX_HIG_FILTER, APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_I, size);
}

// Equalizer microphone
static void doMIC_EQ(uint16_t size, uint8_t mode)
{
	switch (mode)
	{
	case TRX_MODE_LSB:
	case TRX_MODE_USB:
	case TRX_MODE_LOOPBACK:
	default:
		if (TRX.MIC_EQ_LOW_SSB != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_LOW_FILTER_SSB, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		if (TRX.MIC_EQ_MID_SSB != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_MID_FILTER_SSB, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		if (TRX.MIC_EQ_HIG_SSB != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_HIG_FILTER_SSB, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		break;

	case TRX_MODE_NFM:
	case TRX_MODE_WFM:
	case TRX_MODE_AM:
	case TRX_MODE_SAM:
		if (TRX.MIC_EQ_LOW_AMFM != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_LOW_FILTER_AMFM, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		if (TRX.MIC_EQ_MID_AMFM != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_MID_FILTER_AMFM, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		if (TRX.MIC_EQ_HIG_AMFM != 0)
			arm_biquad_cascade_df2T_f32_rolled(&EQ_MIC_HIG_FILTER_AMFM, APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		break;
	}

	//Reverber
	if (TRX.MIC_REVERBER > 0)
	{
		//reset
		static uint32_t reverber_position = 0;
		if (NeedReinitReverber)
		{
			reverber_position = 0;
			dma_memset(Processor_Reverber_Buffer, 0x00, sizeof(Processor_Reverber_Buffer));
			NeedReinitReverber = false;
		}

		//save
		dma_memcpy(&Processor_Reverber_Buffer[reverber_position * size], APROC_Audio_Buffer_TX_I, sizeof(float32_t) * size);

		//move
		reverber_position++;
		if (reverber_position > TRX.MIC_REVERBER)
			reverber_position = 0;

		//apply
		arm_add_f32(&Processor_Reverber_Buffer[reverber_position * size], APROC_Audio_Buffer_TX_I, APROC_Audio_Buffer_TX_I, size);
		arm_scale_f32(APROC_Audio_Buffer_TX_I, 0.5f, APROC_Audio_Buffer_TX_I, size);
	}
}

// Digital Noise Reduction
static void doRX_DNR(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (rx_id == AUDIO_RX1)
	{
		if (CurrentVFO->DNR_Type > 0)
		{
			for (uint32_t block = 0; block < (size / NOISE_REDUCTION_BLOCK_SIZE); block++)
				if (CurrentVFO->DNR_Type == 2)
					processNoiseReduction(APROC_Audio_Buffer_RX1_I + (block * NOISE_REDUCTION_BLOCK_SIZE), rx_id, 2);
				else
					processNoiseReduction(APROC_Audio_Buffer_RX1_I + (block * NOISE_REDUCTION_BLOCK_SIZE), rx_id, 1);
		}
	}
	else
	{
		if (SecondaryVFO->DNR_Type > 0)
		{
			for (uint32_t block = 0; block < (size / NOISE_REDUCTION_BLOCK_SIZE); block++)
				if (SecondaryVFO->DNR_Type == 2)
					processNoiseReduction(APROC_Audio_Buffer_RX2_I + (block * NOISE_REDUCTION_BLOCK_SIZE), rx_id, 2);
				else
					processNoiseReduction(APROC_Audio_Buffer_RX2_I + (block * NOISE_REDUCTION_BLOCK_SIZE), rx_id, 1);
		}
	}
}

// automatic gain control
static void doRX_AGC(AUDIO_PROC_RX_NUM rx_id, uint16_t size, uint_fast8_t mode, bool stereo)
{
	if (rx_id == AUDIO_RX1)
		DoRxAGC(APROC_Audio_Buffer_RX1_I, APROC_Audio_Buffer_RX1_Q, size, rx_id, mode, stereo);
	else
		DoRxAGC(APROC_Audio_Buffer_RX2_I, APROC_Audio_Buffer_RX2_Q, size, rx_id, mode, stereo);
}

// impulse noise suppressor
static void doRX_NoiseBlanker(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	if (!TRX.NOISE_BLANKER)
		return;
	if (rx_id == AUDIO_RX1)
	{
		for (uint32_t block = 0; block < (size / NB_BLOCK_SIZE); block++)
			processNoiseBlanking(APROC_Audio_Buffer_RX1_I + (block * NB_BLOCK_SIZE), rx_id);
	}
	else
	{
		for (uint32_t block = 0; block < (size / NB_BLOCK_SIZE); block++)
			processNoiseBlanking(APROC_Audio_Buffer_RX2_I + (block * NB_BLOCK_SIZE), rx_id);
	}
}

// s-meter
static void doRX_SMETER(AUDIO_PROC_RX_NUM rx_id, float32_t *buff, uint16_t size, bool if_gained)
{
	if (rx_id == AUDIO_RX1)
	{
		if (Processor_RX1_Power_value != 0)
			return;
		// Prepare data to calculate s-meter
		static float32_t i = 0;
		arm_rms_f32(buff, size, &i);
		if (if_gained && current_if_gain != 0.0f)
			i *= 1.0f / current_if_gain;
		Processor_RX1_Power_value = i;
	}
	
	if (rx_id == AUDIO_RX2)
	{
		if (Processor_RX2_Power_value != 0)
			return;
		// Prepare data to calculate s-meter
		static float32_t i = 0;
		arm_rms_f32(buff, size, &i);
		if (if_gained && current_if_gain != 0.0f)
			i *= 1.0f / current_if_gain;
		Processor_RX2_Power_value = i;
	}
}

// copy I to Q channel
static void doRX_COPYCHANNEL(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	// Double channel I-> Q
	if (rx_id == AUDIO_RX1)
	{
		dma_memcpy32((uint32_t *)&APROC_Audio_Buffer_RX1_Q[0], (uint32_t *)&APROC_Audio_Buffer_RX1_I[0], size);
	}
	else
	{
		dma_memcpy32((uint32_t *)&APROC_Audio_Buffer_RX2_Q[0], (uint32_t *)&APROC_Audio_Buffer_RX2_I[0], size);
	}
}

// FM demodulator
static void DemodulateFM(float32_t *data_i, float32_t *data_q, AUDIO_PROC_RX_NUM rx_id, uint16_t size, bool wfm, float32_t dbm)
{
	float32_t *i_prev = &DFM_RX1_i_prev;
	float32_t *q_prev = &DFM_RX1_q_prev;
	float32_t *emph_prev = &DFM_RX1_emph_prev;
	bool *squelched = &DFM_RX1_Squelched;
	float32_t *squelchRate = &DFM_RX1_SquelchRate;
	bool sql_enabled = CurrentVFO->SQL;
	int8_t FM_SQL_threshold_dbm = CurrentVFO->FM_SQL_threshold_dbm;
	float32_t angle, x, y, b;

	if (rx_id == AUDIO_RX2)
	{
		i_prev = &DFM_RX2_i_prev;
		q_prev = &DFM_RX2_q_prev;
		emph_prev = &DFM_RX2_emph_prev;
		squelched = &DFM_RX2_Squelched;
		squelchRate = &DFM_RX2_SquelchRate;
		FM_SQL_threshold_dbm = SecondaryVFO->FM_SQL_threshold_dbm;
		sql_enabled = SecondaryVFO->SQL;
	}

	for (uint_fast16_t i = 0; i < size; i++)
	{
		// first, calculate "x" and "y" for the arctan2, comparing the vectors of present data with previous data
		y = (data_q[i] * *i_prev) - (data_i[i] * *q_prev);
		x = (data_i[i] * *i_prev) + (data_q[i] * *q_prev);
		angle = atan2f(y, x);

		*q_prev = data_q[i]; // save "previous" value of each channel to allow detection of the change of angle in next go-around
		*i_prev = data_i[i];

		//demod
		data_i[i] = (float32_t)(angle / F_PI) * 0.01f;
		//fm de emphasis
		const float32_t alpha = 0.25f;
		data_i[i] = data_i[i] * (1.0f - alpha) + *emph_prev * alpha;
		*emph_prev = data_i[i];

		//smooth SQL edges
		if (!*squelched || !sql_enabled) // high-pass audio only if we are un-squelched (to save processor time)
		{
			if (*squelchRate < 1.00f)
			{
				data_i[i] *= *squelchRate;
				*squelchRate = 1.001f * *squelchRate;
			}
		}
		else if (*squelched) // were we squelched or tone NOT detected?
		{
			if (*squelchRate > 0.01f)
			{
				data_i[i] *= *squelchRate;
				*squelchRate = 0.999f * *squelchRate;
			}
			else
			{
				data_i[i] = 0;
			}
		}
	}

	// *** Squelch Processing ***
	if(FM_SQL_threshold_dbm > dbm && !*squelched)
	{
		*squelchRate = 1.0f;
		*squelched = true; // yes, close the squelch
	}
	else if(FM_SQL_threshold_dbm <= dbm && *squelched)
	{
		*squelchRate = 0.01f;
		*squelched = false; //  yes, open the squelch
	}
}

// FM modulator
static void ModulateFM(uint16_t size, float32_t amplitude)
{
	static float32_t hpf_prev_a = 0;
	static float32_t hpf_prev_b = 0;
	static float32_t fm_mod_accum = 0;
	static float32_t modulation_index = 0;
	if (CurrentVFO->LPF_TX_Filter_Width > 0)
		modulation_index = CurrentVFO->LPF_TX_Filter_Width / (float32_t)TRX_SAMPLERATE * (float32_t)CALIBRATE.FM_DEVIATION_SCALE;
	else
		modulation_index = (float32_t)CALIBRATE.FM_DEVIATION_SCALE;

	//CTCSS
	if (TRX.CTCSS_Freq > 0.0)
	{
		static float32_t CTCSS_gen_index = 0;
		for (uint_fast16_t i = 0; i < AUDIO_BUFFER_HALF_SIZE; i++)
		{
			float32_t point = generateSin(0.25f, &CTCSS_gen_index, TRX_SAMPLERATE, TRX.CTCSS_Freq);
			APROC_Audio_Buffer_TX_I[i] += point;
		}
	}

	for (uint_fast16_t i = 0; i < size; i++)
	{
		hpf_prev_b = FM_TX_HPF_ALPHA * (hpf_prev_b + APROC_Audio_Buffer_TX_I[i] - hpf_prev_a); // do differentiation
		hpf_prev_a = APROC_Audio_Buffer_TX_I[i];											   // save "[n-1] samples for next iteration
		fm_mod_accum += hpf_prev_b * modulation_index;										   // save differentiated data in audio buffer
		while (fm_mod_accum > (2.0f * PI))
			fm_mod_accum -= (2.0f * PI); // limit range
		while (fm_mod_accum < -(2.0f * PI))
			fm_mod_accum += (2.0f * PI); // limit range
		APROC_Audio_Buffer_TX_I[i] = amplitude * arm_sin_f32(fm_mod_accum);
		APROC_Audio_Buffer_TX_Q[i] = amplitude * arm_cos_f32(fm_mod_accum);
	}
}

// voice activity detector
static void doVAD(uint16_t size)
{
	for (uint32_t block = 0; block < (size / VAD_BLOCK_SIZE); block++)
		processVAD(APROC_Audio_Buffer_RX1_I + (block * VAD_BLOCK_SIZE));
}

// Apply IF Gain IF Gain
static void doRX_IFGain(AUDIO_PROC_RX_NUM rx_id, uint16_t size)
{
	float32_t if_gain = db2rateV(TRX.IF_Gain);
	float32_t minVal = 0;
	float32_t maxVal = 0;
	uint32_t index = 0;

	if (rx_id == AUDIO_RX1)
	{
		//overflow protect
		arm_min_f32(APROC_Audio_Buffer_RX1_I, size, &minVal, &index);
		arm_max_no_idx_f32(APROC_Audio_Buffer_RX1_I, size, &maxVal);
		if ((minVal * if_gain) < -1.0f)
		{
			if (!CurrentVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		if ((maxVal * if_gain) > 1.0f)
		{
			if (!CurrentVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		arm_min_f32(APROC_Audio_Buffer_RX1_Q, size, &minVal, &index);
		arm_max_no_idx_f32(APROC_Audio_Buffer_RX1_Q, size, &maxVal);
		if ((minVal * if_gain) < -1.0f)
		{
			if (!CurrentVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		if ((maxVal * if_gain) > 1.0f)
		{
			if (!CurrentVFO->AGC)
				APROC_IFGain_Overflow = true;
		}

		//apply gain
		arm_scale_f32(APROC_Audio_Buffer_RX1_I, if_gain, APROC_Audio_Buffer_RX1_I, size);
		arm_scale_f32(APROC_Audio_Buffer_RX1_Q, if_gain, APROC_Audio_Buffer_RX1_Q, size);
		current_if_gain = if_gain;
	}
	else
	{
		//overflow protect RX2
		arm_min_f32(APROC_Audio_Buffer_RX2_I, size, &minVal, &index);
		arm_max_no_idx_f32(APROC_Audio_Buffer_RX2_I, size, &maxVal);
		if ((minVal * if_gain) < -1.0f)
		{
			if (!SecondaryVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		if ((maxVal * if_gain) > 1.0f)
		{
			if (!SecondaryVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		arm_min_f32(APROC_Audio_Buffer_RX2_Q, size, &minVal, &index);
		arm_max_no_idx_f32(APROC_Audio_Buffer_RX2_Q, size, &maxVal);
		if ((minVal * if_gain) < -1.0f)
		{
			if (!SecondaryVFO->AGC)
				APROC_IFGain_Overflow = true;
		}
		if ((maxVal * if_gain) > 1.0f)
		{
			if (!SecondaryVFO->AGC)
				APROC_IFGain_Overflow = true;
		}

		//apply gain
		arm_scale_f32(APROC_Audio_Buffer_RX2_I, if_gain, APROC_Audio_Buffer_RX2_I, size);
		arm_scale_f32(APROC_Audio_Buffer_RX2_Q, if_gain, APROC_Audio_Buffer_RX2_Q, size);
	}
}

static void APROC_SD_Play(void)
{
	static uint32_t vocoder_index = SIZE_ADPCM_BLOCK;
	static uint32_t outbuff_index = 0;

	if (vocoder_index == SIZE_ADPCM_BLOCK)
	{
		vocoder_index = 0;
		VODECODER_Process();
	}

	if (vocoder_index < SIZE_ADPCM_BLOCK)
	{
		while (vocoder_index < SIZE_ADPCM_BLOCK && outbuff_index < FPGA_RX_IQ_BUFFER_HALF_SIZE)
		{
			APROC_AudioBuffer_out[outbuff_index * 2] = VOCODER_Buffer[vocoder_index] << 16;
			APROC_AudioBuffer_out[outbuff_index * 2 + 1] = APROC_AudioBuffer_out[outbuff_index * 2];
			vocoder_index++;
			outbuff_index++;
		}

		if (outbuff_index == FPGA_RX_IQ_BUFFER_HALF_SIZE)
		{
			outbuff_index = 0;

			//OUT Volume
			float32_t volume_gain_new = volume2rate((float32_t)TRX_Volume / 1023.0f);
			volume_gain = 0.9f * volume_gain + 0.1f * volume_gain_new;

			//Volume Gain and SPI convert
			for (uint_fast16_t i = 0; i < (FPGA_RX_IQ_BUFFER_HALF_SIZE * 2); i++)
			{
				//Gain
				APROC_AudioBuffer_out[i] = (int32_t)((float32_t)APROC_AudioBuffer_out[i] * volume_gain);
				//Codec SPI
				APROC_AudioBuffer_out[i] = convertToSPIBigEndian(APROC_AudioBuffer_out[i]); //for 32bit audio
			}

			//Send to Codec DMA
			if (TRX_Inited)
			{
				Aligned_CleanDCache_by_Addr((uint32_t *)&APROC_AudioBuffer_out[0], sizeof(APROC_AudioBuffer_out));
				if (WM8731_DMA_state) //complete
				{
					HAL_MDMA_Start_IT(&hmdma_mdma_channel41_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[AUDIO_BUFFER_SIZE], CODEC_AUDIO_BUFFER_HALF_SIZE * 4, 1); //*2 -> left_right
					SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel41_sw_0);
				}
				else //half
				{
					HAL_MDMA_Start_IT(&hmdma_mdma_channel42_sw_0, (uint32_t)&APROC_AudioBuffer_out[0], (uint32_t)&CODEC_Audio_Buffer_RX[0], CODEC_AUDIO_BUFFER_HALF_SIZE * 4, 1); //*2 -> left_right
					SLEEPING_MDMA_PollForTransfer(&hmdma_mdma_channel42_sw_0);
				}
			}
			//
			Processor_NeedTXBuffer = false;
			Processor_NeedRXBuffer = false;
			USB_AUDIO_need_rx_buffer = false;
		}
	}
}

static bool APROC_SD_PlayTX(void)
{
	static uint32_t vocoder_index = SIZE_ADPCM_BLOCK;
	static uint32_t outbuff_index = 0;

	if (vocoder_index == SIZE_ADPCM_BLOCK)
	{
		vocoder_index = 0;
		VODECODER_Process();
	}

	if (vocoder_index < SIZE_ADPCM_BLOCK)
	{
		while (vocoder_index < SIZE_ADPCM_BLOCK && outbuff_index < AUDIO_BUFFER_HALF_SIZE)
		{
			APROC_AudioBuffer_out[outbuff_index * 2] = VOCODER_Buffer[vocoder_index] << 16;
			APROC_AudioBuffer_out[outbuff_index * 2 + 1] = APROC_AudioBuffer_out[outbuff_index * 2];
			vocoder_index++;
			outbuff_index++;
		}

		if (outbuff_index == AUDIO_BUFFER_HALF_SIZE)
		{
			outbuff_index = 0;

			//data ready
			return true;
		}
	}

	//data not ready
	return false;
}

static void doRX_FreqTransition(AUDIO_PROC_RX_NUM rx_id, uint16_t size, float32_t freq_diff)
{
	static float32_t gen_position_rx1 = 0.0f;
	static float32_t gen_position_rx2 = 0.0f;

	if (rx_id == AUDIO_RX1)
	{
		for (uint16_t i = 0; i < size; i++)
		{
			float32_t sin = arm_sin_f32(gen_position_rx1 * (2.0f * F_PI));
			float32_t cos = arm_cos_f32(gen_position_rx1 * (2.0f * F_PI));
			APROC_Audio_Buffer_RX1_I[i] *= sin;
			APROC_Audio_Buffer_RX1_Q[i] *= cos;

			gen_position_rx1 += (freq_diff / (float32_t)TRX_SAMPLERATE);
			while (gen_position_rx1 >= 1.0f)
				gen_position_rx1 -= 1.0f;
		}
	}
	else
	{
		for (uint16_t i = 0; i < size; i++)
		{
			float32_t sin = arm_sin_f32(gen_position_rx2 * (2.0f * F_PI));
			float32_t cos = arm_cos_f32(gen_position_rx2 * (2.0f * F_PI));
			APROC_Audio_Buffer_RX2_I[i] *= sin;
			APROC_Audio_Buffer_RX2_Q[i] *= cos;

			gen_position_rx2 += (freq_diff / (float32_t)TRX_SAMPLERATE);
			while (gen_position_rx2 >= 1.0f)
				gen_position_rx2 -= 1.0f;
		}
	}
}

static void doRX_DemodSAM(AUDIO_PROC_RX_NUM rx_id, float32_t *i_buffer, float32_t *q_buffer, float32_t *out_buffer_l, float32_t *out_buffer_r, int16_t blockSize)
{
	//part of UHSDR project https://github.com/df8oe/UHSDR/blob/active-devel/mchf-eclipse/drivers/audio/audio_driver.c
	static demod_sam_data_t sam_data_rx1;
	static demod_sam_data_t sam_data_rx2;
	static demod_sam_data_t *sam_data = &sam_data_rx1;
	if (rx_id == AUDIO_RX2)
		sam_data = &sam_data_rx2;
	
	//sideband separation, these values never change
	static const demod_sam_const_t demod_sam_const =
	{
			.c0 = {
					-0.328201924180698,
					-0.744171491539427,
					-0.923022915444215,
					-0.978490468768238,
					-0.994128272402075,
					-0.998458978159551,
					-0.999790306259206,
			},

			.c1 = {
					-0.0991227952747244,
					-0.565619728761389,
					-0.857467122550052,
					-0.959123933111275,
					-0.988739372718090,
					-0.996959189310611,
					-0.999282492800792,
			},
	};
	
	// definitions and intializations for synchronous AM demodulation = SAM
	if(!sam_data->inited)
	{
		sam_data->inited = true;
		//pll
		sam_data->adb_sam_omega_min = - (2.0 * F_PI * TRX_SAMPLERATE / TRX_SAMPLERATE);
		sam_data->adb_sam_omega_max = (2.0 * F_PI * TRX_SAMPLERATE / TRX_SAMPLERATE);
		sam_data->adb_sam_g1 = (1.0 - expf(-2.0 * SAM_omegaN * SAM_zeta / TRX_SAMPLERATE));
		sam_data->adb_sam_g2 = (- sam_data->adb_sam_g1 + 2.0 * (1 - expf(- SAM_omegaN * SAM_zeta / TRX_SAMPLERATE) * cosf(SAM_omegaN / TRX_SAMPLERATE * sqrtf(1.0 - SAM_zeta * SAM_zeta))));
		//fade leveler
		float32_t tauR = 0.02; // value emperically determined
    float32_t tauI = 1.4;  // value emperically determined
    sam_data->adb_sam_mtauR = (expf(- 1 / ((float32_t)TRX_SAMPLERATE * tauR)));
    sam_data->adb_sam_onem_mtauR = (1.0 - sam_data->adb_sam_mtauR);
    sam_data->adb_sam_mtauI = (expf(- 1 / ((float32_t)TRX_SAMPLERATE * tauI)));
    sam_data->adb_sam_onem_mtauI = (1.0 - sam_data->adb_sam_mtauI);
	}
	// Wheatley 2011 cuteSDR & Warren Pratts WDSP, 2016
	for (int i = 0; i < blockSize; i++)
	{
		// NCO
		float32_t ai, bi, aq, bq;
		float32_t Sin, Cos;

		Sin = arm_sin_f32(sam_data->phs);
		Cos = arm_cos_f32(sam_data->phs);
		ai = Cos * i_buffer[i];
		bi = Sin * i_buffer[i];
		aq = Cos * q_buffer[i];
		bq = Sin * q_buffer[i];

		float32_t corr[2] = {ai + bq, -bi + aq};
		
		sam_data->a[0] = sam_data->dsI;
		sam_data->b[0] = bi;
		sam_data->c[0] = sam_data->dsQ;
		sam_data->d[0] = aq;
		sam_data->dsI = ai;
		sam_data->dsQ = bq;

		for (int j = 0; j < SAM_PLL_HILBERT_STAGES; j++)
		{
			int k = 3 * j;
			sam_data->a[k + 3] = demod_sam_const.c0[j] * (sam_data->a[k] - sam_data->a[k + 5]) + sam_data->a[k + 2];
			sam_data->b[k + 3] = demod_sam_const.c1[j] * (sam_data->b[k] - sam_data->b[k + 5]) + sam_data->b[k + 2];
			sam_data->c[k + 3] = demod_sam_const.c0[j] * (sam_data->c[k] - sam_data->c[k + 5]) + sam_data->c[k + 2];
			sam_data->d[k + 3] = demod_sam_const.c1[j] * (sam_data->d[k] - sam_data->d[k + 5]) + sam_data->d[k + 2];
		}

		float32_t ai_ps = sam_data->a[SAM_OUT_IDX];
		float32_t bi_ps = sam_data->b[SAM_OUT_IDX];
		float32_t bq_ps = sam_data->c[SAM_OUT_IDX];
		float32_t aq_ps = sam_data->d[SAM_OUT_IDX];

		// make room for next sample
		for (int j = SAM_OUT_IDX + 2; j > 0; j--)
		{
			sam_data->a[j] = sam_data->a[j - 1];
			sam_data->b[j] = sam_data->b[j - 1];
			sam_data->c[j] = sam_data->c[j - 1];
			sam_data->d[j] = sam_data->d[j - 1];
		}

		//out
		out_buffer_l[i] = (ai_ps + bi_ps) - (aq_ps - bq_ps);
		out_buffer_r[i] = (ai_ps - bi_ps) + (aq_ps + bq_ps);
		
		//fade leveler
		static float32_t dc27_l = 0;
		static float32_t dc27_r = 0;
    static float32_t dc_insert_l = {0};
    static float32_t dc_insert_r = {0};
		
    dc27_l = sam_data->adb_sam_mtauR * dc27_l + sam_data->adb_sam_onem_mtauR * out_buffer_l[i];
		dc27_r = sam_data->adb_sam_mtauR * dc27_r + sam_data->adb_sam_onem_mtauR * out_buffer_r[i];
    dc_insert_l = sam_data->adb_sam_mtauI * dc_insert_l + sam_data->adb_sam_onem_mtauI * corr[0];
		dc_insert_r = sam_data->adb_sam_mtauI * dc_insert_r + sam_data->adb_sam_onem_mtauI * corr[0];
    out_buffer_l[i] = out_buffer_l[i] + dc_insert_l - dc27_l;
		out_buffer_r[i] = out_buffer_r[i] + dc_insert_r - dc27_r;

		// determine phase error
		float32_t phzerror = atan2f(corr[1], corr[0]);

		float32_t del_out = sam_data->fil_out;
		// correct frequency 1st step
		sam_data->omega2 = sam_data->omega2 + sam_data->adb_sam_g2 * phzerror;
		if (sam_data->omega2 < sam_data->adb_sam_omega_min)
		{
			sam_data->omega2 = sam_data->adb_sam_omega_min;
		}
		else if (sam_data->omega2 > sam_data->adb_sam_omega_max)
		{
			sam_data->omega2 = sam_data->adb_sam_omega_max;
		}
		// correct frequency 2nd step
		sam_data->fil_out = sam_data->adb_sam_g1 * phzerror + sam_data->omega2;
		sam_data->phs = sam_data->phs + del_out;

		// wrap round 2PI, modulus
		while (sam_data->phs >= 2.0 * F_PI)
		{
			sam_data->phs -= (2.0 * F_PI);
		}
		while (sam_data->phs < 0.0)
		{
			sam_data->phs += (2.0 * F_PI);
		}
	}

	sam_data->count++;
	if (sam_data->count > 100) // to display the exact carrier frequency that the PLL is tuned to
	{
		float32_t carrier = 0.1 * (sam_data->omega2 * (float32_t)TRX_SAMPLERATE) / (2.0 * PI);
		carrier = carrier + 0.9 * sam_data->lowpass;
		println("SAM Carrier offset: ", carrier);
		sam_data->count = 0;
		sam_data->lowpass = carrier;
	}
}
