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
#include "verilog_top_netlist.h"

/* Local Subroutines declaration */

/******** Subroutines ***********/

/* Connect BLs and WLs to configuration bus in the top-level Verilog netlist*/
static 
void dump_verilog_top_netlist_memory_bank_internal_wires(t_sram_orgz_info* cur_sram_orgz_info, 
                                                         FILE* fp) {
  t_spice_model* mem_model = NULL;
  int num_bl, num_wl;
  int num_array_bl, num_array_wl;
  int num_reserved_bl, num_reserved_wl;
  int bl_decoder_size, wl_decoder_size;
  int num_blb_ports, num_wlb_ports;
  t_spice_model_port** blb_port = NULL;
  t_spice_model_port** wlb_port = NULL;
  
  /* Check */
  assert (cur_sram_orgz_info->type == SPICE_SRAM_MEMORY_BANK);

  /* A valid file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Depending on the memory technology*/
  get_sram_orgz_info_mem_model(cur_sram_orgz_info, &mem_model);
  assert(NULL != mem_model);

  /* Get the total number of BLs and WLs */
  get_sram_orgz_info_num_blwl(cur_sram_orgz_info, &num_bl, &num_wl);
  /* Get the reserved BLs and WLs */
  get_sram_orgz_info_reserved_blwl(cur_sram_orgz_info, &num_reserved_bl, &num_reserved_wl);

  determine_blwl_decoder_size(cur_sram_orgz_info,
                              &num_array_bl, &num_array_wl, &bl_decoder_size, &wl_decoder_size);

  /* Get BLB and WLB ports */
  find_blb_wlb_ports_spice_model(mem_model, &num_blb_ports, &blb_port,
                                 &num_wlb_ports, &wlb_port);
  /* Get inverter spice_model */

  /* Important!!!:
   * BL/WL should always start from LSB to MSB!
   * In order to follow this convention in primitive nodes. 
   */
  /* No. of BLs and WLs in the array */
  dump_verilog_generic_port(fp, VERILOG_PORT_WIRE,
                            top_netlist_array_bl_port_name, 0, num_array_bl - 1);
  fprintf(fp, "; //--- Array Bit lines bus \n");
  dump_verilog_generic_port(fp, VERILOG_PORT_WIRE,
                            top_netlist_array_wl_port_name, 0, num_array_wl - 1);
  fprintf(fp, "; //--- Array Bit lines bus \n");
  if (1 == num_blb_ports) {
    dump_verilog_generic_port(fp, VERILOG_PORT_WIRE,
                              top_netlist_array_blb_port_name, 0, num_array_bl - 1);
    fprintf(fp, "; //--- Inverted Array Bit lines bus \n");
  }
  if (1 == num_wlb_ports) {
    dump_verilog_generic_port(fp, VERILOG_PORT_WIRE,
                              top_netlist_array_wlb_port_name, 0, num_array_wl - 1);
    fprintf(fp, "; //--- Inverted Array Word lines bus \n");
  }
  fprintf(fp, "\n");

  switch (mem_model->design_tech) {
  case SPICE_MODEL_DESIGN_CMOS:
    assert( 0 == num_reserved_bl );
    assert( 0 == num_reserved_wl );
    /* SRAMs are place in an array
     * BLs of SRAMs in the same column are connected to a common BL
     * BLs of SRAMs in the same row are connected to a common WL
     */
    /* Declare wires */
    fprintf(fp, "  wire [%d:%d] %s%s; //---- Normal Bit lines \n",
            0, num_bl - 1, mem_model->prefix, top_netlist_normal_bl_port_postfix);
    fprintf(fp, "  wire [%d:%d] %s%s; //---- Normal Word lines \n",
            0, num_wl - 1, mem_model->prefix, top_netlist_normal_wl_port_postfix);
    /* Declare inverted wires if needed */
    if (1 == num_blb_ports) {
      fprintf(fp, " wire [%d:%d] %s%s; //---- Inverted Normal Bit lines \n",
              0, num_bl - 1, mem_model->prefix, top_netlist_normal_blb_port_postfix);
    }
    if (1 == num_wlb_ports) {
      fprintf(fp, " wire [%d:%d] %s%s; //---- Inverted Normal Word lines \n",
              0, num_wl - 1, mem_model->prefix, top_netlist_normal_wlb_port_postfix);
    }
    /* Dump ports only visible during formal verification*/
    fprintf(fp, "`ifdef %s\n", verilog_formal_verification_preproc_flag);
    dump_verilog_formal_verification_sram_ports(fp, cur_sram_orgz_info, 
                                                0, get_sram_orgz_info_num_mem_bit(cur_sram_orgz_info) - 1,
                                                VERILOG_PORT_WIRE);
    fprintf(fp, ";\n");
    fprintf(fp, "`endif\n");
    break; 
  case SPICE_MODEL_DESIGN_RRAM: 
    /* Check: there should be reserved BLs and WLs */
    assert( 0 < num_reserved_bl );
    assert( 0 < num_reserved_wl );
    /* Declare reserved and normal conf_bits ports  */
    fprintf(fp, "  wire [0:%d] %s%s; //---- Reserved Bit lines \n",
            num_reserved_bl - 1, mem_model->prefix, top_netlist_reserved_bl_port_postfix);
    fprintf(fp, "  wire [0:%d] %s%s; //---- Reserved Word lines \n",
            num_reserved_wl - 1, mem_model->prefix, top_netlist_reserved_wl_port_postfix);
    fprintf(fp, "  wire [%d:%d] %s%s; //---- Normal Bit lines \n",
            num_reserved_bl, num_array_bl - 1, mem_model->prefix, top_netlist_normal_bl_port_postfix);
    fprintf(fp, "  wire [%d:%d] %s%s; //---- Normal Word lines \n",
            num_reserved_wl, num_array_wl - 1, mem_model->prefix, top_netlist_normal_wl_port_postfix);
    /* TODO: Dump ports only visible during formal verification*/
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  return;
}

/* Delcare primary inputs/outputs for scan-chains in the top-level netlists
 */
static 
void dump_verilog_top_netlist_scan_chain_ports(t_sram_orgz_info* cur_sram_orgz_info, 
                                               FILE* fp,
                                               enum e_dump_verilog_port_type dump_port_type) {
  /* Only accept two types of dump_port_type here! */
  assert((VERILOG_PORT_INPUT == dump_port_type)||(VERILOG_PORT_CONKT == dump_port_type));

  /* Check */
  assert (cur_sram_orgz_info->type == SPICE_SRAM_SCAN_CHAIN);

  /* A valid file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Only the head of scan-chain will be the primary input in the top-level netlist 
   * TODO: we may have multiple scan-chains, their heads will be the primary outputs 
   */
  dump_verilog_generic_port(fp, dump_port_type,
                            top_netlist_scan_chain_head_prefix, 0, 0);
  fprintf(fp, " //---- Scan-chain head \n"); 

  return;
}

/* Connect scan-chain flip-flops in the top-level netlist */
static 
void dump_verilog_top_netlist_scan_chain_internal_wires(t_sram_orgz_info* cur_sram_orgz_info, 
                                                        FILE* fp) {
  t_spice_model* scff_mem_model = NULL;
  int num_scffs;

  /* A valid file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  num_scffs = get_sram_orgz_info_num_mem_bit(cur_sram_orgz_info);
  get_sram_orgz_info_mem_model(cur_sram_orgz_info, &scff_mem_model);
  /* Check */
  assert( SPICE_MODEL_SCFF == scff_mem_model->type );

  /* Delcare local wires */
  fprintf(fp, "  wire [0:%d] %s_scff_in_local_bus;\n",
          num_scffs - 1, scff_mem_model->prefix);

  fprintf(fp, "  wire [0:%d] %s_scff_out_local_bus;\n",
          num_scffs - 1, scff_mem_model->prefix);

  /* Dump ports only visible during formal verification*/
  fprintf(fp, "`ifdef %s\n", verilog_formal_verification_preproc_flag);
  fprintf(fp, "  ");
  dump_verilog_formal_verification_sram_ports(fp, cur_sram_orgz_info, 
                                              0, num_scffs - 1,
                                              VERILOG_PORT_WIRE);
  fprintf(fp, ";\n");
  fprintf(fp, "`endif\n");


  /* Exception for head: connect to primary inputs */ 
  /*
  fprintf(fp, "  assign %s_scff_in[%d] = %s;\n",
          scff_mem_model->prefix, 0,
          top_netlist_scan_chain_head_prefix);
  */
  /* Connected the scan-chain flip-flops */
  /* Ensure we are in the correct range */
  /*
  fprintf(fp, "  genvar i;\n");
  fprintf(fp, "  generate\n");
  fprintf(fp, "    for (i = %d; i < %d; i = i + 1) begin\n", 
          1, num_scffs - 1);
  fprintf(fp,   "assign %s_scff_in[i] = %s_scff_out[i - 1];\n",
            scff_mem_model->prefix, 
            scff_mem_model->prefix);
  fprintf(fp, "    end\n");
  fprintf(fp, "  endgenerate;\n");
  */

  return;
}

/* Dump ports for the top-level module in Verilog netlist */
void dump_verilog_top_module_ports(t_sram_orgz_info* cur_sram_orgz_info, 
                                   FILE* fp,
                                   enum e_dump_verilog_port_type dump_port_type) {
  char* port_name = NULL;
  char split_sign;
  enum e_dump_verilog_port_type actual_dump_port_type;
  boolean dump_global_port_type = FALSE;

  split_sign = determine_verilog_generic_port_split_sign(dump_port_type);

  /* Only accept two types of dump_port_type here! */
  assert((VERILOG_PORT_INPUT == dump_port_type)||(VERILOG_PORT_CONKT == dump_port_type));

  if (VERILOG_PORT_INPUT == dump_port_type) {
     dump_global_port_type = TRUE;
  }

  /* dump global ports */
  if (0 < dump_verilog_global_ports(fp, global_ports_head, dump_global_port_type)) {
    fprintf(fp, "%c\n", split_sign);
  }
  /* Inputs and outputs of I/O pads */
  /* Inout Pads */
  assert(NULL != iopad_verilog_model);
  if ((NULL == iopad_verilog_model)
   ||(iopad_verilog_model->cnt > 0)) {
    actual_dump_port_type = VERILOG_PORT_CONKT;
    if (VERILOG_PORT_INPUT == dump_port_type) {
      actual_dump_port_type = VERILOG_PORT_INOUT;
    }
    /* Malloc and assign port_name */
    port_name = (char*)my_malloc(sizeof(char)*(strlen(gio_inout_prefix) + strlen(iopad_verilog_model->prefix) + 1));
    sprintf(port_name, "%s%s", gio_inout_prefix, iopad_verilog_model->prefix);
    /* Dump a register port */
    dump_verilog_generic_port(fp, actual_dump_port_type, 
                              port_name, iopad_verilog_model->cnt - 1, 0);
    fprintf(fp, "%c //---FPGA inouts \n", split_sign); 
    /* Free port_name */
    my_free(port_name);
  }

  /* Configuration ports depend on the organization of SRAMs */
  switch(cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
    dump_verilog_generic_port(fp, dump_port_type, 
                              sram_verilog_model->prefix, sram_verilog_model->cnt - 1, 0);
    fprintf(fp, " //--- SRAM outputs \n"); 
    /* Definition ends */
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    dump_verilog_top_netlist_scan_chain_ports(cur_sram_orgz_info, fp, dump_port_type);
    /* Definition ends */
    break;
  case SPICE_SRAM_MEMORY_BANK:
    dump_verilog_decoder_memory_bank_ports(cur_sram_orgz_info, fp, dump_port_type);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  return; 
}

  
/* Dump ports for the top-level Verilog netlist */
void dump_verilog_top_netlist_ports(t_sram_orgz_info* cur_sram_orgz_info,
                                    FILE* fp,
                                    int num_clocks,
                                    char* circuit_name,
                                    t_spice verilog) {
  /* 
  int num_array_bl, num_array_wl;
  int bl_decoder_size, wl_decoder_size;
  char* port_name = NULL;
  */

  /* A valid file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  fprintf(fp, "//----- Top-level Verilog Module -----\n");
  fprintf(fp, "module %s_top (\n", circuit_name);
  fprintf(fp, "\n");

  dump_verilog_top_module_ports(cur_sram_orgz_info, fp, VERILOG_PORT_INPUT);

  fprintf(fp, ");\n");

  return;
}
 

void dump_verilog_top_netlist_internal_wires(t_sram_orgz_info* cur_sram_orgz_info, 
                                             FILE* fp) {
  /* Configuration ports depend on the organization of SRAMs */
  switch(cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
    /* Definition ends */
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    dump_verilog_top_netlist_scan_chain_internal_wires(cur_sram_orgz_info, fp);
    /* Definition ends */
    break;
  case SPICE_SRAM_MEMORY_BANK:
    dump_verilog_top_netlist_memory_bank_internal_wires(cur_sram_orgz_info, fp);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  return; 
}

static 
void dump_verilog_defined_one_grid(t_sram_orgz_info* cur_sram_orgz_info, 
                                   FILE* fp,
                                   int ix, int iy) {
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }
  
  if ((NULL == grid[ix][iy].type)
    || (EMPTY_TYPE == grid[ix][iy].type) 
    ||(0 != grid[ix][iy].offset)) {
    return;
  }

  /* Comment lines */
  fprintf(fp, "//----- BEGIN Call Grid[%d][%d] module -----\n", ix, iy);
  /* Print the Grid module */
  fprintf(fp, "grid_%d__%d_  ", ix, iy); /* Call the name of subckt */ 
  fprintf(fp, "grid_%d__%d__0_ ", ix, iy);
  fprintf(fp, "(");
  fprintf(fp, "\n");
  /* dump global ports */
  if (0 < dump_verilog_global_ports(fp, global_ports_head, FALSE)) {
    fprintf(fp, ",\n");
  }

  if (IO_TYPE == grid[ix][iy].type) {
    dump_verilog_io_grid_pins(fp, ix, iy, 1, FALSE, FALSE);
  } else {
    dump_verilog_grid_pins(fp, ix, iy, 1, FALSE, FALSE);
  }
 
  /* IO PAD */
  dump_verilog_grid_common_port(fp, iopad_verilog_model, gio_inout_prefix, 
                                iopad_verilog_model->grid_index_low[ix][iy],
                                iopad_verilog_model->grid_index_high[ix][iy] - 1,
                                VERILOG_PORT_CONKT); 

  /* Print configuration ports */
  /* Reserved configuration ports */
  if (0 < cur_sram_orgz_info->grid_reserved_conf_bits[ix][iy]) {
    fprintf(fp, ",\n");
  }
  dump_verilog_reserved_sram_ports(fp, cur_sram_orgz_info,
                                   0, 
                                   cur_sram_orgz_info->grid_reserved_conf_bits[ix][iy] - 1,
                                   VERILOG_PORT_CONKT);
  /* Normal configuration ports */
  if (0 < (cur_sram_orgz_info->grid_conf_bits_msb[ix][iy]
           - cur_sram_orgz_info->grid_conf_bits_lsb[ix][iy])) {
    fprintf(fp, ",\n");
  }
  dump_verilog_sram_ports(fp, cur_sram_orgz_info,
                          cur_sram_orgz_info->grid_conf_bits_lsb[ix][iy],
                          cur_sram_orgz_info->grid_conf_bits_msb[ix][iy] - 1,
                          VERILOG_PORT_CONKT);
  fprintf(fp, ");\n");
  /* Comment lines */
  fprintf(fp, "//----- END call Grid[%d][%d] module -----\n\n", ix, iy);

  return;
}


/***** Print (call) the defined grids *****/
void dump_verilog_defined_grids(t_sram_orgz_info* cur_sram_orgz_info,
                                FILE* fp) {
  int ix, iy;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Normal Grids */
  for (ix = 1; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      /* Bypass EMPTY grid */
      if (EMPTY_TYPE == grid[ix][iy].type) {
        continue;
      }
      assert(IO_TYPE != grid[ix][iy].type);
      dump_verilog_defined_one_grid(cur_sram_orgz_info, fp, ix, iy);
    }
  } 

  /* IO Grids */
  /* TOP side */
  iy = ny + 1;
  for (ix = 1; ix < (nx + 1); ix++) {
    /* Bypass EMPTY grid */
    if (EMPTY_TYPE == grid[ix][iy].type) {
      continue;
    }
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_defined_one_grid(cur_sram_orgz_info, fp, ix, iy);
  } 

  /* RIGHT side */
  ix = nx + 1;
  for (iy = 1; iy < (ny + 1); iy++) {
    /* Bypass EMPTY grid */
    if (EMPTY_TYPE == grid[ix][iy].type) {
      continue;
    }
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_defined_one_grid(cur_sram_orgz_info, fp, ix, iy);
  }

  /* BOTTOM side */
  iy = 0;
  for (ix = 1; ix < (nx + 1); ix++) {
    /* Bypass EMPTY grid */
    if (EMPTY_TYPE == grid[ix][iy].type) {
      continue;
    }
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_defined_one_grid(cur_sram_orgz_info, fp, ix, iy);
  } 

  /* LEFT side */
  ix = 0;
  for (iy = 1; iy < (ny + 1); iy++) {
    /* Bypass EMPTY grid */
    if (EMPTY_TYPE == grid[ix][iy].type) {
      continue;
    }
    assert(IO_TYPE == grid[ix][iy].type);
    dump_verilog_defined_one_grid(cur_sram_orgz_info, fp, ix, iy);
  }


  return;
}

/* Call defined channels. 
 * Ensure the port name here is co-herent to other sub-circuits(SB,CB,grid)!!!
 */
static 
void dump_verilog_defined_one_channel(FILE* fp,
                                      t_rr_type chan_type, int x, int y,
                                      int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                      t_ivec*** LL_rr_node_indices) {
  int itrack;
  int chan_width = 0;
  t_rr_node** chan_rr_nodes = NULL;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Check */
  assert((CHANX == chan_type)||(CHANY == chan_type));
  /* check x*/
  assert((!(0 > x))&&(x < (nx + 1))); 
  /* check y*/
  assert((!(0 > y))&&(y < (ny + 1))); 

  /* Collect rr_nodes for Tracks for chanx[ix][iy] */
  chan_rr_nodes = get_chan_rr_nodes(&chan_width, chan_type, x, y,
                                    LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices);
 
  /* Comment lines */
  switch (chan_type) {
  case CHANX:
    fprintf(fp, "//----- BEGIN Call Channel-X [%d][%d] module -----\n", x, y);
    break;
  case CHANY:
    fprintf(fp, "//----- BEGIN call Channel-Y [%d][%d] module -----\n\n", x, y);
    break;
  default: 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid Channel Type!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Call the define sub-circuit */
  fprintf(fp, "%s_%d__%d_ ", 
          convert_chan_type_to_string(chan_type),
          x, y);
  fprintf(fp, "%s_%d__%d__0_ ", 
          convert_chan_type_to_string(chan_type),
          x, y);
  fprintf(fp, "(");
  fprintf(fp, "\n");
  /* dump global ports */
  if (0 < dump_verilog_global_ports(fp, global_ports_head, FALSE)) {
    fprintf(fp, ",\n");
  }

  /* LEFT/BOTTOM side port of CHANX/CHANY */
  /* We apply an opposite port naming rule than function: fprint_routing_chan_subckt 
   * In top-level netlists, we follow the same port name as switch blocks and connection blocks 
   * When a track is in INC_DIRECTION, the LEFT/BOTTOM port would be an output of a switch block
   * When a track is in DEC_DIRECTION, the LEFT/BOTTOM port would be an input of a switch block
   */
  for (itrack = 0; itrack < chan_width; itrack++) {
    switch (chan_rr_nodes[itrack]->direction) {
    case INC_DIRECTION:
      fprintf(fp, "%s_%d__%d__out_%d_, ", 
              convert_chan_type_to_string(chan_type),
              x, y, itrack);
      fprintf(fp, "\n");
      break;
    case DEC_DIRECTION:
      fprintf(fp, "%s_%d__%d__in_%d_, ", 
              convert_chan_type_to_string(chan_type),
              x, y, itrack);
      fprintf(fp, "\n");
      break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid direction of %s[%d][%d]_track[%d]!\n",
                 __FILE__, __LINE__,
                 convert_chan_type_to_string(chan_type),
                 x, y, itrack);
      exit(1);
    }
  }
  /* RIGHT/TOP side port of CHANX/CHANY */
  /* We apply an opposite port naming rule than function: fprint_routing_chan_subckt 
   * In top-level netlists, we follow the same port name as switch blocks and connection blocks 
   * When a track is in INC_DIRECTION, the RIGHT/TOP port would be an input of a switch block
   * When a track is in DEC_DIRECTION, the RIGHT/TOP port would be an output of a switch block
   */
  for (itrack = 0; itrack < chan_width; itrack++) {
    switch (chan_rr_nodes[itrack]->direction) {
    case INC_DIRECTION:
      fprintf(fp, "%s_%d__%d__in_%d_, ", 
              convert_chan_type_to_string(chan_type),
              x, y, itrack);
      fprintf(fp, "\n");
      break;
    case DEC_DIRECTION:
      fprintf(fp, "%s_%d__%d__out_%d_, ", 
              convert_chan_type_to_string(chan_type),
              x, y, itrack);
      fprintf(fp, "\n");
      break;
    default:
      vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid direction of %s[%d][%d]_track[%d]!\n",
                 __FILE__, __LINE__,
                 convert_chan_type_to_string(chan_type),
                 x, y, itrack);
      exit(1);
    }
  }

  /* output at middle point */
  for (itrack = 0; itrack < chan_width; itrack++) {
    fprintf(fp, "%s_%d__%d__midout_%d_ ", 
            convert_chan_type_to_string(chan_type),
            x, y, itrack);
    if (itrack < chan_width - 1) {
      fprintf(fp, ",");
    }
    fprintf(fp, "\n");
  }
  fprintf(fp, ");\n");

  /* Comment lines */
  switch (chan_type) {
  case CHANX:
    fprintf(fp, "//----- END Call Channel-X [%d][%d] module -----\n", x, y);
    break;
  case CHANY:
    fprintf(fp, "//----- END call Channel-Y [%d][%d] module -----\n\n", x, y);
    break;
  default: 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid Channel Type!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Free */
  my_free(chan_rr_nodes);

  return;
}

/* Call the sub-circuits for channels : Channel X and Channel Y*/
void dump_verilog_defined_channels(FILE* fp,
                                   int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                   t_ivec*** LL_rr_node_indices) {
  int ix, iy;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Channel X */
  for (iy = 0; iy < (ny + 1); iy++) {
    for (ix = 1; ix < (nx + 1); ix++) {
      dump_verilog_defined_one_channel(fp, CHANX, ix, iy,
                                       LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices);
    }
  }

  /* Channel Y */
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      dump_verilog_defined_one_channel(fp, CHANY, ix, iy,
                                       LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices);
    }
  }

  return;
}

/* Call the defined sub-circuit of connection box
 * TODO: actually most of this function is copied from
 * spice_routing.c : dump_verilog_conneciton_box_interc
 * Should be more clever to use the original function
 */
static 
void dump_verilog_defined_one_connection_box(t_sram_orgz_info* cur_sram_orgz_info, 
                                             FILE* fp,
                                             t_cb cur_cb_info) {
  int itrack, inode, side, x, y;
  int side_cnt = 0;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check */
  assert((!(0 > cur_cb_info.x))&&(!(cur_cb_info.x > (nx + 1)))); 
  assert((!(0 > cur_cb_info.y))&&(!(cur_cb_info.y > (ny + 1)))); 

  x = cur_cb_info.x;
  y = cur_cb_info.y;
  
  /* Print the definition of subckt*/
  /* Identify the type of connection box */
  switch(cur_cb_info.type) {
  case CHANX:
    /* Comment lines */
    fprintf(fp, "//----- BEGIN Call Connection Box-X direction [%d][%d] module -----\n", x, y);
    /* Print module */
    fprintf(fp, "cbx_%d__%d_ ", x, y);
    fprintf(fp, "cbx_%d__%d__0_ ", x, y);
    break;
  case CHANY:
    /* Comment lines */
    fprintf(fp, "//----- BEGIN Call Connection Box-Y direction [%d][%d] module -----\n", x, y);
    /* Print module */
    fprintf(fp, "cby_%d__%d_ ", x, y);
    fprintf(fp, "cby_%d__%d__0_ ", x, y);
    break;
  default: 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid type of channel!\n", __FILE__, __LINE__);
    exit(1);
  }
 
  fprintf(fp, "(");
  fprintf(fp, "\n");
  /* dump global ports */
  if (0 < dump_verilog_global_ports(fp, global_ports_head, FALSE)) {
    fprintf(fp, ",\n");
  }

  /* Print the ports of channels*/
  /* connect to the mid point of a track*/
  side_cnt = 0;
  for (side = 0; side < cur_cb_info.num_sides; side++) {
    /* Bypass side with zero channel width */
    if (0 == cur_cb_info.chan_width[side]) {
      continue;
    }
    assert (0 < cur_cb_info.chan_width[side]);
    side_cnt++;
    fprintf(fp, "//----- %s side inputs: channel track middle outputs -----\n", convert_side_index_to_string(side));
    for (itrack = 0; itrack < cur_cb_info.chan_width[side]; itrack++) {
      fprintf(fp, "%s_%d__%d__midout_%d_, ",
              convert_chan_type_to_string(cur_cb_info.type),
              cur_cb_info.x, cur_cb_info.y, itrack);
      fprintf(fp, "\n");
    }
  }
  /*check side_cnt */
  assert(1 == side_cnt);
 
  side_cnt = 0;
  /* Print the ports of grids*/
  for (side = 0; side < cur_cb_info.num_sides; side++) {
    /* Bypass side with zero IPINs*/
    if (0 == cur_cb_info.num_ipin_rr_nodes[side]) {
      continue;
    }
    side_cnt++;
    assert(0 < cur_cb_info.num_ipin_rr_nodes[side]);
    assert(NULL != cur_cb_info.ipin_rr_node[side]);
    fprintf(fp, "//----- %s side outputs: CLB input pins -----\n", convert_side_index_to_string(side));
    for (inode = 0; inode < cur_cb_info.num_ipin_rr_nodes[side]; inode++) {
      /* Print each INPUT Pins of a grid */
      dump_verilog_grid_side_pin_with_given_index(fp, OPIN,
                                                 cur_cb_info.ipin_rr_node[side][inode]->ptc_num,
                                                 cur_cb_info.ipin_rr_node_grid_side[side][inode],
                                                 cur_cb_info.ipin_rr_node[side][inode]->xlow,
                                                 cur_cb_info.ipin_rr_node[side][inode]->ylow, 
                                                 FALSE); /* Do not specify direction of port */
      fprintf(fp, ", \n");
    }
  }
  /* Make sure only 2 sides of IPINs are printed */
  assert((1 == side_cnt)||(2 == side_cnt));
 
  /* Configuration ports */
  /* Reserved sram ports */
  if (0 < (cur_cb_info.num_reserved_conf_bits)) {
    dump_verilog_reserved_sram_ports(fp, cur_sram_orgz_info, 
                                     0, cur_cb_info.num_reserved_conf_bits - 1,
                                     VERILOG_PORT_CONKT);
    fprintf(fp, ",\n");
  }
  /* Normal sram ports */
  if (0 < (cur_cb_info.conf_bits_msb - cur_cb_info.conf_bits_lsb)) {
    dump_verilog_sram_local_ports(fp, cur_sram_orgz_info, 
                                  cur_cb_info.conf_bits_lsb, cur_cb_info.conf_bits_msb - 1,
                                  VERILOG_PORT_CONKT);
  }
  /* Dump ports only visible during formal verification*/
  if (0 < (cur_cb_info.conf_bits_msb - 1 - cur_cb_info.conf_bits_lsb)) {
    fprintf(fp, "\n");
    fprintf(fp, "`ifdef %s\n", verilog_formal_verification_preproc_flag);
    fprintf(fp, ",\n");
    dump_verilog_formal_verification_sram_ports(fp, cur_sram_orgz_info, 
                                                cur_cb_info.conf_bits_lsb, 
                                                cur_cb_info.conf_bits_msb - 1,
                                                VERILOG_PORT_CONKT);
    fprintf(fp, "\n");
    fprintf(fp, "`endif\n");
  } 
  fprintf(fp, ");\n");

  /* Comment lines */
  switch(cur_cb_info.type) {
  case CHANX:
    fprintf(fp, "//----- END call Connection Box-X direction [%d][%d] module -----\n\n", x, y);
    break;
  case CHANY:
    fprintf(fp, "//----- END call Connection Box-Y direction [%d][%d] module -----\n\n", x, y);
    break;
  default: 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid type of channel!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Check */
  assert((1 == side_cnt)||(2 == side_cnt));
 
  return;
}

/* Call the sub-circuits for connection boxes */
void dump_verilog_defined_connection_boxes(t_sram_orgz_info* cur_sram_orgz_info,
                                           FILE* fp) {
  int ix, iy;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* X - channels [1...nx][0..ny]*/
  for (iy = 0; iy < (ny + 1); iy++) {
    for (ix = 1; ix < (nx + 1); ix++) {
      if ((TRUE == is_cb_exist(CHANX, ix, iy))
         &&(0 < count_cb_info_num_ipin_rr_nodes(cbx_info[ix][iy]))) {
        dump_verilog_defined_one_connection_box(cur_sram_orgz_info, fp, cbx_info[ix][iy]);
      }
    }
  }
  /* Y - channels [1...ny][0..nx]*/
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      if ((TRUE == is_cb_exist(CHANY, ix, iy))
         &&(0 < count_cb_info_num_ipin_rr_nodes(cby_info[ix][iy]))) {
        dump_verilog_defined_one_connection_box(cur_sram_orgz_info, fp, cby_info[ix][iy]);
      }
    }
  }
 
  return; 
}

/* Call the defined switch box sub-circuit
 * TODO: This function is also copied from
 * spice_routing.c : dump_verilog_routing_switch_box_subckt
 */
static 
void dump_verilog_defined_one_switch_box(t_sram_orgz_info* cur_sram_orgz_info, 
                                         FILE* fp,
                                         t_sb cur_sb_info) {
  int ix, iy, side, itrack, x, y, inode;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check */
  assert((!(0 > cur_sb_info.x))&&(!(cur_sb_info.x > (nx + 1)))); 
  assert((!(0 > cur_sb_info.y))&&(!(cur_sb_info.y > (ny + 1)))); 

  x = cur_sb_info.x;
  y = cur_sb_info.y;
                 
  /* Comment lines */                 
  fprintf(fp, "//----- BEGIN call module Switch blocks [%d][%d] -----\n", 
          cur_sb_info.x, cur_sb_info.y);
  /* Print module*/
  fprintf(fp, "sb_%d__%d_ ", cur_sb_info.x, cur_sb_info.y);
  fprintf(fp, "sb_%d__%d__0_ ", cur_sb_info.x, cur_sb_info.y);
  fprintf(fp, "(");

  fprintf(fp, "\n");
  /* dump global ports */
  if (0 < dump_verilog_global_ports(fp, global_ports_head, FALSE)) {
    fprintf(fp, ",\n");
  }

  for (side = 0; side < cur_sb_info.num_sides; side++) {
    determine_sb_port_coordinator(cur_sb_info, side, &ix, &iy); 

    fprintf(fp, "//----- %s side channel ports-----\n", convert_side_index_to_string(side));
    for (itrack = 0; itrack < cur_sb_info.chan_width[side]; itrack++) {
      switch (cur_sb_info.chan_rr_node_direction[side][itrack]) {
      case OUT_PORT:
        fprintf(fp, "%s_%d__%d__out_%d_, ", 
                convert_chan_type_to_string(cur_sb_info.chan_rr_node[side][itrack]->type), 
                ix, iy, itrack); 
        break;
      case IN_PORT:
        fprintf(fp, "%s_%d__%d__in_%d_, ",
                convert_chan_type_to_string(cur_sb_info.chan_rr_node[side][itrack]->type), 
                ix, iy, itrack); 
        break;
      default:
        vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid direction of sb[%d][%d] side[%d] track[%d]!\n",
                   __FILE__, __LINE__, x, y, side, itrack);
        exit(1);
      }
    }
    fprintf(fp, "\n");
    fprintf(fp, "//----- %s side inputs: CLB output pins -----\n", convert_side_index_to_string(side));
    /* Dump OPINs of adjacent CLBs */
    for (inode = 0; inode < cur_sb_info.num_opin_rr_nodes[side]; inode++) {
      dump_verilog_grid_side_pin_with_given_index(fp, IPIN,
                                                  cur_sb_info.opin_rr_node[side][inode]->ptc_num,
                                                  cur_sb_info.opin_rr_node_grid_side[side][inode],
                                                  cur_sb_info.opin_rr_node[side][inode]->xlow,
                                                  cur_sb_info.opin_rr_node[side][inode]->ylow,
                                                  FALSE); /* Do not specify the direction of port */ 
      fprintf(fp, ",");
    } 
    fprintf(fp, "\n");
  }

  /* Configuration ports */
  /* output of each configuration bit */
  /* Reserved sram ports */
  if (0 < (cur_sb_info.num_reserved_conf_bits)) {
    dump_verilog_reserved_sram_ports(fp, cur_sram_orgz_info, 
                                     0, cur_sb_info.num_reserved_conf_bits - 1,
                                     VERILOG_PORT_CONKT);
    fprintf(fp, ",\n");
  }
  /* Normal sram ports */
  if (0 < (cur_sb_info.conf_bits_msb - cur_sb_info.conf_bits_lsb)) {
    dump_verilog_sram_local_ports(fp, cur_sram_orgz_info, 
                                  cur_sb_info.conf_bits_lsb, 
                                  cur_sb_info.conf_bits_msb - 1,
                                  VERILOG_PORT_CONKT);
  }

  /* Dump ports only visible during formal verification*/
  if (0 < (cur_sb_info.conf_bits_msb - 1 - cur_sb_info.conf_bits_lsb)) {
    fprintf(fp, "\n");
    fprintf(fp, "`ifdef %s\n", verilog_formal_verification_preproc_flag);
    fprintf(fp, ",\n");
    dump_verilog_formal_verification_sram_ports(fp, cur_sram_orgz_info, 
                                                cur_sb_info.conf_bits_lsb, 
                                                cur_sb_info.conf_bits_msb - 1,
                                                VERILOG_PORT_CONKT);
    fprintf(fp, "\n");
    fprintf(fp, "`endif\n");
  }
  fprintf(fp, ");\n");

  /* Comment lines */                 
  fprintf(fp, "//----- END call module Switch blocks [%d][%d] -----\n\n", x, y);

  /* Free */

  return;
}

void dump_verilog_defined_switch_boxes(t_sram_orgz_info* cur_sram_orgz_info, 
                                       FILE* fp) {
  int ix, iy;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 0; iy < (ny + 1); iy++) {
      dump_verilog_defined_one_switch_box(cur_sram_orgz_info, fp, sb_info[ix][iy]);
    }
  }

  return;
}

/* Apply a CLB to CLB direct connection to a SPICE netlist 
 */
static 
void dump_verilog_one_clb2clb_direct(FILE* fp, 
                                     int from_grid_x, int from_grid_y,
                                     int to_grid_x, int to_grid_y,
                                     t_clb_to_clb_directs* cur_direct) {
  int ipin, cur_from_clb_pin_index, cur_to_clb_pin_index;
  int cur_from_clb_pin_side, cur_to_clb_pin_side;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check bandwidth match between from_clb and to_clb pins */
  if (0 != (cur_direct->from_clb_pin_end_index - cur_direct->from_clb_pin_start_index 
     - cur_direct->to_clb_pin_end_index - cur_direct->to_clb_pin_start_index)) {
    vpr_printf(TIO_MESSAGE_ERROR, "(%s, [LINE%d]) Unmatch pin bandwidth in direct connection (name=%s)!\n",
               __FILE__, __LINE__, cur_direct->name);
    exit(1);
  }

  for (ipin = 0; ipin < cur_direct->from_clb_pin_end_index - cur_direct->from_clb_pin_start_index; ipin++) {
    /* Update pin index and get the side of the pins on grids */
    cur_from_clb_pin_index = cur_direct->from_clb_pin_start_index + ipin;
    cur_to_clb_pin_index = cur_direct->to_clb_pin_start_index + ipin;
    cur_from_clb_pin_side = get_grid_pin_side(from_grid_x, from_grid_y, cur_from_clb_pin_index); 
    cur_to_clb_pin_side = get_grid_pin_side(to_grid_x, to_grid_y, cur_to_clb_pin_index); 
    /* Call the subckt that has already been defined before */
    fprintf(fp, "%s ", cur_direct->spice_model->name);
    fprintf(fp, "%s_%d_ (", cur_direct->spice_model->prefix, cur_direct->spice_model->cnt); 
    /* Dump global ports */
    if  (0 < rec_dump_verilog_spice_model_global_ports(fp, cur_direct->spice_model, FALSE, FALSE)) {
      fprintf(fp, ",\n");
    }
    /* Input: Print the source grid pin */
    dump_verilog_toplevel_one_grid_side_pin_with_given_index(fp, OPIN,
                                                             cur_from_clb_pin_index,
                                                             cur_from_clb_pin_side,
                                                             from_grid_x, from_grid_y,
                                                             FALSE);
    fprintf(fp, ", ");
    /* Output: Print the destination grid pin */
    dump_verilog_toplevel_one_grid_side_pin_with_given_index(fp, IPIN, 
                                                             cur_to_clb_pin_index,
                                                             cur_to_clb_pin_side,
                                                             to_grid_x, from_grid_y,
                                                             FALSE);
    fprintf(fp, ");\n");
  
    /* Stats the number of spice_model used*/
    cur_direct->spice_model->cnt++; 
  }

  return;
}                 

/* Apply CLB to CLB direct connections to a Verilog netlist 
 */
void dump_verilog_clb2clb_directs(FILE* fp, 
                                  int num_directs, t_clb_to_clb_directs* direct) {
  int ix, iy, idirect;   
  int to_clb_x, to_clb_y;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  fprintf(fp, "//----- BEGIN CLB to CLB Direct Connections -----\n");   

  /* Scan the grid, visit each grid and apply direct connections */
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 0; iy < (ny + 1); iy++) {
      /* Bypass EMPTY_TYPE*/
      if ((NULL == grid[ix][iy].type)
         || (EMPTY_TYPE == grid[ix][iy].type)) {
        continue;
      }
      /* Check each clb2clb directs, 
       * see if a match to the type
       */ 
      for (idirect = 0; idirect < num_directs; idirect++) {
        /* Bypass unmatch types */
        if (grid[ix][iy].type != direct[idirect].from_clb_type) {
          continue;
        }
        /* Apply x/y_offset */ 
        to_clb_x = ix + direct[idirect].x_offset;
        to_clb_y = iy + direct[idirect].y_offset;
        /* see if the destination CLB is in the bound */
        if ((FALSE == is_grid_coordinate_in_range(0, nx, to_clb_x))
           ||(FALSE == is_grid_coordinate_in_range(0, ny, to_clb_y))) {
          continue;
        }
        /* Check if capacity (z_offset) is in the range 
        if (FALSE == is_grid_coordinate_in_range(0, grid[ix][iy].type->capacity, grid[ix][iy].type->z + direct[idirect].z_offset)) {
          continue;
        }
        */
        /* Check if the to_clb_type matches */
        if (grid[to_clb_x][to_clb_y].type != direct[idirect].to_clb_type) {
          continue;
        }
        /* Bypass x/y_offset =  1 
         * since it may be addressed in Connection blocks 
        if (1 == (x_offset + y_offset)) {
          continue;
        }
         */
        /* Now we can print a direct connection with the spice models */
        dump_verilog_one_clb2clb_direct(fp, 
                                        ix, iy, 
                                        to_clb_x, to_clb_y, 
                                        &direct[idirect]);
      }
    }
  }

  fprintf(fp, "//----- END CLB to CLB Direct Connections -----\n");   

  return;

}

/** Dump Standalone SRAMs 
 */
static 
void dump_verilog_configuration_circuits_standalone_srams(t_sram_orgz_info* cur_sram_orgz_info, 
                                                          FILE* fp) {
  int num_mem_bits = 0;

  /* Check */
  assert(SPICE_SRAM_STANDALONE == cur_sram_orgz_info->type);
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Invalid File handler!",__FILE__, __LINE__); 
    exit(1);
  } 

  /* Get the total memory bits */
  num_mem_bits = get_sram_orgz_info_num_mem_bit(cur_sram_orgz_info);

  /* Dump each SRAM */
  fprintf(fp, "//------ Standalone SRAMs -----\n");
  fprintf(fp, "%s %s_0_ (\n", 
              verilog_config_peripheral_prefix,
              verilog_config_peripheral_prefix);
  fprintf(fp, "%s_in[%d:%d],\n",
              sram_verilog_model->prefix, 
              0, num_mem_bits - 1);
  fprintf(fp, "%s_out[%d:%d],\n",
              sram_verilog_model->prefix, 
              0, num_mem_bits - 1);
  fprintf(fp, "%s_outb[%d:%d]);\n",
              sram_verilog_model->prefix, 
              0, num_mem_bits - 1);
  fprintf(fp, "//------ END Standalone SRAMs -----\n");

  return;
}

/** Dump scan-chains 
 */
static 
void dump_verilog_configuration_circuits_scan_chains(t_sram_orgz_info* cur_sram_orgz_info, 
                                                     FILE* fp) {
  int num_mem_bits = 0;

  /* Check */
  assert(SPICE_SRAM_SCAN_CHAIN == cur_sram_orgz_info->type);
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Invalid file handler!",__FILE__, __LINE__); 
    exit(1);
  } 

  /* Get the total memory bits */
  num_mem_bits = get_sram_orgz_info_num_mem_bit(cur_sram_orgz_info);

  /* Dump each Scan-chain FF */
  fprintf(fp, "//------ Configuration peripheral for Scan-chain FFs -----\n");
  fprintf(fp, "%s %s_0_ (\n", 
              verilog_config_peripheral_prefix,
              verilog_config_peripheral_prefix);
  /* Scan-chain input*/
  dump_verilog_generic_port(fp, VERILOG_PORT_CONKT,
                            top_netlist_scan_chain_head_prefix, 0, 0);
  fprintf(fp, ",\n");
  dump_verilog_sram_one_local_outport(fp, cur_sram_orgz_info, 0, num_mem_bits - 1, -1, VERILOG_PORT_CONKT);
  fprintf(fp, ",\n");
  dump_verilog_sram_one_local_outport(fp, cur_sram_orgz_info, 0, num_mem_bits - 1, 0, VERILOG_PORT_CONKT);
  fprintf(fp, ");\n");
  fprintf(fp, "//------ END Configuration peripheral Scan-chain FFs -----\n");

  return;
}

/* Dump a memory bank to configure all the Bit lines and Word lines */
static 
void dump_verilog_configuration_circuits_memory_bank(FILE* fp, 
                                                     t_sram_orgz_info* cur_sram_orgz_info) {
  int num_bl, num_wl;
  int num_reserved_bl, num_reserved_wl;
  int num_array_bl, num_array_wl;
  int bl_decoder_size, wl_decoder_size;
  t_spice_model* mem_model = NULL;
  int num_blb_ports, num_wlb_ports;
  t_spice_model_port** blb_port = NULL;
  t_spice_model_port** wlb_port = NULL;

  /* Check */
  assert(SPICE_SRAM_MEMORY_BANK == cur_sram_orgz_info->type);
  assert(NULL != cur_sram_orgz_info->mem_bank_info);

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Invalid file handler!",__FILE__, __LINE__); 
    exit(1);
  } 

  /* Get the total number of BLs and WLs */
  get_sram_orgz_info_num_blwl(cur_sram_orgz_info, &num_bl, &num_wl);
  /* Get the reserved BLs and WLs */
  get_sram_orgz_info_reserved_blwl(cur_sram_orgz_info, &num_reserved_bl, &num_reserved_wl);

  /* Get memory model */
  get_sram_orgz_info_mem_model(cur_sram_orgz_info, &mem_model);
  assert(NULL != mem_model);

  determine_blwl_decoder_size(cur_sram_orgz_info,
                              &num_array_bl, &num_array_wl, &bl_decoder_size, &wl_decoder_size);

  /* Get BLB and WLB ports */
  find_blb_wlb_ports_spice_model(mem_model, &num_blb_ports, &blb_port,
                                 &num_wlb_ports, &wlb_port);

  /* Comment lines */
  fprintf(fp, "//------ BEGIN Configuration peripheral for Memory-bank -----\n");
  fprintf(fp, "%s %s_0_ (\n", 
              verilog_config_peripheral_prefix,
              verilog_config_peripheral_prefix);
  /* Ports for memory decoders */
  dump_verilog_decoder_memory_bank_ports(cur_sram_orgz_info, fp, VERILOG_PORT_CONKT);
  fprintf(fp, ",");
  /* Ports for all the SRAM cells  */
  switch (mem_model->design_tech) {
  case SPICE_MODEL_DESIGN_CMOS:
    assert( 0 == num_reserved_bl );
    assert( 0 == num_reserved_wl );
    /* Declare normal BL / WL inputs */
    fprintf(fp, "  %s%s[%d:%d],",
            mem_model->prefix, top_netlist_normal_bl_port_postfix,
            0, num_bl - 1); 
    fprintf(fp, "  %s%s[%d:%d]",
            mem_model->prefix, top_netlist_normal_wl_port_postfix,
            0, num_wl - 1);
    /* Declare inverted wires if needed */
    if (1 == num_blb_ports) {
      fprintf(fp, ", ");
      fprintf(fp, "%s%s[%d:%d]",
              mem_model->prefix, top_netlist_normal_blb_port_postfix,
              0, num_bl - 1); 
    }
    if (1 == num_wlb_ports) {
      fprintf(fp, ", ");
      fprintf(fp, "%s%s[%d:%d]",
              mem_model->prefix, top_netlist_normal_wlb_port_postfix,
              0, num_wl - 1);
    }

    break; 
  case SPICE_MODEL_DESIGN_RRAM: 
    /* Check: there should be reserved BLs and WLs */
    assert( 0 < num_reserved_bl );
    assert( 0 < num_reserved_wl );
    /* Declare reserved and normal conf_bits ports  */
    fprintf(fp, "  %s%s[0:%d],",
            mem_model->prefix, top_netlist_reserved_bl_port_postfix,
            num_reserved_bl - 1);
    fprintf(fp, "  %s%s[0:%d],",
            mem_model->prefix, top_netlist_reserved_wl_port_postfix,
            num_reserved_wl - 1); 
    fprintf(fp, "  %s%s[%d:%d],",
            mem_model->prefix, top_netlist_normal_bl_port_postfix,
            num_reserved_bl, num_array_bl - 1); 
    fprintf(fp, "  %s%s[%d:%d]",
            mem_model->prefix, top_netlist_normal_wl_port_postfix,
            num_reserved_wl, num_array_wl - 1);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  fprintf(fp, ");\n");
  fprintf(fp, "//------ END Configuration peripheral for Memory-bank -----\n");

  return; 
}

/* Dump the configuration circuits in verilog according to user-specification
 * Supported styles of configuration circuits:
 * 1. Scan-chains
 * 2. Memory banks
 * 3. Standalone SRAMs
 */
void dump_verilog_configuration_circuits(t_sram_orgz_info* cur_sram_orgz_info, 
                                         FILE* fp) {
  switch(cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
    dump_verilog_configuration_circuits_standalone_srams(cur_sram_orgz_info, fp);
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    dump_verilog_configuration_circuits_scan_chains(cur_sram_orgz_info, fp);
    break;
  case SPICE_SRAM_MEMORY_BANK:
    dump_verilog_configuration_circuits_memory_bank(fp, cur_sram_orgz_info);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }
  return;
}


/***** Print Top-level SPICE netlist *****/
void dump_verilog_top_netlist(t_sram_orgz_info* cur_sram_orgz_info,
                              char* circuit_name,
                              char* top_netlist_name,
                              char* include_dir_path,
                              char* subckt_dir_path,
                              int LL_num_rr_nodes,
                              t_rr_node* LL_rr_node,
                              t_ivec*** LL_rr_node_indices,
                              int num_clock,
                              t_spice verilog) {
  FILE* fp = NULL;
  char* formatted_subckt_dir_path = format_dir_path(subckt_dir_path);
  char* temp_include_file_path = NULL;
  char* title = my_strcat("FPGA Verilog Netlist for Design: ", circuit_name);

  /* Check if the path exists*/
  fp = fopen(top_netlist_name,"w");
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Failure in create top Verilog netlist %s!",__FILE__, __LINE__, top_netlist_name); 
    exit(1);
  } 
  
  vpr_printf(TIO_MESSAGE_INFO, "Writing FPGA Top-level Verilog Netlist for %s...\n", circuit_name);
 
  /* Print the title */
  dump_verilog_file_header(fp, title);
  my_free(title);

  /* Include user-defined sub-circuit netlist */
  fprintf(fp, "//----- Include User-defined netlists -----\n");
  init_include_user_defined_verilog_netlists(verilog);
  dump_include_user_defined_verilog_netlists(fp, verilog);
  
  /* Special subckts for Top-level SPICE netlist */
  fprintf(fp, "//----- Include subckt netlists: Multiplexers -----\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, muxes_verilog_file_name);
  fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
  my_free(temp_include_file_path);

  fprintf(fp, "//----- Include subckt netlists: Wires -----\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, wires_verilog_file_name);
  fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
  my_free(temp_include_file_path);

  fprintf(fp, "//----- Include subckt netlists: Look-Up Tables (LUTs) -----\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, luts_verilog_file_name);
  fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
  my_free(temp_include_file_path);

  fprintf(fp, "//------ Include subckt netlists: Logic Blocks -----\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, logic_block_verilog_file_name);
  fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
  my_free(temp_include_file_path);

  fprintf(fp, "//----- Include subckt netlists: Routing structures (Switch Boxes, Channels, Connection Boxes) -----\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, routing_verilog_file_name);
  fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
  my_free(temp_include_file_path);
 
  /* Include decoders if required */ 
  switch(cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
  case SPICE_SRAM_SCAN_CHAIN:
    break;
  case SPICE_SRAM_MEMORY_BANK:
    /* Include verilog decoder */
    fprintf(fp, "//----- Include subckt netlists: Decoders (controller for memeory bank) -----\n");
    temp_include_file_path = my_strcat(formatted_subckt_dir_path, config_peripheral_verilog_file_name);
    fprintf(fp, "// `include \"%s\"\n", temp_include_file_path);
    my_free(temp_include_file_path);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization in Verilog Generator!\n",
               __FILE__, __LINE__);
    exit(1);
  }
 
  /* Print all global wires*/
  dump_verilog_top_netlist_ports(cur_sram_orgz_info, fp, num_clock, circuit_name, verilog);

  dump_verilog_top_netlist_internal_wires(cur_sram_orgz_info, fp);

  /* Quote defined Logic blocks subckts (Grids) */
  dump_verilog_defined_grids(cur_sram_orgz_info, fp);

  /* Quote Routing structures: Channels */
  dump_verilog_defined_channels(fp, LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices);

  /* Quote Routing structures: Connection Boxes */
  dump_verilog_defined_connection_boxes(cur_sram_orgz_info, fp); 
  
  /* Quote Routing structures: Switch Boxes */
  dump_verilog_defined_switch_boxes(cur_sram_orgz_info, fp); 

  /* Apply CLB to CLB direct connections */
  dump_verilog_clb2clb_directs(fp, num_clb2clb_directs, clb2clb_direct);

  /* Dump configuration circuits */
  dump_verilog_configuration_circuits(cur_sram_orgz_info, fp);

  /* verilog ends*/
  fprintf(fp, "endmodule\n");

  /* Close the file*/
  fclose(fp);

  return;
}



