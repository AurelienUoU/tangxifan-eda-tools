/***********************************/
/*      SPICE Modeling for VPR     */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include <stdlib.h>
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
#include "rr_graph_util.h"
#include "rr_graph.h"
#include "rr_graph2.h"
#include "vpr_utils.h"

/* Include SPICE support headers*/
#include "linkedlist.h"
#include "fpga_spice_utils.h"
#include "fpga_spice_backannotate_utils.h"
#include "fpga_spice_globals.h"
#include "fpga_spice_mux_utils.h"
#include "fpga_spice_bitstream_utils.h"

/* Include Verilog support headers*/


static 
void fpga_spice_generate_bitstream_routing_chan_subckt(int x,  int y,
                                                       t_rr_type chan_type, 
                                                       t_sram_orgz_info* cur_sram_orgz_info,
                                                       int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                                       t_ivec*** LL_rr_node_indices,
                                                       int num_segment, t_segment_inf* segments) {
  return;
}

/* Print a short interconneciton in switch box
 * There are two cases should be noticed.
 * 1. The actual fan-in of cur_rr_node is 0. In this case,
      the cur_rr_node need to be short connected to itself which is on the opposite side of this switch
 * 2. The actual fan-in of cur_rr_node is 0. In this case,
 *    The cur_rr_node need to connected to the drive_rr_node
 */
static 
void fpga_spice_generate_bitstream_switch_box_short_interc(t_sb* cur_sb_info,
                                                           t_sram_orgz_info* cur_sram_orgz_info,
                                                           int chan_side,
                                                           t_rr_node* cur_rr_node,
                                                           int actual_fan_in,
                                                           t_rr_node* drive_rr_node) {

  return;
}

/* Print the SPICE netlist of multiplexer that drive this rr_node */
static 
void fpga_spice_generate_bitstream_switch_box_mux(FILE* fp,
                                                  t_sb* cur_sb_info, 
                                                  t_sram_orgz_info* cur_sram_orgz_info,
                                                  int chan_side,
                                                  t_rr_node* cur_rr_node,
                                                  int mux_size,
                                                  t_rr_node** drive_rr_nodes,
                                                  int switch_index) {
  int inode, ilevel;
  t_spice_model* verilog_model = NULL;
  int mux_level, path_id;
  int num_mux_sram_bits = 0;
  int* mux_sram_bits = NULL;

  /* Check */
  assert((!(0 > cur_sb_info->x))&&(!(cur_sb_info->x > (nx + 1)))); 
  assert((!(0 > cur_sb_info->y))&&(!(cur_sb_info->y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check current rr_node is CHANX or CHANY*/
  assert((CHANX == cur_rr_node->type)||(CHANY == cur_rr_node->type));
  
  /* Allocate drive_rr_nodes according to the fan-in*/
  assert((2 == mux_size)||(2 < mux_size));

  /* Get verilog model*/
  verilog_model = switch_inf[switch_index].spice_model;

  /* Configuration bits for this MUX*/
  path_id = -1;
  for (inode = 0; inode < mux_size; inode++) {
    if (drive_rr_nodes[inode] == &(rr_node[cur_rr_node->prev_node])) {
      path_id = inode;
      break;
    }
  }

  assert((-1 != path_id)&&(path_id < mux_size));

  /* Depend on both technology and structure of this MUX*/
  switch (verilog_model->design_tech) {
  case SPICE_MODEL_DESIGN_CMOS:
    decode_cmos_mux_sram_bits(verilog_model, mux_size, path_id, &num_mux_sram_bits, &mux_sram_bits, &mux_level);
    break;
  case SPICE_MODEL_DESIGN_RRAM:
    decode_rram_mux(verilog_model, mux_size, path_id, &num_mux_sram_bits, &mux_sram_bits, &mux_level);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid design technology for verilog model (%s)!\n",
               __FILE__, __LINE__, verilog_model->name);
    exit(1);
  }

  /* Print the encoding in SPICE netlist for debugging */
  fprintf(fp, "***** Switch Block [%d][%d] *****\n", 
          cur_sb_info->x, cur_sb_info->y);
  fprintf(fp, "***** SRAM bits for MUX[%d], level=%d, select_path_id=%d. *****\n", 
          verilog_model->cnt, mux_level, path_id);
  fprintf(fp, "*****");
  for (ilevel = 0; ilevel < num_mux_sram_bits; ilevel++) {
    fprintf(fp, "%d", mux_sram_bits[ilevel]);
  }
  fprintf(fp, "*****\n\n");
  
  /* Store the configuraion bit to linked-list */
  add_mux_conf_bits_to_llist(mux_size, cur_sram_orgz_info, 
                             num_mux_sram_bits, mux_sram_bits,
                             verilog_model);

  /* Synchronize the sram_orgz_info with mem_bits */
  add_mux_conf_bits_to_sram_orgz_info(cur_sram_orgz_info, verilog_model, mux_size); 
  
  /* update sram counter */
  verilog_model->cnt++;

  /* Free */
  my_free(mux_sram_bits);

  return;
}

static 
void fpga_spice_generate_bitstream_switch_box_interc(FILE* fp,
                                                     t_sb* cur_sb_info,
                                                     t_sram_orgz_info* cur_sram_orgz_info,
                                                     int chan_side,
                                                     t_rr_node* cur_rr_node) {
  int sb_x, sb_y;
  int num_drive_rr_nodes = 0;  
  t_rr_node** drive_rr_nodes = NULL;

  sb_x = cur_sb_info->x;
  sb_y = cur_sb_info->y;

  /* Check */
  assert((!(0 > sb_x))&&(!(sb_x > (nx + 1)))); 
  assert((!(0 > sb_y))&&(!(sb_y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Determine if the interc lies inside a channel wire, that is interc between segments */
  /* Check each num_drive_rr_nodes, see if they appear in the cur_sb_info */
  if (TRUE == check_drive_rr_node_imply_short(*cur_sb_info, cur_rr_node, chan_side)) {
    /* Double check if the interc lies inside a channel wire, that is interc between segments */
    assert(1 == is_rr_node_exist_opposite_side_in_sb_info(*cur_sb_info, cur_rr_node, chan_side));
    num_drive_rr_nodes = 0;
    drive_rr_nodes = NULL;
  } else {
    num_drive_rr_nodes = cur_rr_node->num_drive_rr_nodes;
    drive_rr_nodes = cur_rr_node->drive_rr_nodes;
  }

  if (0 == num_drive_rr_nodes) {
    /* Print a special direct connection*/
    fpga_spice_generate_bitstream_switch_box_short_interc(cur_sb_info, cur_sram_orgz_info,
                                                          chan_side, cur_rr_node, 
                                                          num_drive_rr_nodes, cur_rr_node);
  } else if (1 == num_drive_rr_nodes) {
    /* Print a direct connection*/
    fpga_spice_generate_bitstream_switch_box_short_interc(cur_sb_info, cur_sram_orgz_info,
                                                          chan_side, cur_rr_node, 
                                                          num_drive_rr_nodes, drive_rr_nodes[0]);
  } else if (1 < num_drive_rr_nodes) {
    /* Print the multiplexer, fan_in >= 2 */
    fpga_spice_generate_bitstream_switch_box_mux(fp, cur_sb_info, cur_sram_orgz_info,
                                                 chan_side, cur_rr_node, 
                                                 num_drive_rr_nodes, drive_rr_nodes, 
                                                 cur_rr_node->drive_switches[0]);
  } /*Nothing should be done else*/ 

  /* Free */

  return;
}

/* Task: Print the subckt of a Switch Box.
 * A Switch Box subckt consists of following ports:
 * 1. Channel Y [x][y] inputs 
 * 2. Channel X [x+1][y] inputs
 * 3. Channel Y [x][y-1] outputs
 * 4. Channel X [x][y] outputs
 * 5. Grid[x][y+1] Right side outputs pins
 * 6. Grid[x+1][y+1] Left side output pins
 * 7. Grid[x+1][y+1] Bottom side output pins
 * 8. Grid[x+1][y] Top side output pins
 * 9. Grid[x+1][y] Left side output pins
 * 10. Grid[x][y] Right side output pins
 * 11. Grid[x][y] Top side output pins
 * 12. Grid[x][y+1] Bottom side output pins
 *
 *    --------------          --------------
 *    |            |          |            |
 *    |    Grid    |  ChanY   |    Grid    |
 *    |  [x][y+1]  | [x][y+1] | [x+1][y+1] |
 *    |            |          |            |
 *    --------------          --------------
 *                  ----------
 *       ChanX      | Switch |     ChanX 
 *       [x][y]     |   Box  |    [x+1][y]
 *                  | [x][y] |
 *                  ----------
 *    --------------          --------------
 *    |            |          |            |
 *    |    Grid    |  ChanY   |    Grid    |
 *    |   [x][y]   |  [x][y]  |  [x+1][y]  |
 *    |            |          |            |
 *    --------------          --------------
 */
static 
void fpga_spice_generate_bitstream_routing_switch_box_subckt(FILE* fp, 
                                                             t_sb* cur_sb_info, 
                                                             t_sram_orgz_info* cur_sram_orgz_info,
                                                             int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                                             t_ivec*** LL_rr_node_indices) {
  int itrack, side;

  /* Check */
  assert((!(0 > cur_sb_info->x))&&(!(cur_sb_info->x > (nx + 1)))); 
  assert((!(0 > cur_sb_info->y))&&(!(cur_sb_info->y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Put down all the multiplexers */
  for (side = 0; side < cur_sb_info->num_sides; side++) {
    for (itrack = 0; itrack < cur_sb_info->chan_width[side]; itrack++) {
      assert((CHANX == cur_sb_info->chan_rr_node[side][itrack]->type)
           ||(CHANY == cur_sb_info->chan_rr_node[side][itrack]->type));
      /* We care INC_DIRECTION tracks at this side*/
      if (OUT_PORT == cur_sb_info->chan_rr_node_direction[side][itrack]) {
        fpga_spice_generate_bitstream_switch_box_interc(fp, cur_sb_info, cur_sram_orgz_info, 
                                                        side, cur_sb_info->chan_rr_node[side][itrack]);
      } 
    }
  }

  /* Check */

  /* Free chan_rr_nodes */

  return;
}

/* SRC rr_node is the IPIN of a grid.*/
static 
void fpga_spice_generate_bitstream_connection_box_short_interc(t_cb* cur_cb_info,
                                                               t_sram_orgz_info* cur_sram_orgz_info,
                                                               t_rr_node* src_rr_node) {
  return;
}

static 
void fpga_spice_generate_bitstream_connection_box_mux(FILE* fp,
                                                      t_cb* cur_cb_info,
                                                      t_sram_orgz_info* cur_sram_orgz_info,
                                                      t_rr_node* src_rr_node) {
  int mux_size = 0;
  t_rr_node** drive_rr_nodes = NULL;
  int inode, mux_level, path_id, switch_index;
  t_spice_model* verilog_model = NULL;
  int num_mux_sram_bits = 0;
  int* mux_sram_bits = NULL;
  int ilevel;

  /* Check */
  assert((!(0 > cur_cb_info->x))&&(!(cur_cb_info->x > (nx + 1)))); 
  assert((!(0 > cur_cb_info->y))&&(!(cur_cb_info->y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Find drive_rr_nodes*/
  mux_size = src_rr_node->num_drive_rr_nodes;
  drive_rr_nodes = src_rr_node->drive_rr_nodes; 

  /* Configuration bits for MUX*/
  path_id = -1;
  for (inode = 0; inode < mux_size; inode++) {
    if (drive_rr_nodes[inode] == &(rr_node[src_rr_node->prev_node])) {
      path_id = inode;
      break;
    }
  }
  assert((-1 != path_id)&&(path_id < mux_size));

  switch_index = src_rr_node->drive_switches[path_id];

  verilog_model = switch_inf[switch_index].spice_model;

  switch (verilog_model->design_tech) {
  case SPICE_MODEL_DESIGN_CMOS:
    decode_cmos_mux_sram_bits(verilog_model, mux_size, path_id, &num_mux_sram_bits, &mux_sram_bits, &mux_level);
    break;
  case SPICE_MODEL_DESIGN_RRAM:
    decode_rram_mux(verilog_model, mux_size, path_id, &num_mux_sram_bits, &mux_sram_bits, &mux_level);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid design technology for verilog model (%s)!\n",
               __FILE__, __LINE__, verilog_model->name);
  }

  /* Print the encoding in SPICE netlist for debugging */
  switch(cur_cb_info->type) {
  case CHANX:
    fprintf(fp, "***** Connection Block X-channel [%d][%d] *****\n", 
            cur_cb_info->x, cur_cb_info->y);
    break;
  case CHANY:
    fprintf(fp, "***** Connection Block Y-channel [%d][%d] *****\n", 
            cur_cb_info->x, cur_cb_info->y);
    break;
  default: 
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid type of channel!\n", __FILE__, __LINE__);
    exit(1);
  }
 
  fprintf(fp, "***** SRAM bits for MUX[%d], level=%d, select_path_id=%d. *****\n", 
          verilog_model->cnt, mux_level, path_id);
  fprintf(fp, "*****");
  for (ilevel = 0; ilevel < num_mux_sram_bits; ilevel++) {
    fprintf(fp, "%d", mux_sram_bits[ilevel]);
  }
  fprintf(fp, "*****\n\n");

  /* Store the configuraion bit to linked-list */
  add_mux_conf_bits_to_llist(mux_size, cur_sram_orgz_info, 
                             num_mux_sram_bits, mux_sram_bits,
                             verilog_model);
  /* Synchronize the sram_orgz_info with mem_bits */
  add_mux_conf_bits_to_sram_orgz_info(cur_sram_orgz_info, verilog_model, mux_size); 

  /* update sram counter */
  verilog_model->cnt++;

  /* Free */
  my_free(mux_sram_bits);

  return;
}

static 
void fpga_spice_generate_bitstream_connection_box_interc(FILE* fp,
                                                         t_cb* cur_cb_info,
                                                         t_sram_orgz_info* cur_sram_orgz_info,
                                                         t_rr_node* src_rr_node) {
  /* Check */
  assert((!(0 > cur_cb_info->x))&&(!(cur_cb_info->x > (nx + 1)))); 
  assert((!(0 > cur_cb_info->y))&&(!(cur_cb_info->y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  if (1 == src_rr_node->fan_in) {
    /* Print a direct connection*/
    fpga_spice_generate_bitstream_connection_box_short_interc(cur_cb_info, cur_sram_orgz_info,
                                                              src_rr_node);
  } else if (1 < src_rr_node->fan_in) {
    /* Print the multiplexer, fan_in >= 2 */
    fpga_spice_generate_bitstream_connection_box_mux(fp, cur_cb_info, cur_sram_orgz_info, 
                                                     src_rr_node);
  } /*Nothing should be done else*/ 
   
  return;
}


/* Print connection boxes
 * Print the sub-circuit of a connection Box (Type: [CHANX|CHANY])
 * Actually it is very similiar to switch box but
 * the difference is connection boxes connect Grid INPUT Pins to channels
 * TODO: merge direct connections into CB 
 *    --------------             --------------
 *    |            |             |            |
 *    |    Grid    |   ChanY     |    Grid    |
 *    |  [x][y+1]  |   [x][y]    | [x+1][y+1] |
 *    |            | Connection  |            |
 *    -------------- Box_Y[x][y] --------------
 *                   ----------
 *       ChanX       | Switch |        ChanX 
 *       [x][y]      |   Box  |       [x+1][y]
 *     Connection    | [x][y] |      Connection 
 *    Box_X[x][y]    ----------     Box_X[x+1][y]
 *    --------------             --------------
 *    |            |             |            |
 *    |    Grid    |  ChanY      |    Grid    |
 *    |   [x][y]   | [x][y-1]    |  [x+1][y]  |
 *    |            | Connection  |            |
 *    --------------Box_Y[x][y-1]--------------
 */
static 
void fpga_spice_generate_bitstream_routing_connection_box_subckt(FILE* fp,
                                                                 t_cb* cur_cb_info,
                                                                 t_sram_orgz_info* cur_sram_orgz_info,
                                                                 int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                                                 t_ivec*** LL_rr_node_indices) {
  int inode, side; 
  int side_cnt = 0;
   
  /* Check */
  assert((!(0 > cur_cb_info->x))&&(!(cur_cb_info->x > (nx + 1)))); 
  assert((!(0 > cur_cb_info->y))&&(!(cur_cb_info->y > (ny + 1)))); 

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Print multiplexers or direct interconnect*/
  side_cnt = 0;
  for (side = 0; side < cur_cb_info->num_sides; side++) {
    /* Bypass side with zero IPINs*/
    if (0 == cur_cb_info->num_ipin_rr_nodes[side]) {
      continue;
    }
    side_cnt++;
    assert(0 < cur_cb_info->num_ipin_rr_nodes[side]);
    assert(NULL != cur_cb_info->ipin_rr_node[side]);
    for (inode = 0; inode < cur_cb_info->num_ipin_rr_nodes[side]; inode++) { 
      fpga_spice_generate_bitstream_connection_box_interc(fp, cur_cb_info, cur_sram_orgz_info,
                                                          cur_cb_info->ipin_rr_node[side][inode]);
    }
  }

  /* Check */

  /* Free */
 
  return;
}

/* Top Function*/
/* Build the routing resource SPICE sub-circuits*/
void fpga_spice_generate_bitstream_routing_resources(char* routing_bitstream_log_file_path,
                                                     t_arch arch,
                                                     t_det_routing_arch* routing_arch,
                                                     t_sram_orgz_info* cur_sram_orgz_info,
                                                     int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                                     t_ivec*** LL_rr_node_indices) {
  int ix, iy; 
  FILE* fp = NULL;
 
  assert(UNI_DIRECTIONAL == routing_arch->directionality);

  /* Create a file handler */
  fp = fopen(routing_bitstream_log_file_path, "w");

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "(FILE:%s,LINE[%d])Failure in creating log file %s",
               __FILE__, __LINE__, routing_bitstream_log_file_path); 
    exit(1);
  } 
  
  /* Two major tasks: 
   * 1. Generate sub-circuits for Routing Channels 
   * 2. Generate sub-circuits for Switch Boxes
   */ 
  /* Now: First task: Routing channels
   * Sub-circuits are named as chanx[ix][iy] or chany[ix][iy] for horizontal or vertical channels
   * each channels consist of a number of routing tracks. (Actually they are metal wires)
   * We only support single-driver routing architecture. 
   * The direction is defined as INC_DIRECTION ------> and DEC_DIRECTION <-------- for chanx
   * The direction is defined as INC_DIRECTION /|\ and DEC_DIRECTION | for chany
   *                                            |                    |
   *                                            |                    |
   *                                            |                   \|/
   * For INC_DIRECTION chanx, the inputs are at the left of channels, the outputs are at the right of channels
   * For DEC_DIRECTION chanx, the inputs are at the right of channels, the outputs are at the left of channels
   * For INC_DIRECTION chany, the inputs are at the bottom of channels, the outputs are at the top of channels
   * For DEC_DIRECTION chany, the inputs are at the top of channels, the outputs are at the bottom of channels
   */
  /* X - channels [1...nx][0..ny]*/
  vpr_printf(TIO_MESSAGE_INFO,"Generating bitstream for Routing Channels - X direction...\n");
  for (iy = 0; iy < (ny + 1); iy++) {
    for (ix = 1; ix < (nx + 1); ix++) {
      fpga_spice_generate_bitstream_routing_chan_subckt(ix, iy, CHANX, cur_sram_orgz_info, 
                                                        LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices, 
                                                        arch.num_segments, arch.Segments);
    }
  }
  /* Y - channels [1...ny][0..nx]*/
  vpr_printf(TIO_MESSAGE_INFO,"Generating bitstream for Routing Channels - Y direction...\n");
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      fpga_spice_generate_bitstream_routing_chan_subckt(ix, iy, CHANY, cur_sram_orgz_info, 
                                                        LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices, 
                                                        arch.num_segments, arch.Segments);
    }
  }

  /* Switch Boxes*/
  vpr_printf(TIO_MESSAGE_INFO,"Generating bitstream for Switch blocks...\n");
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 0; iy < (ny + 1); iy++) {
      /* vpr_printf(TIO_MESSAGE_INFO, "Writing Switch Boxes[%d][%d]...\n", ix, iy); */
      update_spice_models_routing_index_low(ix, iy, SOURCE, arch.spice->num_spice_model, arch.spice->spice_models);
      fpga_spice_generate_bitstream_routing_switch_box_subckt(fp, 
                                                              &(sb_info[ix][iy]), cur_sram_orgz_info, 
                                                              LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices);
      update_spice_models_routing_index_high(ix, iy, SOURCE, arch.spice->num_spice_model, arch.spice->spice_models);
    }
  }

  /* Connection Boxes */
  vpr_printf(TIO_MESSAGE_INFO,"Generating bitstream for Connection blocks - X direction ...\n");
  /* X - channels [1...nx][0..ny]*/
  for (iy = 0; iy < (ny + 1); iy++) {
    for (ix = 1; ix < (nx + 1); ix++) {
      /* vpr_printf(TIO_MESSAGE_INFO, "Writing X-direction Connection Boxes[%d][%d]...\n", ix, iy); */
      update_spice_models_routing_index_low(ix, iy, CHANX, arch.spice->num_spice_model, arch.spice->spice_models);
      if ((TRUE == is_cb_exist(CHANX, ix, iy))
         &&(0 < count_cb_info_num_ipin_rr_nodes(cbx_info[ix][iy]))) {
        fpga_spice_generate_bitstream_routing_connection_box_subckt(fp, 
                                                                    &(cbx_info[ix][iy]), cur_sram_orgz_info, 
                                                                    LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices); 
      }
      update_spice_models_routing_index_high(ix, iy, CHANX, arch.spice->num_spice_model, arch.spice->spice_models);
    }
  }
  /* Y - channels [1...ny][0..nx]*/
  vpr_printf(TIO_MESSAGE_INFO,"Generating bitstream for Connection blocks - Y direction ...\n");
  for (ix = 0; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      /* vpr_printf(TIO_MESSAGE_INFO, "Writing Y-direction Connection Boxes[%d][%d]...\n", ix, iy); */
      update_spice_models_routing_index_low(ix, iy, CHANY, arch.spice->num_spice_model, arch.spice->spice_models);
      if ((TRUE == is_cb_exist(CHANY, ix, iy)) 
         &&(0 < count_cb_info_num_ipin_rr_nodes(cby_info[ix][iy]))) {
        fpga_spice_generate_bitstream_routing_connection_box_subckt(fp,
                                                                    &(cby_info[ix][iy]), cur_sram_orgz_info, 
                                                                    LL_num_rr_nodes, LL_rr_node, LL_rr_node_indices); 
      }
      update_spice_models_routing_index_high(ix, iy, CHANY, arch.spice->num_spice_model, arch.spice->spice_models);
    }
  }

  /* Close log file */
  fclose(fp);
  
  return;
}
