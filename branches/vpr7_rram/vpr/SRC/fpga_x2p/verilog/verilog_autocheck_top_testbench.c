/***********************************/
/*  Dump Synthesizable Veriolog    */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* Include vpr structs*/
#include "util.h"
#include "physical_types.h"
#include "vpr_types.h"
#include "globals.h"
#include "rr_graph.h"
#include "route_common.h"
#include "vpr_utils.h"

/* Include spice support headers*/
#include "read_xml_spice_util.h"
#include "linkedlist.h"
#include "fpga_x2p_types.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_pbtypes_utils.h"
#include "fpga_x2p_backannotate_utils.h"
#include "fpga_x2p_bitstream_utils.h"
#include "fpga_x2p_globals.h"
#include "fpga_bitstream.h"

/* Include verilog support headers*/
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_routing.h"
#include "verilog_pbtypes.h"
#include "verilog_decoder.h"
#include "verilog_top_netlist_utils.h"
#include "verilog_top_testbench.h"

/* Local Subroutines declaration */

/******** Subroutines ***********/
static 
void dump_verilog_top_auto_testbench_ports(FILE* fp,
                                           t_sram_orgz_info* cur_sram_orgz_info,
                                           char* circuit_name,
                                           t_syn_verilog_opts fpga_verilog_opts){
  int num_array_bl, num_array_wl;
  int bl_decoder_size, wl_decoder_size;
  int iblock, iopad_idx;
  t_spice_model* mem_model = NULL;
  char* port_name = NULL;
 
  get_sram_orgz_info_mem_model(cur_sram_orgz_info, &mem_model);

  fprintf(fp, "`include \"%s\"\n", fpga_verilog_opts.reference_verilog_benchmark_file);

  fprintf(fp, "module %s_top_autocheck_tb;\n", circuit_name);
  /* Local wires */
  /* 1. reset, set, clock signals */
  /* 2. iopad signals */

  /* Connect to defined signals */
  /* set and reset signals */
  fprintf(fp, "\n");
  dump_verilog_top_testbench_global_ports(fp, global_ports_head, VERILOG_PORT_WIRE);
  fprintf(fp, "\n");

  /* TODO: dump each global signal as reg here */

  /* Inputs and outputs of I/O pads */
  /* Inout Pads */
  assert(NULL != iopad_verilog_model);
  if ((NULL == iopad_verilog_model)
   ||(iopad_verilog_model->cnt > 0)) {
    /* Malloc and assign port_name */
    port_name = (char*)my_malloc(sizeof(char)*(strlen(gio_inout_prefix) + strlen(iopad_verilog_model->prefix) + 1));
    sprintf(port_name, "%s%s", gio_inout_prefix, iopad_verilog_model->prefix);
    /* Dump a wired port */
    dump_verilog_generic_port(fp, VERILOG_PORT_WIRE, 
                              port_name, iopad_verilog_model->cnt - 1, 0);
    fprintf(fp, "; //--- FPGA inouts \n"); 
    /* Free port_name */
    my_free(port_name);
    /* Malloc and assign port_name */
    port_name = (char*)my_malloc(sizeof(char)*(strlen(gio_inout_prefix) + strlen(iopad_verilog_model->prefix) + strlen(top_tb_inout_reg_postfix) + 1));
    sprintf(port_name, "%s%s%s", gio_inout_prefix, iopad_verilog_model->prefix, top_tb_inout_reg_postfix);
    /* Dump a wired port */
    dump_verilog_generic_port(fp, VERILOG_PORT_REG, 
                              port_name, iopad_verilog_model->cnt - 1, 0);
    fprintf(fp, "; //--- reg for FPGA inouts \n"); 
    /* Free port_name */
    my_free(port_name);
  }

  /* Add a signal to identify the configuration phase is finished */
  fprintf(fp, "reg [0:0] %s;\n", top_tb_config_done_port_name);
  /* Programming clock */
  fprintf(fp, "wire [0:0] %s;\n", top_tb_prog_clock_port_name);
  fprintf(fp, "reg [0:0] %s%s;\n", top_tb_prog_clock_port_name, top_tb_clock_reg_postfix);
  /* Operation clock */
  fprintf(fp, "wire [0:0] %s;\n", top_tb_op_clock_port_name);
  fprintf(fp, "reg [0:0] %s%s;\n", top_tb_op_clock_port_name, top_tb_clock_reg_postfix);
  /* Programming set and reset */
  fprintf(fp, "reg [0:0] %s;\n", top_tb_prog_reset_port_name);
  fprintf(fp, "reg [0:0] %s;\n", top_tb_prog_set_port_name);
  /* Global set and reset */
  fprintf(fp, "reg [0:0] %s;\n", top_tb_reset_port_name);
  fprintf(fp, "reg [0:0] %s;\n", top_tb_set_port_name);
  /* Generate stimuli for global ports or connect them to existed signals */
  dump_verilog_top_testbench_global_ports_stimuli(fp, global_ports_head);

  /* Configuration ports depend on the organization of SRAMs */
  switch(cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
    dump_verilog_generic_port(fp, VERILOG_PORT_WIRE, 
                              sram_verilog_model->prefix, sram_verilog_model->cnt - 1, 0);
    fprintf(fp, "; //---- SRAM outputs \n");
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    /* We put the head of scan-chains here  
   */
    dump_verilog_generic_port(fp, VERILOG_PORT_REG, 
                              top_netlist_scan_chain_head_prefix, 0, 0);
    fprintf(fp, "; //---- Scan-chain head \n");
    break;
  case SPICE_SRAM_MEMORY_BANK:
    /* Get the number of array BLs/WLs, decoder sizes */
    determine_blwl_decoder_size(cur_sram_orgz_info,
                                &num_array_bl, &num_array_wl, &bl_decoder_size, &wl_decoder_size);

    fprintf(fp, "  wire [0:0] %s;\n",
            top_netlist_bl_enable_port_name);
    fprintf(fp, "  wire [0:0] %s;\n",
            top_netlist_wl_enable_port_name);
    /* Wire en_bl, en_wl to prog_clock */
    fprintf(fp, "assign %s[0:0] = %s[0:0];\n",
            top_netlist_bl_enable_port_name,
            top_tb_prog_clock_port_name);
    fprintf(fp, "assign %s [0:0]= %s[0:0];\n",
            top_netlist_wl_enable_port_name,
            top_tb_prog_clock_port_name);
    dump_verilog_generic_port(fp, VERILOG_PORT_REG, 
                              top_netlist_addr_bl_port_name, bl_decoder_size - 1, 0);
    fprintf(fp, "; //--- Address of bit lines \n"); 
    dump_verilog_generic_port(fp, VERILOG_PORT_REG, 
                              top_netlist_addr_wl_port_name, wl_decoder_size - 1, 0);
    fprintf(fp, "; //--- Address of word lines \n"); 
    /* data_in is only require by BL decoder of SRAM array 
     * As for RRAM array, the data_in signal will not be used 
     */
    if (SPICE_MODEL_DESIGN_CMOS == mem_model->design_tech) {
      fprintf(fp, "  reg [0:0] %s; // --- Data_in signal for BL decoder, only required by SRAM array \n",
                     top_netlist_bl_data_in_port_name);
    }
    /* I add all the Bit lines and Word lines here just for testbench usage
    fprintf(fp, "  input wire [%d:0] %s_out; //--- Bit lines \n", 
                   sram_verilog_model->cnt - 1, sram_verilog_model->prefix);
    fprintf(fp, "  input wire [%d:0] %s_outb; //--- Word lines \n", 
                   sram_verilog_model->cnt - 1, sram_verilog_model->prefix);
    */
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Add signals from blif benchmark and short-wire them to FPGA I/O PADs
   * This brings convenience to checking functionality  
   */
  fprintf(fp, "//-----Link Blif Benchmark inputs to FPGA IOPADs -----\n");
  for (iblock = 0; iblock < num_logical_blocks; iblock++) {
    /* General INOUT*/
    if (iopad_verilog_model == logical_block[iblock].mapped_spice_model) {
      iopad_idx = logical_block[iblock].mapped_spice_model_index;
      /* Make sure We find the correct logical block !*/
      assert((VPACK_INPAD == logical_block[iblock].type)
           ||(VPACK_OUTPAD == logical_block[iblock].type));
      fprintf(fp, "//----- Blif Benchmark inout %s is mapped to FPGA IOPAD %s[%d] -----\n", 
              logical_block[iblock].name, gio_inout_prefix, iopad_idx);
      fprintf(fp, "wire %s_%s_%d_;\n",
              logical_block[iblock].name, gio_inout_prefix, iopad_idx);
      fprintf(fp, "assign %s_%s_%d_ = %s%s[%d];\n",
              logical_block[iblock].name, gio_inout_prefix, iopad_idx,
              gio_inout_prefix, iopad_verilog_model->prefix, iopad_idx);
      // AA: Generate wire and reg to autocheck with benchmark
      if(VPACK_OUTPAD == logical_block[iblock].type) {    
        fprintf(fp, "wire %s_benchmark;\n", logical_block[iblock].name);
        fprintf(fp, "reg %s_verification;\n", logical_block[iblock].name);
      }  
    }
  }

  return;
}

static
void dump_verilog_top_auto_testbench_call_benchmark(FILE* fp, 
                                                    char* reference_verilog_top_name){
  int iblock, iopad_idx;

  fprintf(fp, "// Benchmark instanciation\n");
  fprintf(fp, "  %s Benchmark(\n", reference_verilog_top_name);

  for (iblock = 0; iblock < num_logical_blocks; iblock++) {
    /* General INOUT*/
    if (iopad_verilog_model == logical_block[iblock].mapped_spice_model) {
      iopad_idx = logical_block[iblock].mapped_spice_model_index;
      /* Make sure We find the correct logical block !*/
      assert((VPACK_INPAD == logical_block[iblock].type)
           ||(VPACK_OUTPAD == logical_block[iblock].type));
      if(iblock > 0){
        fprintf(fp, ",\n");
      }
      if(VPACK_INPAD == logical_block[iblock].type){
      /*  See if this is a clock net */
        if (TRUE == logical_block[iblock].is_clock) {  
          fprintf(fp, "        %s", top_tb_op_clock_port_name);
        } else{
          fprintf(fp, "        %s_%s_%d_", logical_block[iblock].name, gio_inout_prefix, iopad_idx);
        }
      } else if(VPACK_OUTPAD == logical_block[iblock].type){
        fprintf(fp, "        %s_benchmark", logical_block[iblock].name);
      }
    }
  }
  fprintf(fp, " );\n");
  fprintf(fp, "// End Benchmark instanciation\n\n");

  return;
}

static
void dump_verilog_top_auto_testbench_check(FILE* fp){
  int iblock, iopad_idx;
  fprintf(fp, "  // Begin checking\n");
  fprintf(fp, "  always@(negedge %s) begin\n", top_tb_op_clock_port_name);
  for (iblock = 0; iblock < num_logical_blocks; iblock++) {
    if (iopad_verilog_model == logical_block[iblock].mapped_spice_model) {
      iopad_idx = logical_block[iblock].mapped_spice_model_index;
      /* Make sure We find the correct logical block !*/
      assert((VPACK_INPAD == logical_block[iblock].type)
           ||(VPACK_OUTPAD == logical_block[iblock].type));
      if(VPACK_OUTPAD == logical_block[iblock].type){
        fprintf(fp, "    %s_verification <= %s_benchmark ^ %s_%s_%d_ ;\n", 
                logical_block[iblock].name, 
                logical_block[iblock].name, 
                logical_block[iblock].name, 
                gio_inout_prefix, iopad_idx);
      }
    }
  }
  fprintf(fp, "  end\n\n");
  for (iblock = 0; iblock < num_logical_blocks; iblock++) {
    if (iopad_verilog_model == logical_block[iblock].mapped_spice_model) {
      iopad_idx = logical_block[iblock].mapped_spice_model_index;
      /* Make sure We find the correct logical block !*/
      assert((VPACK_INPAD == logical_block[iblock].type)
           ||(VPACK_OUTPAD == logical_block[iblock].type));
      if(VPACK_OUTPAD == logical_block[iblock].type){
        fprintf(fp, "  always@(posedge %s_verification) begin\n", logical_block[iblock].name);
        fprintf(fp, "      if(%s_verification) begin\n", logical_block[iblock].name);
        fprintf(fp, "        $display(\"Mismatch on %s_verification\");\n", logical_block[iblock].name);
        fprintf(fp, "        $finish;\n");
        fprintf(fp, "      end\n");
        fprintf(fp, "  end\n\n");
      }
    }
  }
  return;
}

void dump_verilog_autocheck_top_testbench(t_sram_orgz_info* cur_sram_orgz_info,
                                          char* circuit_name,
                                          char* top_netlist_name,
                                          int num_clock,
                                          t_syn_verilog_opts fpga_verilog_opts,
                                          t_spice verilog) {
  FILE* fp = NULL;
  char* title = my_strcat("FPGA Verilog Testbench for Top-level netlist of Design: ", circuit_name);
  
  /* Check if the path exists*/
  fp = fopen(top_netlist_name,"w");
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "(FILE:%s,LINE[%d])Failure in create top Verilog testbench %s!",
               __FILE__, __LINE__, top_netlist_name); 
    exit(1);
  } 
  
  vpr_printf(TIO_MESSAGE_INFO, 
             "Writing Autocheck Testbench for FPGA Top-level Verilog netlist for  %s...\n", 
             circuit_name);
 
  /* Print the title */
  dump_verilog_file_header(fp, title);
  my_free(title);

  /* Print preprocessing flags */
  dump_verilog_preproc(fp, 
                       fpga_verilog_opts, 
                       VERILOG_TB_AUTOCHECK_TOP);

  /* Start of testbench */
  dump_verilog_top_auto_testbench_ports(fp, cur_sram_orgz_info, circuit_name, fpga_verilog_opts);

  /* Call defined top-level module */
  dump_verilog_top_testbench_call_top_module(cur_sram_orgz_info, fp, circuit_name);

  /* Call defined benchmark */
  dump_verilog_top_auto_testbench_call_benchmark(fp, blif_circuit_name);

  /* Add stimuli for reset, set, clock and iopad signals */
  dump_verilog_top_testbench_stimuli(cur_sram_orgz_info, fp, num_clock, fpga_verilog_opts, verilog);

  /* Add output autocheck */
  dump_verilog_top_auto_testbench_check(fp);

  /* Testbench ends*/
  fprintf(fp, "endmodule\n");

  /* Close the file*/
  fclose(fp);

  return;
}

