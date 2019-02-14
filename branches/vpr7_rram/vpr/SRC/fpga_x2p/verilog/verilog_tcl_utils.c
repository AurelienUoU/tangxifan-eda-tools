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
#include "route_common.h"
#include "vpr_utils.h"

/* Include SPICE support headers*/
#include "linkedlist.h"
#include "fpga_x2p_types.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_backannotate_utils.h"
#include "fpga_x2p_mux_utils.h"
#include "fpga_x2p_pbtypes_utils.h"
#include "fpga_x2p_bitstream_utils.h"
#include "fpga_x2p_rr_graph_utils.h"
#include "fpga_x2p_globals.h"

/* Include Verilog support headers*/
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_routing.h"

/***** Subroutine Functions *****/
void dump_verilog_sdc_file_header(FILE* fp,
                                  char* usage) {
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s, LINE[%d]) FileHandle is NULL!\n",__FILE__,__LINE__); 
    exit(1);
  } 
  fprintf(fp,"#############################################\n");
  fprintf(fp,"#     Synopsys Design Constraints (SDC)    # \n");
  fprintf(fp,"#    FPGA Synthesizable Verilog Netlist    # \n");
  fprintf(fp,"#    Description: %s \n",usage);
  fprintf(fp,"#           Author: Xifan TANG             # \n");
  fprintf(fp,"#        Organization: EPFL/IC/LSI         # \n");
  fprintf(fp,"#    Date: %s \n", my_gettime());
  fprintf(fp,"#############################################\n");
  fprintf(fp,"\n");

  return;
}


void dump_verilog_one_sb_chan_pin(FILE* fp, 
                                  t_sb* cur_sb_info,
                                  t_rr_node* cur_rr_node,
                                  enum PORTS port_type) {
  int track_idx, side;
  int x_start, y_start;
  t_rr_type chan_rr_type;

  /* Check the file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "(FILE:%s,LINE[%d])Invalid file handler for SDC generation",
               __FILE__, __LINE__); 
    exit(1);
  } 

  /* Check */
  assert ((CHANX == cur_rr_node->type)
        ||(CHANY == cur_rr_node->type));
  /* Get the coordinate of chanx or chany*/
  /* Find the coordinate of the cur_rr_node */  
  get_rr_node_side_and_index_in_sb_info(cur_rr_node, 
                                        *cur_sb_info,
                                        port_type, &side, &track_idx); 
  get_chan_rr_node_coorindate_in_sb_info(*cur_sb_info, side, 
                                         &(chan_rr_type),
                                         &x_start, &y_start);
  assert (chan_rr_type == cur_rr_node->type); 
  /* Print the pin of the cur_rr_node */  
  fprintf(fp, "%s",
          gen_verilog_routing_channel_one_pin_name(cur_rr_node,
                                                   x_start, y_start, track_idx, 
                                                   port_type));
  return;
}

/* Output the pin name of a routing wire in a SB */
void dump_verilog_one_sb_routing_pin(FILE* fp,
                                     t_sb* cur_sb_info,
                                     t_rr_node* cur_rr_node) {
  int side;

  /* Check the file handler */
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "(FILE:%s,LINE[%d])Invalid file handler for SDC generation",
               __FILE__, __LINE__); 
    exit(1);
  } 

  /* Get the top-level pin name and print it out */
  /* Depends on the type of node */
  switch (cur_rr_node->type) {
  case OPIN:
    /* Identify the side of OPIN on a grid */
    side = get_grid_pin_side(cur_rr_node->xlow, cur_rr_node->ylow, cur_rr_node->ptc_num);
    assert (OPEN != side);
    dump_verilog_grid_side_pin_with_given_index(fp, OPIN,
                                                cur_rr_node->ptc_num,
                                                side,
                                                cur_rr_node->xlow,
                                                cur_rr_node->ylow, 
                                                FALSE); /* Do not specify direction of port */
    break; 
  case CHANX:
  case CHANY:
    dump_verilog_one_sb_chan_pin(fp, cur_sb_info, cur_rr_node, IN_PORT); 
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid type of ending point rr_node!\n",
               __FILE__, __LINE__);
 
    exit(1);
  }

  return;
}

/** Given a starting rr_node (CHANX or CHANY) 
 *  and a ending rr_node (IPIN) 
 *  return the cb contains both (the ending CB of the routing wire)
 */
t_cb* get_chan_rr_node_ending_cb(t_rr_node* src_rr_node, 
                                t_rr_node* end_rr_node) {
  int num_ipin_sides = 2;
  int* ipin_side = (int*)my_calloc(num_ipin_sides, sizeof(int));
  int num_chan_sides = 2;
  int* chan_side = (int*)my_calloc(num_chan_sides, sizeof(int));
  int iside, next_cb_x, next_cb_y;
  int node_exist;
  t_cb* next_cb = NULL;
 
  /* Type of connection block depends on the src_rr_node */
  switch (src_rr_node->type) {
  case CHANX:
    /* the x of CB is same as end_rr_node,
     * the y of CB should be same as src_rr_node
     */
    assert (end_rr_node->xlow == end_rr_node->xhigh);
    next_cb_x = end_rr_node->xlow;
    assert (src_rr_node->ylow == src_rr_node->yhigh);
    next_cb_y = src_rr_node->ylow;
    /* Side will be either on TOP or BOTTOM */
    ipin_side[0] = TOP;
    ipin_side[1] = BOTTOM;
    chan_side[0] = RIGHT;
    chan_side[1] = LEFT;
    next_cb = &(cbx_info[next_cb_x][next_cb_y]); 
    break;
  case CHANY:
    /* the x of CB is same as src_rr_node,
     * the y of CB should be same as end_rr_node
     */
    assert (src_rr_node->xlow == src_rr_node->xhigh);
    next_cb_x = src_rr_node->xlow;
    assert (end_rr_node->ylow == end_rr_node->yhigh);
    next_cb_y = end_rr_node->ylow;
    /* Side will be either on RIGHT or LEFT */
    ipin_side[0] = LEFT;
    ipin_side[1] = RIGHT;
    chan_side[0] = BOTTOM;
    chan_side[1] = TOP;
    next_cb = &(cby_info[next_cb_x][next_cb_y]); 
    break;
  default:
   vpr_printf(TIO_MESSAGE_ERROR, 
              "(File: %s [LINE%d]) Invalid type of src_rr_node!\n",
               __FILE__, __LINE__);
 
    exit(1);
  }

  /* Double check if src_rr_node is in the IN_PORT list */
  node_exist = 0;
  for (iside = 0; iside < num_chan_sides; iside++) {
    if (OPEN != get_rr_node_index_in_cb_info( src_rr_node,
                                              *next_cb, 
                                              chan_side[iside], IN_PORT)) {   
      node_exist++;
    }
  }
  assert (0 < node_exist);

  /* Double check if end_rr_node is in the OUT_PORT list */
  node_exist = 0;
  for (iside = 0; iside < num_ipin_sides; iside++) {
    if (OPEN != get_rr_node_index_in_cb_info( end_rr_node,
                                              *next_cb, 
                                              ipin_side[iside], OUT_PORT)) {
      node_exist++;
    }
  }
  assert (0 < node_exist);

  return next_cb;
}

/** Given a starting rr_node (CHANX or CHANY) 
 *  and a ending rr_node (IPIN) 
 *  return the sb contains both (the ending CB of the routing wire)
 */
t_sb* get_chan_rr_node_ending_sb(t_rr_node* src_rr_node, 
                                 t_rr_node* end_rr_node) {
  int side;
  int x_start, y_start;
  int x_end, y_end;
  int next_sb_x, next_sb_y;
  int node_exist;
  t_sb* next_sb = NULL;

  get_chan_rr_node_start_coordinate(src_rr_node, &x_start, &y_start);
  get_chan_rr_node_start_coordinate(end_rr_node, &x_end, &y_end);

  /* Case 1:                       
   *                     end_rr_node(chany[x][y+1]) 
   *                        /|\ 
   *                         |  
   *                     ---------
   *                    |         | 
   * src_rr_node ------>| next_sb |-------> end_rr_node
   * (chanx[x][y])      |  [x][y] |        (chanx[x+1][y]
   *                     ---------
   *                         |
   *                        \|/
   *                     end_rr_node(chany[x][y])
   */
  /* Case 2                            
   *                     end_rr_node(chany[x][y+1]) 
   *                        /|\ 
   *                         |  
   *                     ---------
   *                    |         | 
   * end_rr_node <------| next_sb |<-------- src_rr_node
   * (chanx[x][y])      |  [x][y] |        (chanx[x+1][y]
   *                     ---------
   *                         |
   *                        \|/
   *                     end_rr_node(chany[x][y])
   */
  /* Case 3                            
   *                     end_rr_node(chany[x][y+1]) 
   *                        /|\ 
   *                         |  
   *                     ---------
   *                    |         | 
   * end_rr_node <------| next_sb |-------> src_rr_node
   * (chanx[x][y])      |  [x][y] |        (chanx[x+1][y]
   *                     ---------
   *                        /|\
   *                         |
   *                     src_rr_node(chany[x][y])
   */
  /* Case 4                            
   *                     src_rr_node(chany[x][y+1]) 
   *                         | 
   *                        \|/  
   *                     ---------
   *                    |         | 
   * end_rr_node <------| next_sb |--------> end_rr_node
   * (chanx[x][y])      |  [x][y] |        (chanx[x+1][y]
   *                     ---------
   *                         |
   *                        \|/
   *                     end_rr_node(chany[x][y])
   */

 
  /* Try the xlow, ylow of ending rr_node */
  switch (src_rr_node->type) {
  case CHANX:
    next_sb_x = x_end;
    next_sb_y = y_start;
    break;
  case CHANY:
    next_sb_x = x_start;
    next_sb_y = y_end;
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid type of rr_node!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  switch (src_rr_node->direction) {
  case INC_DIRECTION:
    get_chan_rr_node_end_coordinate(src_rr_node, &x_end, &y_end);
    if (next_sb_x > x_end) {
      next_sb_x = x_end;
    }
    if (next_sb_y > y_end) {
      next_sb_y = y_end;
    }
    break;
  case DEC_DIRECTION:
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR, "(File: %s [LINE%d]) Invalid type of rr_node!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Double check if src_rr_node is in the list */
  node_exist = 0;
  for (side = 0; side < 4; side++) {
    if( OPEN != get_rr_node_index_in_sb_info(src_rr_node, 
                                             sb_info[next_sb_x][next_sb_y],
                                             side, IN_PORT)) {
      node_exist++;
    }
  }
  assert (1 == node_exist);                                        

  /* Double check if end_rr_node is in the list */
  node_exist = 0;
  for (side = 0; side < 4; side++) {
    if (OPEN != get_rr_node_index_in_sb_info(end_rr_node, 
                                             sb_info[next_sb_x][next_sb_y],
                                             side, OUT_PORT)) {
      node_exist++;
    }
  }
  if (1 != node_exist) {
    assert (1 == node_exist);                                        
  }

  /* Passing the check, assign ending sb */
  next_sb = &(sb_info[next_sb_x][next_sb_y]);

  return next_sb;
}

