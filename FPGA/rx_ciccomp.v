// -------------------------------------------------------------
//
// Module: rx_ciccomp
// Generated by MATLAB(R) 9.4 and Filter Design HDL Coder 3.1.3.
// Generated on: 2018-11-30 22:14:43
// -------------------------------------------------------------

// -------------------------------------------------------------
// HDL Code Generation Options:
//
// OptimizeForHDL: on
// EDAScriptGeneration: off
// Name: rx_ciccomp
// SerialPartition: 24
// TargetLanguage: Verilog
// TestBenchName: rx_ciccomp_tb
// TestBenchStimulus: step ramp chirp noise 
// GenerateHDLTestBench: off

// Filter Specifications:
//
// Sample Rate            : N/A (normalized frequency)
// Response               : CIC Compensator
// Specification          : Fp,Fst,Ap,Ast
// Decimation Factor      : 2
// Multirate Type         : Decimator
// Passband Ripple        : 1 dB
// Number of Sections     : 6
// CIC Rate Change Factor : 521
// Passband Edge          : 0.45
// Differential Delay     : 1
// Stopband Atten.        : 60 dB
// Stopband Edge          : 0.55
// -------------------------------------------------------------

// -------------------------------------------------------------
// HDL Implementation    : Fully Serial
// Folding Factor        : 24
// -------------------------------------------------------------
// Filter Settings:
//
// Discrete-Time FIR Multirate Filter (real)
// -----------------------------------------
// Filter Structure   : Direct-Form FIR Polyphase Decimator
// Decimation Factor  : 2
// Polyphase Length   : 24
// Filter Length      : 48
// Stable             : Yes
// Linear Phase       : Yes (Type 2)
//
// Arithmetic         : fixed
// Numerator          : s16,15 -> [-1 1)
// -------------------------------------------------------------




`timescale 1 ns / 1 ns

module rx_ciccomp
               (
                clk,
                clk_enable,
                reset,
                filter_in,
                filter_out,
                ce_out
                );

  input   clk; 
  input   clk_enable; 
  input   reset; 
  input   signed [15:0] filter_in; //sfix16_En15
  output  signed [15:0] filter_out; //sfix16_En15
  output  ce_out; 

////////////////////////////////////////////////////////////////
//Module Architecture: rx_ciccomp
////////////////////////////////////////////////////////////////
  // Local Functions
  // Type Definitions
  // Constants
  parameter const_one = 1'b1; //boolean
  parameter signed [15:0] coeffphase1_1 = 16'b0000000010110001; //sfix16_En15
  parameter signed [15:0] coeffphase1_2 = 16'b0000000111001101; //sfix16_En15
  parameter signed [15:0] coeffphase1_3 = 16'b1111111001100010; //sfix16_En15
  parameter signed [15:0] coeffphase1_4 = 16'b0000001000000010; //sfix16_En15
  parameter signed [15:0] coeffphase1_5 = 16'b1111110101011111; //sfix16_En15
  parameter signed [15:0] coeffphase1_6 = 16'b0000001101100110; //sfix16_En15
  parameter signed [15:0] coeffphase1_7 = 16'b1111101110100010; //sfix16_En15
  parameter signed [15:0] coeffphase1_8 = 16'b0000010110010010; //sfix16_En15
  parameter signed [15:0] coeffphase1_9 = 16'b1111100011011000; //sfix16_En15
  parameter signed [15:0] coeffphase1_10 = 16'b0000100101100000; //sfix16_En15
  parameter signed [15:0] coeffphase1_11 = 16'b1111001100110010; //sfix16_En15
  parameter signed [15:0] coeffphase1_12 = 16'b0001000110011001; //sfix16_En15
  parameter signed [15:0] coeffphase1_13 = 16'b0100010001110100; //sfix16_En15
  parameter signed [15:0] coeffphase1_14 = 16'b1110101011000000; //sfix16_En15
  parameter signed [15:0] coeffphase1_15 = 16'b0000101001011100; //sfix16_En15
  parameter signed [15:0] coeffphase1_16 = 16'b1111101000011100; //sfix16_En15
  parameter signed [15:0] coeffphase1_17 = 16'b0000001110000100; //sfix16_En15
  parameter signed [15:0] coeffphase1_18 = 16'b1111110111101111; //sfix16_En15
  parameter signed [15:0] coeffphase1_19 = 16'b0000000100100010; //sfix16_En15
  parameter signed [15:0] coeffphase1_20 = 16'b1111111101111100; //sfix16_En15
  parameter signed [15:0] coeffphase1_21 = 16'b0000000000100010; //sfix16_En15
  parameter signed [15:0] coeffphase1_22 = 16'b0000000000011011; //sfix16_En15
  parameter signed [15:0] coeffphase1_23 = 16'b1111111111000010; //sfix16_En15
  parameter signed [15:0] coeffphase1_24 = 16'b0000000111100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_1 = 16'b0000000111100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_2 = 16'b1111111111000010; //sfix16_En15
  parameter signed [15:0] coeffphase2_3 = 16'b0000000000011011; //sfix16_En15
  parameter signed [15:0] coeffphase2_4 = 16'b0000000000100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_5 = 16'b1111111101111100; //sfix16_En15
  parameter signed [15:0] coeffphase2_6 = 16'b0000000100100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_7 = 16'b1111110111101111; //sfix16_En15
  parameter signed [15:0] coeffphase2_8 = 16'b0000001110000100; //sfix16_En15
  parameter signed [15:0] coeffphase2_9 = 16'b1111101000011100; //sfix16_En15
  parameter signed [15:0] coeffphase2_10 = 16'b0000101001011100; //sfix16_En15
  parameter signed [15:0] coeffphase2_11 = 16'b1110101011000000; //sfix16_En15
  parameter signed [15:0] coeffphase2_12 = 16'b0100010001110100; //sfix16_En15
  parameter signed [15:0] coeffphase2_13 = 16'b0001000110011001; //sfix16_En15
  parameter signed [15:0] coeffphase2_14 = 16'b1111001100110010; //sfix16_En15
  parameter signed [15:0] coeffphase2_15 = 16'b0000100101100000; //sfix16_En15
  parameter signed [15:0] coeffphase2_16 = 16'b1111100011011000; //sfix16_En15
  parameter signed [15:0] coeffphase2_17 = 16'b0000010110010010; //sfix16_En15
  parameter signed [15:0] coeffphase2_18 = 16'b1111101110100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_19 = 16'b0000001101100110; //sfix16_En15
  parameter signed [15:0] coeffphase2_20 = 16'b1111110101011111; //sfix16_En15
  parameter signed [15:0] coeffphase2_21 = 16'b0000001000000010; //sfix16_En15
  parameter signed [15:0] coeffphase2_22 = 16'b1111111001100010; //sfix16_En15
  parameter signed [15:0] coeffphase2_23 = 16'b0000000111001101; //sfix16_En15
  parameter signed [15:0] coeffphase2_24 = 16'b0000000010110001; //sfix16_En15

  // Signals
  reg  [5:0] cur_count; // ufix6
  wire phase_0; // boolean
  wire phase_24; // boolean
  wire phase_25; // boolean
  wire phase_temp; // boolean
  wire phase_reg_temp; // boolean
  wire phase_reg; // boolean
  reg  int_delay_pipe [0:47] ; // boolean
  reg  ce_out_reg; // boolean
  reg  signed [15:0] input_register; // sfix16_En15
  reg  signed [15:0] input_pipeline_phase0 [0:23] ; // sfix16_En15
  reg  signed [15:0] input_pipeline_phase1 [0:23] ; // sfix16_En15
  wire signed [15:0] inputmux; // sfix16_En15
  wire signed [31:0] product; // sfix32_En30
  wire signed [15:0] product_mux; // sfix16_En15
  wire signed [31:0] sumofproducts; // sfix32_En30
  wire signed [54:0] sumofproducts_cast; // sfix55_En30
  wire signed [54:0] acc_sum; // sfix55_En30
  wire signed [54:0] accreg_in; // sfix55_En30
  reg  signed [54:0] accreg_out; // sfix55_En30
  wire signed [54:0] add_signext; // sfix55_En30
  wire signed [54:0] add_signext_1; // sfix55_En30
  wire signed [55:0] add_temp; // sfix56_En30
  reg  signed [54:0] accreg_final; // sfix55_En30
  wire signed [15:0] output_typeconvert; // sfix16_En15
  reg  signed [15:0] output_register; // sfix16_En15

  // Block Statements
  always @ (posedge clk or posedge reset)
    begin: Counter
      if (reset == 1'b1) begin
        cur_count <= 6'b101111;
      end
      else begin
        if (clk_enable == 1'b1) begin
          if (cur_count >= 6'b101111) begin
            cur_count <= 6'b000000;
          end
          else begin
            cur_count <= cur_count + 6'b000001;
          end
        end
      end
    end // Counter

  assign  phase_0 = (cur_count == 6'b000000 && clk_enable == 1'b1) ? 1'b1 : 1'b0;

  assign  phase_24 = (cur_count == 6'b011000 && clk_enable == 1'b1) ? 1'b1 : 1'b0;

  assign  phase_25 = (cur_count == 6'b011001 && clk_enable == 1'b1) ? 1'b1 : 1'b0;

  assign phase_temp =  phase_0 & const_one;

  always @ (posedge clk or posedge reset)
    begin: ceout_delay_process
      if (reset == 1'b1) begin
        int_delay_pipe[0] <= 1'b0;
        int_delay_pipe[1] <= 1'b0;
        int_delay_pipe[2] <= 1'b0;
        int_delay_pipe[3] <= 1'b0;
        int_delay_pipe[4] <= 1'b0;
        int_delay_pipe[5] <= 1'b0;
        int_delay_pipe[6] <= 1'b0;
        int_delay_pipe[7] <= 1'b0;
        int_delay_pipe[8] <= 1'b0;
        int_delay_pipe[9] <= 1'b0;
        int_delay_pipe[10] <= 1'b0;
        int_delay_pipe[11] <= 1'b0;
        int_delay_pipe[12] <= 1'b0;
        int_delay_pipe[13] <= 1'b0;
        int_delay_pipe[14] <= 1'b0;
        int_delay_pipe[15] <= 1'b0;
        int_delay_pipe[16] <= 1'b0;
        int_delay_pipe[17] <= 1'b0;
        int_delay_pipe[18] <= 1'b0;
        int_delay_pipe[19] <= 1'b0;
        int_delay_pipe[20] <= 1'b0;
        int_delay_pipe[21] <= 1'b0;
        int_delay_pipe[22] <= 1'b0;
        int_delay_pipe[23] <= 1'b0;
        int_delay_pipe[24] <= 1'b0;
        int_delay_pipe[25] <= 1'b0;
        int_delay_pipe[26] <= 1'b0;
        int_delay_pipe[27] <= 1'b0;
        int_delay_pipe[28] <= 1'b0;
        int_delay_pipe[29] <= 1'b0;
        int_delay_pipe[30] <= 1'b0;
        int_delay_pipe[31] <= 1'b0;
        int_delay_pipe[32] <= 1'b0;
        int_delay_pipe[33] <= 1'b0;
        int_delay_pipe[34] <= 1'b0;
        int_delay_pipe[35] <= 1'b0;
        int_delay_pipe[36] <= 1'b0;
        int_delay_pipe[37] <= 1'b0;
        int_delay_pipe[38] <= 1'b0;
        int_delay_pipe[39] <= 1'b0;
        int_delay_pipe[40] <= 1'b0;
        int_delay_pipe[41] <= 1'b0;
        int_delay_pipe[42] <= 1'b0;
        int_delay_pipe[43] <= 1'b0;
        int_delay_pipe[44] <= 1'b0;
        int_delay_pipe[45] <= 1'b0;
        int_delay_pipe[46] <= 1'b0;
        int_delay_pipe[47] <= 1'b0;
      end
      else begin
        if (clk_enable == 1'b1) begin
          int_delay_pipe[1] <= int_delay_pipe[0];
          int_delay_pipe[2] <= int_delay_pipe[1];
          int_delay_pipe[3] <= int_delay_pipe[2];
          int_delay_pipe[4] <= int_delay_pipe[3];
          int_delay_pipe[5] <= int_delay_pipe[4];
          int_delay_pipe[6] <= int_delay_pipe[5];
          int_delay_pipe[7] <= int_delay_pipe[6];
          int_delay_pipe[8] <= int_delay_pipe[7];
          int_delay_pipe[9] <= int_delay_pipe[8];
          int_delay_pipe[10] <= int_delay_pipe[9];
          int_delay_pipe[11] <= int_delay_pipe[10];
          int_delay_pipe[12] <= int_delay_pipe[11];
          int_delay_pipe[13] <= int_delay_pipe[12];
          int_delay_pipe[14] <= int_delay_pipe[13];
          int_delay_pipe[15] <= int_delay_pipe[14];
          int_delay_pipe[16] <= int_delay_pipe[15];
          int_delay_pipe[17] <= int_delay_pipe[16];
          int_delay_pipe[18] <= int_delay_pipe[17];
          int_delay_pipe[19] <= int_delay_pipe[18];
          int_delay_pipe[20] <= int_delay_pipe[19];
          int_delay_pipe[21] <= int_delay_pipe[20];
          int_delay_pipe[22] <= int_delay_pipe[21];
          int_delay_pipe[23] <= int_delay_pipe[22];
          int_delay_pipe[24] <= int_delay_pipe[23];
          int_delay_pipe[25] <= int_delay_pipe[24];
          int_delay_pipe[26] <= int_delay_pipe[25];
          int_delay_pipe[27] <= int_delay_pipe[26];
          int_delay_pipe[28] <= int_delay_pipe[27];
          int_delay_pipe[29] <= int_delay_pipe[28];
          int_delay_pipe[30] <= int_delay_pipe[29];
          int_delay_pipe[31] <= int_delay_pipe[30];
          int_delay_pipe[32] <= int_delay_pipe[31];
          int_delay_pipe[33] <= int_delay_pipe[32];
          int_delay_pipe[34] <= int_delay_pipe[33];
          int_delay_pipe[35] <= int_delay_pipe[34];
          int_delay_pipe[36] <= int_delay_pipe[35];
          int_delay_pipe[37] <= int_delay_pipe[36];
          int_delay_pipe[38] <= int_delay_pipe[37];
          int_delay_pipe[39] <= int_delay_pipe[38];
          int_delay_pipe[40] <= int_delay_pipe[39];
          int_delay_pipe[41] <= int_delay_pipe[40];
          int_delay_pipe[42] <= int_delay_pipe[41];
          int_delay_pipe[43] <= int_delay_pipe[42];
          int_delay_pipe[44] <= int_delay_pipe[43];
          int_delay_pipe[45] <= int_delay_pipe[44];
          int_delay_pipe[46] <= int_delay_pipe[45];
          int_delay_pipe[47] <= int_delay_pipe[46];
          int_delay_pipe[0] <= phase_temp;
        end
      end
    end // ceout_delay_process
  assign phase_reg_temp = int_delay_pipe[47];

  assign phase_reg =  phase_reg_temp & phase_temp;

  always @ (posedge clk or posedge reset)
    begin: ce_out_register_process
      if (reset == 1'b1) begin
        ce_out_reg <= 1'b0;
      end
      else begin
        if (clk_enable == 1'b1) begin
          ce_out_reg <= phase_reg;
        end
      end
    end // ce_out_register_process

  always @ (posedge clk or posedge reset)
    begin: input_reg_process
      if (reset == 1'b1) begin
        input_register <= 0;
      end
      else begin
        if (clk_enable == 1'b1) begin
          input_register <= filter_in;
        end
      end
    end // input_reg_process

  always @( posedge clk or posedge reset)
    begin: Delay_Pipeline_Phase0_process
      if (reset == 1'b1) begin
        input_pipeline_phase0[0] <= 0;
        input_pipeline_phase0[1] <= 0;
        input_pipeline_phase0[2] <= 0;
        input_pipeline_phase0[3] <= 0;
        input_pipeline_phase0[4] <= 0;
        input_pipeline_phase0[5] <= 0;
        input_pipeline_phase0[6] <= 0;
        input_pipeline_phase0[7] <= 0;
        input_pipeline_phase0[8] <= 0;
        input_pipeline_phase0[9] <= 0;
        input_pipeline_phase0[10] <= 0;
        input_pipeline_phase0[11] <= 0;
        input_pipeline_phase0[12] <= 0;
        input_pipeline_phase0[13] <= 0;
        input_pipeline_phase0[14] <= 0;
        input_pipeline_phase0[15] <= 0;
        input_pipeline_phase0[16] <= 0;
        input_pipeline_phase0[17] <= 0;
        input_pipeline_phase0[18] <= 0;
        input_pipeline_phase0[19] <= 0;
        input_pipeline_phase0[20] <= 0;
        input_pipeline_phase0[21] <= 0;
        input_pipeline_phase0[22] <= 0;
        input_pipeline_phase0[23] <= 0;
      end
      else begin
        if (phase_0 == 1'b1) begin
          input_pipeline_phase0[0] <= input_register;
          input_pipeline_phase0[1] <= input_pipeline_phase0[0];
          input_pipeline_phase0[2] <= input_pipeline_phase0[1];
          input_pipeline_phase0[3] <= input_pipeline_phase0[2];
          input_pipeline_phase0[4] <= input_pipeline_phase0[3];
          input_pipeline_phase0[5] <= input_pipeline_phase0[4];
          input_pipeline_phase0[6] <= input_pipeline_phase0[5];
          input_pipeline_phase0[7] <= input_pipeline_phase0[6];
          input_pipeline_phase0[8] <= input_pipeline_phase0[7];
          input_pipeline_phase0[9] <= input_pipeline_phase0[8];
          input_pipeline_phase0[10] <= input_pipeline_phase0[9];
          input_pipeline_phase0[11] <= input_pipeline_phase0[10];
          input_pipeline_phase0[12] <= input_pipeline_phase0[11];
          input_pipeline_phase0[13] <= input_pipeline_phase0[12];
          input_pipeline_phase0[14] <= input_pipeline_phase0[13];
          input_pipeline_phase0[15] <= input_pipeline_phase0[14];
          input_pipeline_phase0[16] <= input_pipeline_phase0[15];
          input_pipeline_phase0[17] <= input_pipeline_phase0[16];
          input_pipeline_phase0[18] <= input_pipeline_phase0[17];
          input_pipeline_phase0[19] <= input_pipeline_phase0[18];
          input_pipeline_phase0[20] <= input_pipeline_phase0[19];
          input_pipeline_phase0[21] <= input_pipeline_phase0[20];
          input_pipeline_phase0[22] <= input_pipeline_phase0[21];
          input_pipeline_phase0[23] <= input_pipeline_phase0[22];
        end
      end
    end // Delay_Pipeline_Phase0_process


  always @( posedge clk or posedge reset)
    begin: Delay_Pipeline_Phase1_process
      if (reset == 1'b1) begin
        input_pipeline_phase1[0] <= 0;
        input_pipeline_phase1[1] <= 0;
        input_pipeline_phase1[2] <= 0;
        input_pipeline_phase1[3] <= 0;
        input_pipeline_phase1[4] <= 0;
        input_pipeline_phase1[5] <= 0;
        input_pipeline_phase1[6] <= 0;
        input_pipeline_phase1[7] <= 0;
        input_pipeline_phase1[8] <= 0;
        input_pipeline_phase1[9] <= 0;
        input_pipeline_phase1[10] <= 0;
        input_pipeline_phase1[11] <= 0;
        input_pipeline_phase1[12] <= 0;
        input_pipeline_phase1[13] <= 0;
        input_pipeline_phase1[14] <= 0;
        input_pipeline_phase1[15] <= 0;
        input_pipeline_phase1[16] <= 0;
        input_pipeline_phase1[17] <= 0;
        input_pipeline_phase1[18] <= 0;
        input_pipeline_phase1[19] <= 0;
        input_pipeline_phase1[20] <= 0;
        input_pipeline_phase1[21] <= 0;
        input_pipeline_phase1[22] <= 0;
        input_pipeline_phase1[23] <= 0;
      end
      else begin
        if (phase_24 == 1'b1) begin
          input_pipeline_phase1[0] <= input_register;
          input_pipeline_phase1[1] <= input_pipeline_phase1[0];
          input_pipeline_phase1[2] <= input_pipeline_phase1[1];
          input_pipeline_phase1[3] <= input_pipeline_phase1[2];
          input_pipeline_phase1[4] <= input_pipeline_phase1[3];
          input_pipeline_phase1[5] <= input_pipeline_phase1[4];
          input_pipeline_phase1[6] <= input_pipeline_phase1[5];
          input_pipeline_phase1[7] <= input_pipeline_phase1[6];
          input_pipeline_phase1[8] <= input_pipeline_phase1[7];
          input_pipeline_phase1[9] <= input_pipeline_phase1[8];
          input_pipeline_phase1[10] <= input_pipeline_phase1[9];
          input_pipeline_phase1[11] <= input_pipeline_phase1[10];
          input_pipeline_phase1[12] <= input_pipeline_phase1[11];
          input_pipeline_phase1[13] <= input_pipeline_phase1[12];
          input_pipeline_phase1[14] <= input_pipeline_phase1[13];
          input_pipeline_phase1[15] <= input_pipeline_phase1[14];
          input_pipeline_phase1[16] <= input_pipeline_phase1[15];
          input_pipeline_phase1[17] <= input_pipeline_phase1[16];
          input_pipeline_phase1[18] <= input_pipeline_phase1[17];
          input_pipeline_phase1[19] <= input_pipeline_phase1[18];
          input_pipeline_phase1[20] <= input_pipeline_phase1[19];
          input_pipeline_phase1[21] <= input_pipeline_phase1[20];
          input_pipeline_phase1[22] <= input_pipeline_phase1[21];
          input_pipeline_phase1[23] <= input_pipeline_phase1[22];
        end
      end
    end // Delay_Pipeline_Phase1_process


  // Mux(es) to select the input taps for multipliers 

  assign inputmux = (cur_count == 6'b000001) ? input_pipeline_phase0[0] :
                   (cur_count == 6'b000010) ? input_pipeline_phase0[1] :
                   (cur_count == 6'b000011) ? input_pipeline_phase0[2] :
                   (cur_count == 6'b000100) ? input_pipeline_phase0[3] :
                   (cur_count == 6'b000101) ? input_pipeline_phase0[4] :
                   (cur_count == 6'b000110) ? input_pipeline_phase0[5] :
                   (cur_count == 6'b000111) ? input_pipeline_phase0[6] :
                   (cur_count == 6'b001000) ? input_pipeline_phase0[7] :
                   (cur_count == 6'b001001) ? input_pipeline_phase0[8] :
                   (cur_count == 6'b001010) ? input_pipeline_phase0[9] :
                   (cur_count == 6'b001011) ? input_pipeline_phase0[10] :
                   (cur_count == 6'b001100) ? input_pipeline_phase0[11] :
                   (cur_count == 6'b001101) ? input_pipeline_phase0[12] :
                   (cur_count == 6'b001110) ? input_pipeline_phase0[13] :
                   (cur_count == 6'b001111) ? input_pipeline_phase0[14] :
                   (cur_count == 6'b010000) ? input_pipeline_phase0[15] :
                   (cur_count == 6'b010001) ? input_pipeline_phase0[16] :
                   (cur_count == 6'b010010) ? input_pipeline_phase0[17] :
                   (cur_count == 6'b010011) ? input_pipeline_phase0[18] :
                   (cur_count == 6'b010100) ? input_pipeline_phase0[19] :
                   (cur_count == 6'b010101) ? input_pipeline_phase0[20] :
                   (cur_count == 6'b010110) ? input_pipeline_phase0[21] :
                   (cur_count == 6'b010111) ? input_pipeline_phase0[22] :
                   (cur_count == 6'b011000) ? input_pipeline_phase0[23] :
                   (cur_count == 6'b011001) ? input_pipeline_phase1[0] :
                   (cur_count == 6'b011010) ? input_pipeline_phase1[1] :
                   (cur_count == 6'b011011) ? input_pipeline_phase1[2] :
                   (cur_count == 6'b011100) ? input_pipeline_phase1[3] :
                   (cur_count == 6'b011101) ? input_pipeline_phase1[4] :
                   (cur_count == 6'b011110) ? input_pipeline_phase1[5] :
                   (cur_count == 6'b011111) ? input_pipeline_phase1[6] :
                   (cur_count == 6'b100000) ? input_pipeline_phase1[7] :
                   (cur_count == 6'b100001) ? input_pipeline_phase1[8] :
                   (cur_count == 6'b100010) ? input_pipeline_phase1[9] :
                   (cur_count == 6'b100011) ? input_pipeline_phase1[10] :
                   (cur_count == 6'b100100) ? input_pipeline_phase1[11] :
                   (cur_count == 6'b100101) ? input_pipeline_phase1[12] :
                   (cur_count == 6'b100110) ? input_pipeline_phase1[13] :
                   (cur_count == 6'b100111) ? input_pipeline_phase1[14] :
                   (cur_count == 6'b101000) ? input_pipeline_phase1[15] :
                   (cur_count == 6'b101001) ? input_pipeline_phase1[16] :
                   (cur_count == 6'b101010) ? input_pipeline_phase1[17] :
                   (cur_count == 6'b101011) ? input_pipeline_phase1[18] :
                   (cur_count == 6'b101100) ? input_pipeline_phase1[19] :
                   (cur_count == 6'b101101) ? input_pipeline_phase1[20] :
                   (cur_count == 6'b101110) ? input_pipeline_phase1[21] :
                   (cur_count == 6'b101111) ? input_pipeline_phase1[22] :
                   input_pipeline_phase1[23];

  assign product_mux = (cur_count == 6'b000001) ? coeffphase1_1 :
                      (cur_count == 6'b000010) ? coeffphase1_2 :
                      (cur_count == 6'b000011) ? coeffphase1_3 :
                      (cur_count == 6'b000100) ? coeffphase1_4 :
                      (cur_count == 6'b000101) ? coeffphase1_5 :
                      (cur_count == 6'b000110) ? coeffphase1_6 :
                      (cur_count == 6'b000111) ? coeffphase1_7 :
                      (cur_count == 6'b001000) ? coeffphase1_8 :
                      (cur_count == 6'b001001) ? coeffphase1_9 :
                      (cur_count == 6'b001010) ? coeffphase1_10 :
                      (cur_count == 6'b001011) ? coeffphase1_11 :
                      (cur_count == 6'b001100) ? coeffphase1_12 :
                      (cur_count == 6'b001101) ? coeffphase1_13 :
                      (cur_count == 6'b001110) ? coeffphase1_14 :
                      (cur_count == 6'b001111) ? coeffphase1_15 :
                      (cur_count == 6'b010000) ? coeffphase1_16 :
                      (cur_count == 6'b010001) ? coeffphase1_17 :
                      (cur_count == 6'b010010) ? coeffphase1_18 :
                      (cur_count == 6'b010011) ? coeffphase1_19 :
                      (cur_count == 6'b010100) ? coeffphase1_20 :
                      (cur_count == 6'b010101) ? coeffphase1_21 :
                      (cur_count == 6'b010110) ? coeffphase1_22 :
                      (cur_count == 6'b010111) ? coeffphase1_23 :
                      (cur_count == 6'b011000) ? coeffphase1_24 :
                      (cur_count == 6'b011001) ? coeffphase2_1 :
                      (cur_count == 6'b011010) ? coeffphase2_2 :
                      (cur_count == 6'b011011) ? coeffphase2_3 :
                      (cur_count == 6'b011100) ? coeffphase2_4 :
                      (cur_count == 6'b011101) ? coeffphase2_5 :
                      (cur_count == 6'b011110) ? coeffphase2_6 :
                      (cur_count == 6'b011111) ? coeffphase2_7 :
                      (cur_count == 6'b100000) ? coeffphase2_8 :
                      (cur_count == 6'b100001) ? coeffphase2_9 :
                      (cur_count == 6'b100010) ? coeffphase2_10 :
                      (cur_count == 6'b100011) ? coeffphase2_11 :
                      (cur_count == 6'b100100) ? coeffphase2_12 :
                      (cur_count == 6'b100101) ? coeffphase2_13 :
                      (cur_count == 6'b100110) ? coeffphase2_14 :
                      (cur_count == 6'b100111) ? coeffphase2_15 :
                      (cur_count == 6'b101000) ? coeffphase2_16 :
                      (cur_count == 6'b101001) ? coeffphase2_17 :
                      (cur_count == 6'b101010) ? coeffphase2_18 :
                      (cur_count == 6'b101011) ? coeffphase2_19 :
                      (cur_count == 6'b101100) ? coeffphase2_20 :
                      (cur_count == 6'b101101) ? coeffphase2_21 :
                      (cur_count == 6'b101110) ? coeffphase2_22 :
                      (cur_count == 6'b101111) ? coeffphase2_23 :
                      coeffphase2_24;
  assign product = inputmux * product_mux;



  // Add the products in linear fashion

  assign sumofproducts = product;

  // Resize the sum of products to the accumulator type for full precision addition

  assign sumofproducts_cast = $signed({{23{sumofproducts[31]}}, sumofproducts});

  // Accumulator register with a mux to reset it with the first addend

  assign add_signext = sumofproducts_cast;
  assign add_signext_1 = accreg_out;
  assign add_temp = add_signext + add_signext_1;
  assign acc_sum = add_temp[54:0];

  assign accreg_in = (phase_25 == 1'b1) ? sumofproducts_cast :
                    acc_sum;

  always @ (posedge clk or posedge reset)
    begin: Acc_reg_process
      if (reset == 1'b1) begin
        accreg_out <= 0;
      end
      else begin
        if (clk_enable == 1'b1) begin
          accreg_out <= accreg_in;
        end
      end
    end // Acc_reg_process

  // Register to hold the final value of the accumulated sum

  always @ (posedge clk or posedge reset)
    begin: Acc_finalreg_process
      if (reset == 1'b1) begin
        accreg_final <= 0;
      end
      else begin
        if (phase_25 == 1'b1) begin
          accreg_final <= accreg_out;
        end
      end
    end // Acc_finalreg_process

  assign output_typeconvert = (accreg_final[30:0] + {accreg_final[15], {14{~accreg_final[15]}}})>>>15;

  always @ (posedge clk or posedge reset)
    begin: output_register_process
      if (reset == 1'b1) begin
        output_register <= 0;
      end
      else begin
        if (phase_reg == 1'b1) begin
          output_register <= output_typeconvert;
        end
      end
    end // output_register_process

  // Assignment Statements
  assign ce_out = ce_out_reg;
  assign filter_out = output_register;
endmodule  // rx_ciccomp
