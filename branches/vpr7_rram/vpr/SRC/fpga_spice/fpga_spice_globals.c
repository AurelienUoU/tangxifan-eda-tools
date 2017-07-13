/***********************************/
/*        FPGA-SPICE for VPR       */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include "spice_types.h"
#include "linkedlist.h"
#include "fpga_spice_globals.h"

/* Global variables to be shared by different tools of FPGA-SPICE */
/* SRAM SPICE MODEL should be set as global*/
t_spice_model* fpga_spice_sram_model = NULL;
enum e_sram_orgz fpga_spice_sram_orgz_type = SPICE_SRAM_STANDALONE;
/* Input and Output Pad spice model. should be set as global */
t_spice_model* fpga_spice_inpad_model = NULL;
t_spice_model* fpga_spice_outpad_model = NULL;
t_spice_model* fpga_spice_iopad_model = NULL;

/* Prefix of global input, output and inout of a I/O pad */
char* gio_inout_prefix = "gfpga_pad_";

/* Number of configuration bits of each switch block */
int** num_conf_bits_sb = NULL;
/* Number of configuration bits of each Connection Box CHANX */
int** num_conf_bits_cbx = NULL;
/* Number of configuration bits of each Connection Box CHANY */
int** num_conf_bits_cby = NULL;

/* Linked list for global ports */
t_llist* global_ports_head = NULL;

/* Linked list for verilog and spice syntax char */
t_llist* reserved_syntax_char_head = NULL;

/* Default value of a signal */
int default_signal_init_value = 0;

/* Default do parasitic net estimation !!!*/
int run_parasitic_net_estimation = 1;

