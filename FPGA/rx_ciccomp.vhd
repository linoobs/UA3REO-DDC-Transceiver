-- -------------------------------------------------------------
--
-- Module: rx_ciccomp
-- Generated by MATLAB(R) 9.9 and Filter Design HDL Coder 3.1.8.
-- Generated on: 2021-02-09 09:39:32
-- -------------------------------------------------------------

-- -------------------------------------------------------------
-- HDL Code Generation Options:
--
-- TargetLanguage: VHDL
-- OptimizeForHDL: on
-- Name: rx_ciccomp
-- SerialPartition: 28
-- TestBenchName: rx_ciccomp_tb
-- TestBenchStimulus: step ramp chirp noise 

-- Filter Specifications:
--
-- Sample Rate            : N/A (normalized frequency)
-- Response               : CIC Compensator
-- Specification          : Fp,Fst,Ap,Ast
-- Decimation Factor      : 2
-- Multirate Type         : Decimator
-- CIC Rate Change Factor : 640
-- Number of Sections     : 6
-- Differential Delay     : 1
-- Stopband Atten.        : 60 dB
-- Passband Ripple        : 0.3 dB
-- Stopband Edge          : 0.55
-- Passband Edge          : 0.45
-- -------------------------------------------------------------

-- -------------------------------------------------------------
-- HDL Implementation    : Fully Serial
-- Folding Factor        : 28
-- -------------------------------------------------------------
-- Filter Settings:
--
-- Discrete-Time FIR Multirate Filter (real)
-- -----------------------------------------
-- Filter Structure   : Direct-Form FIR Polyphase Decimator
-- Decimation Factor  : 2
-- Polyphase Length   : 29
-- Filter Length      : 58
-- Stable             : Yes
-- Linear Phase       : Yes (Type 2)
--
-- Arithmetic         : fixed
-- Numerator          : s16,15 -> [-1 1)
-- -------------------------------------------------------------



LIBRARY IEEE;
USE IEEE.std_logic_1164.all;
USE IEEE.numeric_std.ALL;

ENTITY rx_ciccomp IS
   PORT( clk                             :   IN    std_logic; 
         clk_enable                      :   IN    std_logic; 
         reset                           :   IN    std_logic; 
         filter_in                       :   IN    std_logic_vector(31 DOWNTO 0); -- sfix32
         filter_out                      :   OUT   std_logic_vector(31 DOWNTO 0); -- sfix32
         ce_out                          :   OUT   std_logic  
         );

END rx_ciccomp;


----------------------------------------------------------------
--Module Architecture: rx_ciccomp
----------------------------------------------------------------
ARCHITECTURE rtl OF rx_ciccomp IS
  -- Local Functions
  -- Type Definitions
  TYPE input_pipeline_type IS ARRAY (NATURAL range <>) OF signed(31 DOWNTO 0); -- sfix32
  -- Constants
  CONSTANT const_one                      : std_logic := '1'; -- boolean
  CONSTANT coeffphase1_1                  : signed(15 DOWNTO 0) := to_signed(-67, 16); -- sfix16_En15
  CONSTANT coeffphase1_2                  : signed(15 DOWNTO 0) := to_signed(95, 16); -- sfix16_En15
  CONSTANT coeffphase1_3                  : signed(15 DOWNTO 0) := to_signed(51, 16); -- sfix16_En15
  CONSTANT coeffphase1_4                  : signed(15 DOWNTO 0) := to_signed(-6, 16); -- sfix16_En15
  CONSTANT coeffphase1_5                  : signed(15 DOWNTO 0) := to_signed(77, 16); -- sfix16_En15
  CONSTANT coeffphase1_6                  : signed(15 DOWNTO 0) := to_signed(-125, 16); -- sfix16_En15
  CONSTANT coeffphase1_7                  : signed(15 DOWNTO 0) := to_signed(222, 16); -- sfix16_En15
  CONSTANT coeffphase1_8                  : signed(15 DOWNTO 0) := to_signed(-345, 16); -- sfix16_En15
  CONSTANT coeffphase1_9                  : signed(15 DOWNTO 0) := to_signed(523, 16); -- sfix16_En15
  CONSTANT coeffphase1_10                 : signed(15 DOWNTO 0) := to_signed(-776, 16); -- sfix16_En15
  CONSTANT coeffphase1_11                 : signed(15 DOWNTO 0) := to_signed(1154, 16); -- sfix16_En15
  CONSTANT coeffphase1_12                 : signed(15 DOWNTO 0) := to_signed(-1765, 16); -- sfix16_En15
  CONSTANT coeffphase1_13                 : signed(15 DOWNTO 0) := to_signed(2903, 16); -- sfix16_En15
  CONSTANT coeffphase1_14                 : signed(15 DOWNTO 0) := to_signed(-5682, 16); -- sfix16_En15
  CONSTANT coeffphase1_15                 : signed(15 DOWNTO 0) := to_signed(17749, 16); -- sfix16_En15
  CONSTANT coeffphase1_16                 : signed(15 DOWNTO 0) := to_signed(4300, 16); -- sfix16_En15
  CONSTANT coeffphase1_17                 : signed(15 DOWNTO 0) := to_signed(-3099, 16); -- sfix16_En15
  CONSTANT coeffphase1_18                 : signed(15 DOWNTO 0) := to_signed(2249, 16); -- sfix16_En15
  CONSTANT coeffphase1_19                 : signed(15 DOWNTO 0) := to_signed(-1709, 16); -- sfix16_En15
  CONSTANT coeffphase1_20                 : signed(15 DOWNTO 0) := to_signed(1335, 16); -- sfix16_En15
  CONSTANT coeffphase1_21                 : signed(15 DOWNTO 0) := to_signed(-1054, 16); -- sfix16_En15
  CONSTANT coeffphase1_22                 : signed(15 DOWNTO 0) := to_signed(834, 16); -- sfix16_En15
  CONSTANT coeffphase1_23                 : signed(15 DOWNTO 0) := to_signed(-654, 16); -- sfix16_En15
  CONSTANT coeffphase1_24                 : signed(15 DOWNTO 0) := to_signed(512, 16); -- sfix16_En15
  CONSTANT coeffphase1_25                 : signed(15 DOWNTO 0) := to_signed(-382, 16); -- sfix16_En15
  CONSTANT coeffphase1_26                 : signed(15 DOWNTO 0) := to_signed(303, 16); -- sfix16_En15
  CONSTANT coeffphase1_27                 : signed(15 DOWNTO 0) := to_signed(-179, 16); -- sfix16_En15
  CONSTANT coeffphase1_28                 : signed(15 DOWNTO 0) := to_signed(224, 16); -- sfix16_En15
  CONSTANT coeffphase1_29                 : signed(15 DOWNTO 0) := to_signed(-68, 16); -- sfix16_En15
  CONSTANT coeffphase2_1                  : signed(15 DOWNTO 0) := to_signed(-68, 16); -- sfix16_En15
  CONSTANT coeffphase2_2                  : signed(15 DOWNTO 0) := to_signed(224, 16); -- sfix16_En15
  CONSTANT coeffphase2_3                  : signed(15 DOWNTO 0) := to_signed(-179, 16); -- sfix16_En15
  CONSTANT coeffphase2_4                  : signed(15 DOWNTO 0) := to_signed(303, 16); -- sfix16_En15
  CONSTANT coeffphase2_5                  : signed(15 DOWNTO 0) := to_signed(-382, 16); -- sfix16_En15
  CONSTANT coeffphase2_6                  : signed(15 DOWNTO 0) := to_signed(512, 16); -- sfix16_En15
  CONSTANT coeffphase2_7                  : signed(15 DOWNTO 0) := to_signed(-654, 16); -- sfix16_En15
  CONSTANT coeffphase2_8                  : signed(15 DOWNTO 0) := to_signed(834, 16); -- sfix16_En15
  CONSTANT coeffphase2_9                  : signed(15 DOWNTO 0) := to_signed(-1054, 16); -- sfix16_En15
  CONSTANT coeffphase2_10                 : signed(15 DOWNTO 0) := to_signed(1335, 16); -- sfix16_En15
  CONSTANT coeffphase2_11                 : signed(15 DOWNTO 0) := to_signed(-1709, 16); -- sfix16_En15
  CONSTANT coeffphase2_12                 : signed(15 DOWNTO 0) := to_signed(2249, 16); -- sfix16_En15
  CONSTANT coeffphase2_13                 : signed(15 DOWNTO 0) := to_signed(-3099, 16); -- sfix16_En15
  CONSTANT coeffphase2_14                 : signed(15 DOWNTO 0) := to_signed(4300, 16); -- sfix16_En15
  CONSTANT coeffphase2_15                 : signed(15 DOWNTO 0) := to_signed(17749, 16); -- sfix16_En15
  CONSTANT coeffphase2_16                 : signed(15 DOWNTO 0) := to_signed(-5682, 16); -- sfix16_En15
  CONSTANT coeffphase2_17                 : signed(15 DOWNTO 0) := to_signed(2903, 16); -- sfix16_En15
  CONSTANT coeffphase2_18                 : signed(15 DOWNTO 0) := to_signed(-1765, 16); -- sfix16_En15
  CONSTANT coeffphase2_19                 : signed(15 DOWNTO 0) := to_signed(1154, 16); -- sfix16_En15
  CONSTANT coeffphase2_20                 : signed(15 DOWNTO 0) := to_signed(-776, 16); -- sfix16_En15
  CONSTANT coeffphase2_21                 : signed(15 DOWNTO 0) := to_signed(523, 16); -- sfix16_En15
  CONSTANT coeffphase2_22                 : signed(15 DOWNTO 0) := to_signed(-345, 16); -- sfix16_En15
  CONSTANT coeffphase2_23                 : signed(15 DOWNTO 0) := to_signed(222, 16); -- sfix16_En15
  CONSTANT coeffphase2_24                 : signed(15 DOWNTO 0) := to_signed(-125, 16); -- sfix16_En15
  CONSTANT coeffphase2_25                 : signed(15 DOWNTO 0) := to_signed(77, 16); -- sfix16_En15
  CONSTANT coeffphase2_26                 : signed(15 DOWNTO 0) := to_signed(-6, 16); -- sfix16_En15
  CONSTANT coeffphase2_27                 : signed(15 DOWNTO 0) := to_signed(51, 16); -- sfix16_En15
  CONSTANT coeffphase2_28                 : signed(15 DOWNTO 0) := to_signed(95, 16); -- sfix16_En15
  CONSTANT coeffphase2_29                 : signed(15 DOWNTO 0) := to_signed(-67, 16); -- sfix16_En15

  CONSTANT const_zero                     : signed(47 DOWNTO 0) := to_signed(0, 48); -- sfix48_En15
  -- Signals
  SIGNAL cur_count                        : unsigned(5 DOWNTO 0); -- ufix6
  SIGNAL phase_0                          : std_logic; -- boolean
  SIGNAL phase_1                          : std_logic; -- boolean
  SIGNAL phase_28                         : std_logic; -- boolean
  SIGNAL phase_29                         : std_logic; -- boolean
  SIGNAL phase_temp                       : std_logic; -- boolean
  SIGNAL phase_reg_temp                   : std_logic; -- boolean
  SIGNAL phase_reg                        : std_logic; -- boolean
  SIGNAL int_delay_pipe                   : std_logic_vector(0 TO 55); -- boolean
  SIGNAL ce_out_reg                       : std_logic; -- boolean
  SIGNAL input_register                   : signed(31 DOWNTO 0); -- sfix32
  SIGNAL input_pipeline_phase0            : input_pipeline_type(0 TO 28); -- sfix32
  SIGNAL input_pipeline_phase1            : input_pipeline_type(0 TO 28); -- sfix32
  SIGNAL inputmux                         : signed(31 DOWNTO 0); -- sfix32
  SIGNAL product                          : signed(47 DOWNTO 0); -- sfix48_En15
  SIGNAL product_mux                      : signed(15 DOWNTO 0); -- sfix16_En15
  SIGNAL prod_powertwo_1_24               : signed(47 DOWNTO 0); -- sfix48_En15
  SIGNAL prod_powertwo_2_6                : signed(47 DOWNTO 0); -- sfix48_En15
  SIGNAL powertwo_mux_1_24                : signed(47 DOWNTO 0); -- sfix48_En15
  SIGNAL powertwo_mux_2_6                 : signed(47 DOWNTO 0); -- sfix48_En15
  SIGNAL sumofproducts                    : signed(49 DOWNTO 0); -- sfix50_En15
  SIGNAL sum_1                            : signed(49 DOWNTO 0); -- sfix50_En15
  SIGNAL add_temp                         : signed(48 DOWNTO 0); -- sfix49_En15
  SIGNAL add_temp_1                       : signed(50 DOWNTO 0); -- sfix51_En15
  SIGNAL sumofproducts_cast               : signed(76 DOWNTO 0); -- sfix77_En15
  SIGNAL acc_sum                          : signed(76 DOWNTO 0); -- sfix77_En15
  SIGNAL accreg_in                        : signed(76 DOWNTO 0); -- sfix77_En15
  SIGNAL accreg_out                       : signed(76 DOWNTO 0); -- sfix77_En15
  SIGNAL add_temp_2                       : signed(77 DOWNTO 0); -- sfix78_En15
  SIGNAL accreg_final                     : signed(76 DOWNTO 0); -- sfix77_En15
  SIGNAL output_typeconvert               : signed(31 DOWNTO 0); -- sfix32
  SIGNAL output_register                  : signed(31 DOWNTO 0); -- sfix32


BEGIN

  -- Block Statements
  Counter : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      cur_count <= to_unsigned(55, 6);
    ELSIF clk'event AND clk = '1' THEN
      IF clk_enable = '1' THEN
        IF cur_count >= to_unsigned(55, 6) THEN
          cur_count <= to_unsigned(0, 6);
        ELSE
          cur_count <= cur_count + to_unsigned(1, 6);
        END IF;
      END IF;
    END IF; 
  END PROCESS Counter;

  phase_0 <= '1' WHEN cur_count = to_unsigned(0, 6) AND clk_enable = '1' ELSE '0';

  phase_1 <= '1' WHEN cur_count = to_unsigned(1, 6) AND clk_enable = '1' ELSE '0';

  phase_28 <= '1' WHEN cur_count = to_unsigned(28, 6) AND clk_enable = '1' ELSE '0';

  phase_29 <= '1' WHEN cur_count = to_unsigned(29, 6) AND clk_enable = '1' ELSE '0';

  phase_temp <=  phase_0 AND const_one;

  ceout_delay_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      int_delay_pipe <= (OTHERS => '0');
    ELSIF clk'event AND clk = '1' THEN
      IF clk_enable = '1' THEN
        int_delay_pipe(1 TO 55) <= int_delay_pipe(0 TO 54);
        int_delay_pipe(0) <= phase_temp;
      END IF;
    END IF;
  END PROCESS ceout_delay_process;
  phase_reg_temp <= int_delay_pipe(55);

  phase_reg <=  phase_reg_temp AND phase_temp;

  ce_out_register_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      ce_out_reg <= '0';
    ELSIF clk'event AND clk = '1' THEN
      IF clk_enable = '1' THEN
        ce_out_reg <= phase_reg;
      END IF;
    END IF; 
  END PROCESS ce_out_register_process;

  input_reg_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      input_register <= (OTHERS => '0');
    ELSIF clk'event AND clk = '1' THEN
      IF clk_enable = '1' THEN
        input_register <= signed(filter_in);
      END IF;
    END IF; 
  END PROCESS input_reg_process;

  Delay_Pipeline_Phase0_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      input_pipeline_phase0(0 TO 28) <= (OTHERS => (OTHERS => '0'));
    ELSIF clk'event AND clk = '1' THEN
      IF phase_0 = '1' THEN
        input_pipeline_phase0(0) <= input_register;
        input_pipeline_phase0(1 TO 28) <= input_pipeline_phase0(0 TO 27);
      END IF;
    END IF; 
  END PROCESS Delay_Pipeline_Phase0_process;

  Delay_Pipeline_Phase1_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      input_pipeline_phase1(0 TO 28) <= (OTHERS => (OTHERS => '0'));
    ELSIF clk'event AND clk = '1' THEN
      IF phase_28 = '1' THEN
        input_pipeline_phase1(0) <= input_register;
        input_pipeline_phase1(1 TO 28) <= input_pipeline_phase1(0 TO 27);
      END IF;
    END IF; 
  END PROCESS Delay_Pipeline_Phase1_process;

  -- Mux(es) to select the input taps for multipliers 

  inputmux <= input_pipeline_phase0(0) WHEN ( cur_count = to_unsigned(1, 6) ) ELSE
                   input_pipeline_phase0(1) WHEN ( cur_count = to_unsigned(2, 6) ) ELSE
                   input_pipeline_phase0(2) WHEN ( cur_count = to_unsigned(3, 6) ) ELSE
                   input_pipeline_phase0(3) WHEN ( cur_count = to_unsigned(4, 6) ) ELSE
                   input_pipeline_phase0(4) WHEN ( cur_count = to_unsigned(5, 6) ) ELSE
                   input_pipeline_phase0(5) WHEN ( cur_count = to_unsigned(6, 6) ) ELSE
                   input_pipeline_phase0(6) WHEN ( cur_count = to_unsigned(7, 6) ) ELSE
                   input_pipeline_phase0(7) WHEN ( cur_count = to_unsigned(8, 6) ) ELSE
                   input_pipeline_phase0(8) WHEN ( cur_count = to_unsigned(9, 6) ) ELSE
                   input_pipeline_phase0(9) WHEN ( cur_count = to_unsigned(10, 6) ) ELSE
                   input_pipeline_phase0(10) WHEN ( cur_count = to_unsigned(11, 6) ) ELSE
                   input_pipeline_phase0(11) WHEN ( cur_count = to_unsigned(12, 6) ) ELSE
                   input_pipeline_phase0(12) WHEN ( cur_count = to_unsigned(13, 6) ) ELSE
                   input_pipeline_phase0(13) WHEN ( cur_count = to_unsigned(14, 6) ) ELSE
                   input_pipeline_phase0(14) WHEN ( cur_count = to_unsigned(15, 6) ) ELSE
                   input_pipeline_phase0(15) WHEN ( cur_count = to_unsigned(16, 6) ) ELSE
                   input_pipeline_phase0(16) WHEN ( cur_count = to_unsigned(17, 6) ) ELSE
                   input_pipeline_phase0(17) WHEN ( cur_count = to_unsigned(18, 6) ) ELSE
                   input_pipeline_phase0(18) WHEN ( cur_count = to_unsigned(19, 6) ) ELSE
                   input_pipeline_phase0(19) WHEN ( cur_count = to_unsigned(20, 6) ) ELSE
                   input_pipeline_phase0(20) WHEN ( cur_count = to_unsigned(21, 6) ) ELSE
                   input_pipeline_phase0(21) WHEN ( cur_count = to_unsigned(22, 6) ) ELSE
                   input_pipeline_phase0(22) WHEN ( cur_count = to_unsigned(23, 6) ) ELSE
                   input_pipeline_phase0(24) WHEN ( cur_count = to_unsigned(24, 6) ) ELSE
                   input_pipeline_phase0(25) WHEN ( cur_count = to_unsigned(25, 6) ) ELSE
                   input_pipeline_phase0(26) WHEN ( cur_count = to_unsigned(26, 6) ) ELSE
                   input_pipeline_phase0(27) WHEN ( cur_count = to_unsigned(27, 6) ) ELSE
                   input_pipeline_phase0(28) WHEN ( cur_count = to_unsigned(28, 6) ) ELSE
                   input_pipeline_phase1(0) WHEN ( cur_count = to_unsigned(29, 6) ) ELSE
                   input_pipeline_phase1(1) WHEN ( cur_count = to_unsigned(30, 6) ) ELSE
                   input_pipeline_phase1(2) WHEN ( cur_count = to_unsigned(31, 6) ) ELSE
                   input_pipeline_phase1(3) WHEN ( cur_count = to_unsigned(32, 6) ) ELSE
                   input_pipeline_phase1(4) WHEN ( cur_count = to_unsigned(33, 6) ) ELSE
                   input_pipeline_phase1(6) WHEN ( cur_count = to_unsigned(34, 6) ) ELSE
                   input_pipeline_phase1(7) WHEN ( cur_count = to_unsigned(35, 6) ) ELSE
                   input_pipeline_phase1(8) WHEN ( cur_count = to_unsigned(36, 6) ) ELSE
                   input_pipeline_phase1(9) WHEN ( cur_count = to_unsigned(37, 6) ) ELSE
                   input_pipeline_phase1(10) WHEN ( cur_count = to_unsigned(38, 6) ) ELSE
                   input_pipeline_phase1(11) WHEN ( cur_count = to_unsigned(39, 6) ) ELSE
                   input_pipeline_phase1(12) WHEN ( cur_count = to_unsigned(40, 6) ) ELSE
                   input_pipeline_phase1(13) WHEN ( cur_count = to_unsigned(41, 6) ) ELSE
                   input_pipeline_phase1(14) WHEN ( cur_count = to_unsigned(42, 6) ) ELSE
                   input_pipeline_phase1(15) WHEN ( cur_count = to_unsigned(43, 6) ) ELSE
                   input_pipeline_phase1(16) WHEN ( cur_count = to_unsigned(44, 6) ) ELSE
                   input_pipeline_phase1(17) WHEN ( cur_count = to_unsigned(45, 6) ) ELSE
                   input_pipeline_phase1(18) WHEN ( cur_count = to_unsigned(46, 6) ) ELSE
                   input_pipeline_phase1(19) WHEN ( cur_count = to_unsigned(47, 6) ) ELSE
                   input_pipeline_phase1(20) WHEN ( cur_count = to_unsigned(48, 6) ) ELSE
                   input_pipeline_phase1(21) WHEN ( cur_count = to_unsigned(49, 6) ) ELSE
                   input_pipeline_phase1(22) WHEN ( cur_count = to_unsigned(50, 6) ) ELSE
                   input_pipeline_phase1(23) WHEN ( cur_count = to_unsigned(51, 6) ) ELSE
                   input_pipeline_phase1(24) WHEN ( cur_count = to_unsigned(52, 6) ) ELSE
                   input_pipeline_phase1(25) WHEN ( cur_count = to_unsigned(53, 6) ) ELSE
                   input_pipeline_phase1(26) WHEN ( cur_count = to_unsigned(54, 6) ) ELSE
                   input_pipeline_phase1(27) WHEN ( cur_count = to_unsigned(55, 6) ) ELSE
                   input_pipeline_phase1(28);

  product_mux <= coeffphase1_1 WHEN ( cur_count = to_unsigned(1, 6) ) ELSE
                      coeffphase1_2 WHEN ( cur_count = to_unsigned(2, 6) ) ELSE
                      coeffphase1_3 WHEN ( cur_count = to_unsigned(3, 6) ) ELSE
                      coeffphase1_4 WHEN ( cur_count = to_unsigned(4, 6) ) ELSE
                      coeffphase1_5 WHEN ( cur_count = to_unsigned(5, 6) ) ELSE
                      coeffphase1_6 WHEN ( cur_count = to_unsigned(6, 6) ) ELSE
                      coeffphase1_7 WHEN ( cur_count = to_unsigned(7, 6) ) ELSE
                      coeffphase1_8 WHEN ( cur_count = to_unsigned(8, 6) ) ELSE
                      coeffphase1_9 WHEN ( cur_count = to_unsigned(9, 6) ) ELSE
                      coeffphase1_10 WHEN ( cur_count = to_unsigned(10, 6) ) ELSE
                      coeffphase1_11 WHEN ( cur_count = to_unsigned(11, 6) ) ELSE
                      coeffphase1_12 WHEN ( cur_count = to_unsigned(12, 6) ) ELSE
                      coeffphase1_13 WHEN ( cur_count = to_unsigned(13, 6) ) ELSE
                      coeffphase1_14 WHEN ( cur_count = to_unsigned(14, 6) ) ELSE
                      coeffphase1_15 WHEN ( cur_count = to_unsigned(15, 6) ) ELSE
                      coeffphase1_16 WHEN ( cur_count = to_unsigned(16, 6) ) ELSE
                      coeffphase1_17 WHEN ( cur_count = to_unsigned(17, 6) ) ELSE
                      coeffphase1_18 WHEN ( cur_count = to_unsigned(18, 6) ) ELSE
                      coeffphase1_19 WHEN ( cur_count = to_unsigned(19, 6) ) ELSE
                      coeffphase1_20 WHEN ( cur_count = to_unsigned(20, 6) ) ELSE
                      coeffphase1_21 WHEN ( cur_count = to_unsigned(21, 6) ) ELSE
                      coeffphase1_22 WHEN ( cur_count = to_unsigned(22, 6) ) ELSE
                      coeffphase1_23 WHEN ( cur_count = to_unsigned(23, 6) ) ELSE
                      coeffphase1_25 WHEN ( cur_count = to_unsigned(24, 6) ) ELSE
                      coeffphase1_26 WHEN ( cur_count = to_unsigned(25, 6) ) ELSE
                      coeffphase1_27 WHEN ( cur_count = to_unsigned(26, 6) ) ELSE
                      coeffphase1_28 WHEN ( cur_count = to_unsigned(27, 6) ) ELSE
                      coeffphase1_29 WHEN ( cur_count = to_unsigned(28, 6) ) ELSE
                      coeffphase2_1 WHEN ( cur_count = to_unsigned(29, 6) ) ELSE
                      coeffphase2_2 WHEN ( cur_count = to_unsigned(30, 6) ) ELSE
                      coeffphase2_3 WHEN ( cur_count = to_unsigned(31, 6) ) ELSE
                      coeffphase2_4 WHEN ( cur_count = to_unsigned(32, 6) ) ELSE
                      coeffphase2_5 WHEN ( cur_count = to_unsigned(33, 6) ) ELSE
                      coeffphase2_7 WHEN ( cur_count = to_unsigned(34, 6) ) ELSE
                      coeffphase2_8 WHEN ( cur_count = to_unsigned(35, 6) ) ELSE
                      coeffphase2_9 WHEN ( cur_count = to_unsigned(36, 6) ) ELSE
                      coeffphase2_10 WHEN ( cur_count = to_unsigned(37, 6) ) ELSE
                      coeffphase2_11 WHEN ( cur_count = to_unsigned(38, 6) ) ELSE
                      coeffphase2_12 WHEN ( cur_count = to_unsigned(39, 6) ) ELSE
                      coeffphase2_13 WHEN ( cur_count = to_unsigned(40, 6) ) ELSE
                      coeffphase2_14 WHEN ( cur_count = to_unsigned(41, 6) ) ELSE
                      coeffphase2_15 WHEN ( cur_count = to_unsigned(42, 6) ) ELSE
                      coeffphase2_16 WHEN ( cur_count = to_unsigned(43, 6) ) ELSE
                      coeffphase2_17 WHEN ( cur_count = to_unsigned(44, 6) ) ELSE
                      coeffphase2_18 WHEN ( cur_count = to_unsigned(45, 6) ) ELSE
                      coeffphase2_19 WHEN ( cur_count = to_unsigned(46, 6) ) ELSE
                      coeffphase2_20 WHEN ( cur_count = to_unsigned(47, 6) ) ELSE
                      coeffphase2_21 WHEN ( cur_count = to_unsigned(48, 6) ) ELSE
                      coeffphase2_22 WHEN ( cur_count = to_unsigned(49, 6) ) ELSE
                      coeffphase2_23 WHEN ( cur_count = to_unsigned(50, 6) ) ELSE
                      coeffphase2_24 WHEN ( cur_count = to_unsigned(51, 6) ) ELSE
                      coeffphase2_25 WHEN ( cur_count = to_unsigned(52, 6) ) ELSE
                      coeffphase2_26 WHEN ( cur_count = to_unsigned(53, 6) ) ELSE
                      coeffphase2_27 WHEN ( cur_count = to_unsigned(54, 6) ) ELSE
                      coeffphase2_28 WHEN ( cur_count = to_unsigned(55, 6) ) ELSE
                      coeffphase2_29;
  product <= inputmux * product_mux;


  -- Implementing products without a multiplier for coefficients with values equal to a power of 2.

  -- value of 'coeffphase1_24' is 0.015625

  prod_powertwo_1_24 <= resize(input_pipeline_phase0(23)(31 DOWNTO 0) & '0' & '0' & '0' & '0' & '0' & '0' & '0' & '0' & '0', 48);

  -- value of 'coeffphase2_6' is 0.015625

  prod_powertwo_2_6 <= resize(input_pipeline_phase1(5)(31 DOWNTO 0) & '0' & '0' & '0' & '0' & '0' & '0' & '0' & '0' & '0', 48);

  -- Mux(es) to select the power of 2 products for the corresponding polyphase

  powertwo_mux_1_24 <= prod_powertwo_1_24 WHEN ( phase_1 = '1' ) ELSE
                            const_zero;
  powertwo_mux_2_6 <= prod_powertwo_2_6 WHEN ( phase_29 = '1' ) ELSE
                           const_zero;

  -- Add the products in linear fashion

  add_temp <= resize(product, 49) + resize(powertwo_mux_1_24, 49);
  sum_1 <= resize(add_temp, 50);

  add_temp_1 <= resize(sum_1, 51) + resize(powertwo_mux_2_6, 51);
  sumofproducts <= add_temp_1(49 DOWNTO 0);

  -- Resize the sum of products to the accumulator type for full precision addition

  sumofproducts_cast <= resize(sumofproducts, 77);

  -- Accumulator register with a mux to reset it with the first addend

  add_temp_2 <= resize(sumofproducts_cast, 78) + resize(accreg_out, 78);
  acc_sum <= add_temp_2(76 DOWNTO 0);

  accreg_in <= sumofproducts_cast WHEN ( phase_29 = '1' ) ELSE
                    acc_sum;

  Acc_reg_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      accreg_out <= (OTHERS => '0');
    ELSIF clk'event AND clk = '1' THEN
      IF clk_enable = '1' THEN
        accreg_out <= accreg_in;
      END IF;
    END IF; 
  END PROCESS Acc_reg_process;

  -- Register to hold the final value of the accumulated sum

  Acc_finalreg_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      accreg_final <= (OTHERS => '0');
    ELSIF clk'event AND clk = '1' THEN
      IF phase_29 = '1' THEN
        accreg_final <= accreg_out;
      END IF;
    END IF; 
  END PROCESS Acc_finalreg_process;

  output_typeconvert <= resize(shift_right(accreg_final(46 DOWNTO 0) + ( "0" & (accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15) & NOT accreg_final(15))), 15), 32);

  output_register_process : PROCESS (clk, reset)
  BEGIN
    IF reset = '1' THEN
      output_register <= (OTHERS => '0');
    ELSIF clk'event AND clk = '1' THEN
      IF phase_reg = '1' THEN
        output_register <= output_typeconvert;
      END IF;
    END IF; 
  END PROCESS output_register_process;

  -- Assignment Statements
  ce_out <= ce_out_reg;
  filter_out <= std_logic_vector(output_register);
END rtl;
