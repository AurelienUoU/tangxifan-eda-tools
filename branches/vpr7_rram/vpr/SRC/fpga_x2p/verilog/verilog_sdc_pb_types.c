/***********************************/
/*  	SDC Generation dumping     */
/*       Xifan TANG, EPFL/LSI      */
/*      Baudouin Chauviere LNIS    */
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
#include "vpr_utils.h"
#include "path_delay.h"
#include "stats.h"
#include "route_common.h"

/* Include FPGA-SPICE utils */
#include "read_xml_spice_util.h"
#include "linkedlist.h"
#include "fpga_x2p_types.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_pbtypes_utils.h"
#include "fpga_x2p_backannotate_utils.h"
#include "fpga_x2p_globals.h"
#include "fpga_bitstream.h"

/* Include SynVerilog headers */
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_submodules.h"
#include "verilog_pbtypes.h"
#include "verilog_routing.h"
#include "verilog_compact_netlist.h"
#include "verilog_top_testbench.h"
#include "verilog_autocheck_top_testbench.h"
#include "verilog_verification_top_netlist.h"
#include "verilog_modelsim_autodeck.h"
#include "verilog_report_timing.h"
#include "verilog_sdc.h"
#include "verilog_formality_autodeck.h"
#include "verilog_sdc_pb_types.h"


void sdc_dump_annotation(char* from_path, // includes the cell
						char* to_path,
						FILE* fp,
						t_interconnect interconnect
						){
  char* min_value = NULL;
  char* max_value = NULL;
  int i,j;

// Find in the annotations the min and max

  for (i=0; i < interconnect.num_annotations; i++) {
    if (E_ANNOT_PIN_TO_PIN_DELAY == interconnect.annotations[i].type) {
      for (j=0; j < interconnect.annotations[i].num_value_prop_pairs; j++) {
	    if (E_ANNOT_PIN_TO_PIN_DELAY_MIN == interconnect.annotations[i].prop[j]) {
		    min_value = interconnect.annotations[i].value[j];
		  }
	    if(E_ANNOT_PIN_TO_PIN_DELAY_MAX == interconnect.annotations[i].prop[j]) {
			max_value = interconnect.annotations[i].value[j];
        }
      }
    }
  }
// Dump the annotation
// If no annotation was found, dump 0

fprintf (fp, "set_min_delay -from %s -to %s ", from_path,to_path);
  if (NULL != min_value) {
    fprintf(fp, "%s\n", min_value);
    } else {
    fprintf(fp, "0\n");
    }

fprintf (fp, "set_max_delay -from %s -to %s ", from_path, to_path);
  if (max_value != NULL){
    fprintf (fp,"%s\n",max_value);
  } else {
    fprintf (fp,"0\n");
  }

return;
}


void dump_sdc_pb_graph_pin_interc(t_sram_orgz_info* cur_sram_orgz_info,
                                      FILE* fp,
                                      enum e_spice_pin2pin_interc_type pin2pin_interc_type,
                                      t_pb_graph_pin* des_pb_graph_pin,
                                      t_mode* cur_mode) {
  int iedge, ipin;
  int fan_in = 0;
  t_interconnect* cur_interc = NULL; 
  enum e_interconnect verilog_interc_type = DIRECT_INTERC;

  t_pb_graph_pin* src_pb_graph_pin = NULL;
  t_pb_graph_node* src_pb_graph_node = NULL;
  t_pb_type* src_pb_type = NULL;

  t_pb_graph_node* des_pb_graph_node = NULL;

  char* from_path = NULL;
  char* to_path = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf (TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* 1. identify pin interconnection type, 
   * 2. Identify the number of fan-in (Consider interconnection edges of only selected mode)
   * 3. Select and print the SPICE netlist
   */
  fan_in = 0;
  cur_interc = NULL;
  find_interc_fan_in_des_pb_graph_pin (des_pb_graph_pin, cur_mode, &cur_interc, &fan_in);
  if ((NULL == cur_interc) || (0 == fan_in)) { 
    /* No interconnection matched */
    /* Connect this pin to GND for better convergence */
    /* TODO: find the correct pin name!!!*/
    /*
    dump_verilog_dangling_des_pb_graph_pin_interc(fp, des_pb_graph_pin, cur_mode, pin2pin_interc_type,
                                                  formatted_parent_pin_prefix);
    */
    return;
  }
  /* Initialize the interconnection type that will be implemented in SPICE netlist*/
  verilog_interc_type = determine_actual_pb_interc_type (cur_interc, fan_in);
  /* This time, (2nd round), we print the subckt, according to interc type*/ 
  switch (verilog_interc_type) {
  case DIRECT_INTERC:
    /* Check : 
     * 1. Direct interc has only one fan-in!
     */
    assert (1 == fan_in);
    //assert(1 == des_pb_graph_pin->num_input_edges);
    /* For more than one mode defined, the direct interc has more than one input_edge ,
     * We need to find which edge is connected the pin we want
     */
    for (iedge = 0; iedge < des_pb_graph_pin->num_input_edges; iedge++) {
      if (cur_interc == des_pb_graph_pin->input_edges[iedge]->interconnect) {
        break;
      }
    }
    assert (iedge < des_pb_graph_pin->num_input_edges);
    /* 2. spice_model is a wire */ 
    assert (NULL != cur_interc->spice_model);
    assert (SPICE_MODEL_WIRE == cur_interc->spice_model->type);
    assert (NULL != cur_interc->spice_model->wire_param);
    /* Initialize*/
    /* Source pin, node, pb_type*/
    src_pb_graph_pin = des_pb_graph_pin->input_edges[iedge]->input_pins[0];
    src_pb_graph_node = src_pb_graph_pin->parent_node;
    src_pb_type = src_pb_graph_node->pb_type;
    /* Des pin, node, pb_type */
    des_pb_graph_node  = des_pb_graph_pin->parent_node;
    
	// Generation of the paths for the dumping of the annotations
	from_path = gen_verilog_one_pb_graph_pin_full_name_in_hierarchy (src_pb_graph_pin);	
	to_path = gen_verilog_one_pb_graph_pin_full_name_in_hierarchy (des_pb_graph_pin);	

	// Dumping of the annotations	
	sdc_dump_annotation (from_path, to_path, fp, cur_interc[0]);	
  break;
  case COMPLETE_INTERC:
  case MUX_INTERC:
    /* Check : 
     * MUX should have at least 2 fan_in
     */
    assert ((2 == fan_in) || (2 < fan_in));
    /* 2. spice_model is a wire */ 
    assert (NULL != cur_interc->spice_model);
    assert (SPICE_MODEL_MUX == cur_interc->spice_model->type);
    ipin = 0;
    for (iedge = 0; iedge < des_pb_graph_pin->num_input_edges; iedge++) {
      if (cur_mode != des_pb_graph_pin->input_edges[iedge]->interconnect->parent_mode) {
        continue;
      }
      check_pb_graph_edge(*(des_pb_graph_pin->input_edges[iedge]));
      /* Initialize*/
      /* Source pin, node, pb_type*/
      src_pb_graph_pin = des_pb_graph_pin->input_edges[iedge]->input_pins[0];
      src_pb_graph_node = src_pb_graph_pin->parent_node;
      src_pb_type = src_pb_graph_node->pb_type;
      /* Des pin, node, pb_type */
      des_pb_graph_node  = des_pb_graph_pin->parent_node;
	  
	  // Generation of the paths for the dumping of the annotations
	  from_path = gen_verilog_one_pb_graph_pin_full_name_in_hierarchy(src_pb_graph_pin);	
	  to_path = gen_verilog_one_pb_graph_pin_full_name_in_hierarchy(des_pb_graph_pin);	
	
	  // Dumping of the annotations
	  sdc_dump_annotation (from_path, to_path, fp, cur_interc[0]);	
    }
	break;

  default:
    vpr_printf (TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid interconnection type for %s (Arch[LINE%d])!\n",
               __FILE__, __LINE__, cur_interc->name, cur_interc->line_num);
    exit(1);
  }

  return;
}


/* Print the SPICE interconnections of a port defined in pb_graph */
void dump_sdc_pb_graph_port_interc(t_sram_orgz_info* cur_sram_orgz_info,
                                       FILE* fp,
                                       t_pb_graph_node* cur_pb_graph_node,
                                       enum e_spice_pb_port_type pb_port_type,
                                       t_mode* cur_mode) {
  int iport, ipin;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf (TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  switch (pb_port_type) {
  case SPICE_PB_PORT_INPUT:
    for (iport = 0; iport < cur_pb_graph_node->num_input_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_graph_node->num_input_pins[iport]; ipin++) {
        /* If this is a idle block, we set 0 to the selected edge*/
        /* Get the selected edge of current pin*/
        dump_sdc_pb_graph_pin_interc (cur_sram_orgz_info,
                                         fp, 
                                         INPUT2INPUT_INTERC,
                                         &(cur_pb_graph_node->input_pins[iport][ipin]),
                                         cur_mode);
      }
    }
    break;
  case SPICE_PB_PORT_OUTPUT:
    for (iport = 0; iport < cur_pb_graph_node->num_output_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_graph_node->num_output_pins[iport]; ipin++) {
        dump_sdc_pb_graph_pin_interc(cur_sram_orgz_info,
                                         fp, 
                                         OUTPUT2OUTPUT_INTERC,
                                         &(cur_pb_graph_node->output_pins[iport][ipin]),
                                         cur_mode);
      }
    }
    break;
  case SPICE_PB_PORT_CLOCK:
    for (iport = 0; iport < cur_pb_graph_node->num_clock_ports; iport++) {
      for (ipin = 0; ipin < cur_pb_graph_node->num_clock_pins[iport]; ipin++) {
        dump_sdc_pb_graph_pin_interc(cur_sram_orgz_info,
                                         fp, 
                                         INPUT2INPUT_INTERC,
                                         &(cur_pb_graph_node->clock_pins[iport][ipin]),
                                         cur_mode);
      }
    }
    break;
  default:
   vpr_printf (TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid pb port type!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  return;
}

void sdc_dump_cur_node_constraints(t_sram_orgz_info* cur_sram_orgz_info,
							  FILE*  fp,
							  t_pb_graph_node* cur_pb_graph_node,
							  int select_mode_index) {
  int ipb, jpb;
  t_mode* cur_mode = NULL;
  t_pb_type* cur_pb_type = cur_pb_graph_node->pb_type;
  t_pb_graph_node* child_pb_graph_node = NULL;
  

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf (TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  /* Check cur_pb_type*/
  if (NULL == cur_pb_type) {
    vpr_printf (TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_type.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  
  /* Assign current mode */
  cur_mode = &(cur_pb_graph_node->pb_type->modes[select_mode_index]);

  /* We check output_pins of cur_pb_graph_node and its the input_edges
   * Built the interconnections between outputs of cur_pb_graph_node and outputs of child_pb_graph_node
   *   child_pb_graph_node.output_pins -----------------> cur_pb_graph_node.outpins
   *                                        /|\
   *                                         |
   *                         input_pins,   edges,       output_pins
   */ 
  dump_sdc_pb_graph_port_interc(cur_sram_orgz_info, fp,
                                    cur_pb_graph_node, 
                                    SPICE_PB_PORT_OUTPUT,
                                    cur_mode);
  
  /* We check input_pins of child_pb_graph_node and its the input_edges
   * Built the interconnections between inputs of cur_pb_graph_node and inputs of child_pb_graph_node
   *   cur_pb_graph_node.input_pins -----------------> child_pb_graph_node.input_pins
   *                                        /|\
   *                                         |
   *                         input_pins,   edges,       output_pins
   */ 
  for (ipb = 0; ipb < cur_pb_type->modes[select_mode_index].num_pb_type_children; ipb++) {
    for (jpb = 0; jpb < cur_pb_type->modes[select_mode_index].pb_type_children[ipb].num_pb; jpb++) {
      child_pb_graph_node = &(cur_pb_graph_node->child_pb_graph_nodes[select_mode_index][ipb][jpb]);
      /* For each child_pb_graph_node input pins*/
      dump_sdc_pb_graph_port_interc(cur_sram_orgz_info, fp,
                                         child_pb_graph_node, 
                                         SPICE_PB_PORT_INPUT,
                                         cur_mode);
      /* TODO: for clock pins, we should do the same work */
      dump_sdc_pb_graph_port_interc(cur_sram_orgz_info, fp,
                                        child_pb_graph_node, 
                                        SPICE_PB_PORT_CLOCK,
                                        cur_mode);
    }
  }
  return; 
}

void sdc_rec_dump_child_pb_graph_node(t_sram_orgz_info* cur_sram_orgz_info,
									 FILE* fp,
									 t_pb_graph_node* cur_pb_graph_node) {

  int mode_index, ipb, jpb, child_mode_index;
  t_pb_type* cur_pb_type = NULL;

  /* Check the file handler */
  if (NULL == fp) {
	vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n",__FILE__, __LINE__);
  	  exit(1);
	}
/* Check current node */
  if (NULL == cur_pb_graph_node) {
	vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid cur_pb_graph_node.\n",__FILE__, __LINE__);
	exit(1);
  }
  cur_pb_type = cur_pb_graph_node->pb_type;

/* First we only go through the graph through the physical nodes. The other modes don't have sdc constraints. We then check all the children nodes and repeat the operation until arriving to the leaf nodes. 
 * Once at the leaf, all the edges are dumped with a default set_min/max_delay of 0 if non is user-defined.
 * Contrary to the verilog, because the interconnections can get tricky through shift registers, carry-chains and all, we do not use wild cards or instantiation to be sure to completely constrain our design.
 */
/* Recursively finish all the child pb types */
  if (FALSE == is_primitive_pb_type(cur_pb_type)) {
	/* Find the mode that defines the physical mode*/
	mode_index = find_pb_type_physical_mode_index((*cur_pb_type));	
	for(ipb = 0; ipb < cur_pb_type->modes[mode_index].num_pb_type_children; ipb++) {
		for(jpb = 0; jpb < cur_pb_type->modes[mode_index].pb_type_children[ipb].num_pb; jpb++){
		/* Contrary to the verilog, we do not need to keep the prefix 
 * We go done to every child node to dump the constraints now*/ 		
		  sdc_rec_dump_child_pb_graph_node(cur_sram_orgz_info, fp, &(cur_pb_graph_node->child_pb_graph_nodes[mode_index][ipb][jpb]));
	  }
	}
    sdc_dump_cur_node_constraints(cur_sram_orgz_info, fp, cur_pb_graph_node, mode_index); // graph_head only has one pb_type
  }

return;
}

void sdc_dump_all_pb_graph_nodes(FILE* fp,
							t_sram_orgz_info* cur_sram_orgz_info,
							int type_descriptors_mode){

  // Give head of the pb_graph to the recursive function
  sdc_rec_dump_child_pb_graph_node (cur_sram_orgz_info, fp, type_descriptors[type_descriptors_mode].pb_graph_head);

return;
}

void dump_sdc_physical_blocks(t_sram_orgz_info* cur_sram_orgz_info,
							char* sdc_path,
							int type_descriptor_mode) {

	FILE* fp;

  /* Check if the path exists*/
  fp = fopen (sdc_path,"w");
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Failure in creating SDC for constraining Primitive Blocks %s!",__FILE__, __LINE__, sdc_path); 
    exit(1);
}
  
  vpr_printf (TIO_MESSAGE_INFO, "Generating SDC for constraining Primitive Blocks in P&R flow: (%s)...\n", 
             sdc_path);


  // Launch a recursive function to visit all the nodes of the correct mode
  sdc_dump_all_pb_graph_nodes(fp, cur_sram_orgz_info, type_descriptor_mode);


/* close file */ 
  fclose(fp);

return;
}


void verilog_generate_sdc_constrain_pb_types(t_sram_orgz_info* cur_sram_orgz_info,
                                             char* sdc_dir) {

  int itype;
  char* sdc_path;
  char* fpga_verilog_sdc_pb_types = "pb_types.sdc";

  sdc_path = my_strcat (sdc_dir,fpga_verilog_sdc_pb_types); // Global var

  for (itype = 0; itype < num_types; itype++){
    if (FILL_TYPE == &type_descriptors[itype]){
	  dump_sdc_physical_blocks(cur_sram_orgz_info, sdc_path, itype);
    }
  }
return;
}

