/***********************************/
/*      SPICE Modeling for VPR     */
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
#include "rr_graph_util.h"
#include "rr_graph.h"
#include "rr_graph2.h"
#include "vpr_utils.h"

/* Include spice support headers*/
#include "linkedlist.h"
#include "spice_globals.h"
#include "spice_utils.h"
#include "spice_mux.h"
#include "spice_pbtypes.h"
#include "spice_subckt.h"
#include "spice_netlist_utils.h"
#include "spice_grid_testbench.h"

/* Global variable inside this C-source file*/
/*
static int num_inv_load = 0;
static int num_noninv_load = 0;
static int num_grid_load = 0;
*/

/* Local subroutines only accessible in this C-source file */
static 
void fprint_grid_testbench_global_ports(FILE* fp, 
                                        int num_clock, 
                                        t_spice spice);

/* Subroutines in this source file*/
static 
void fprint_grid_testbench_global_ports(FILE* fp, 
                                        int num_clock, 
                                        t_spice spice) {
  /* A valid file handler*/
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Invalid File Handler!\n",__FILE__, __LINE__); 
    exit(1);
  } 
  /* Global nodes: Vdd for SRAMs, Logic Blocks(Include IO), Switch Boxes, Connection Boxes */
  fprintf(fp, ".global gvdd gset greset\n");
  fprintf(fp, ".global gvdd_local_interc gvdd_hardlogic\n");
  fprintf(fp, ".global gvdd_sram_local_routing gvdd_sram_luts\n");
  fprintf(fp, ".global %s->in\n", sram_spice_model->prefix);
  fprintf(fp, ".global gvdd_load\n");
  fprintf(fp, "***** Global Clock Signals *****\n");
  fprintf(fp, ".global gclock\n");

  /*Global Vdds for LUTs*/
  fprint_global_vdds_spice_model(fp, SPICE_MODEL_LUT, spice);
  /*Global Vdds for FFs*/
  fprint_global_vdds_spice_model(fp, SPICE_MODEL_FF, spice);

  return;
}

void fprint_call_defined_core_grids(FILE* fp) {
  int ix, iy;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Normal Grids */
  for (ix = 1; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      assert(IO_TYPE != grid[ix][iy].type);
      fprintf(fp, "Xgrid[%d][%d] ", ix, iy);
      fprint_grid_pins(fp, ix, iy, 1);
      fprintf(fp, "gvdd 0 grid[%d][%d]\n", ix, iy); /* Call the name of subckt */ 
    }
  } 

  return;
}

void fprint_grid_testbench_one_grid_pin_stimulation(FILE* fp, int x, int y, 
                                                    int height, int side, 
                                                    int ipin,
                                                    t_ivec*** LL_rr_node_indices) {
  int ipin_rr_node_index;
  float ipin_density, ipin_probability;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 

  /* Print a voltage source according to density and probability */
  ipin_rr_node_index = get_rr_node_index(x, y, IPIN, ipin, LL_rr_node_indices);
  /* Get density and probability */
  ipin_density = get_rr_node_net_density(rr_node[ipin_rr_node_index]); 
  ipin_probability = get_rr_node_net_probability(rr_node[ipin_rr_node_index]); 
  /* Print voltage source */
  fprintf(fp, "Vgrid[%d][%d]_pin[%d][%d][%d] grid[%d][%d]_pin[%d][%d][%d] 0 \n",
          x, y, height, side, ipin, x, y, height, side, ipin);
  fprint_voltage_pulse_params(fp, 0, ipin_density, ipin_probability);

  return;
}

void fprint_grid_testbench_one_grid_pin_loads(FILE* fp, int x, int y, 
                                              int height, int side, 
                                              int ipin,
                                              t_ivec*** LL_rr_node_indices) {

  int ipin_rr_node_index;
  int iedge, iswitch, inode;
  char* prefix = NULL;
  t_spice_model* switch_spice_model = NULL;
  int inv_cnt = 0;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 

  /* Print a voltage source according to density and probability */
  ipin_rr_node_index = get_rr_node_index(x, y, OPIN, ipin, LL_rr_node_indices);
  /* Generate prefix */
  prefix = (char*)my_malloc(sizeof(char)*(5 + strlen(my_itoa(x))
             + 2 + strlen(my_itoa(y)) + 6 + strlen(my_itoa(height))
             + 2 + strlen(my_itoa(side)) + 2 + strlen(my_itoa(ipin))
             + 2 + 1));
  sprintf(prefix, "grid[%d][%d]_pin[%d][%d][%d]",
          x, y, height, side, ipin);

  /* Print all the inverter load now*/
  for (iedge = 0; iedge < rr_node[ipin_rr_node_index].num_edges; iedge++) {
    /* Get the switch spice model */
    inode = rr_node[ipin_rr_node_index].edges[iedge];
    iswitch = rr_node[ipin_rr_node_index].switches[iedge]; 
    switch_spice_model = switch_inf[iswitch].spice_model;
    if (NULL == switch_spice_model) {
      continue;
    }
    /* Add inv/buf here */
    fprintf(fp, "X%s_inv[%d] %s %s_out[%d] gvdd_load 0 inv size=%g\n",
            prefix, iedge, prefix, prefix, iedge, switch_spice_model->input_buffer->size);
    inv_cnt++;
  }
 
  /* TODO: Generate loads recursively */
  /*fprint_rr_node_loads_rec(fp, rr_node[ipin_rr_node_index],prefix);*/

  /*Free */
  my_free(prefix);

  return;
}

void fprint_grid_testbench_one_grid_stimulation(FILE* fp, 
                                                t_spice spice,
                                                t_ivec*** LL_rr_node_indices,
                                                int x, int y) {
  int ipin, class_id, side, iheight;
  t_type_ptr type = NULL;

  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Check */
  assert((!(0 > x))&&(!(x > (nx + 1)))); 
  assert((!(0 > y))&&(!(y > (ny + 1)))); 
  type = grid[x][y].type;
  assert(NULL != type);
   
  /* For each input pin, we give a stimulate*/ 
  for (side = 0; side < 4; side++) {
    for (iheight = 0; iheight < type->height; iheight++) {
      for (ipin = 0; ipin < type->num_pins; ipin++) {
        if (1 == type->pinloc[iheight][side][ipin]) {
          class_id = type->pin_class[ipin];
          if (RECEIVER == type->class_inf[class_id].type) { 
            fprint_grid_testbench_one_grid_pin_stimulation(fp, x, y, iheight, side, ipin, LL_rr_node_indices);
          } else if (DRIVER == type->class_inf[class_id].type) { 
            fprint_grid_testbench_one_grid_pin_loads(fp, x, y, iheight, side, ipin, LL_rr_node_indices);
          } else {
            fprint_stimulate_dangling_one_grid_pin(fp, x, y, iheight, side, ipin, LL_rr_node_indices);
          }
        }
      }
    }
  }

  return;
}

static 
void fprint_grid_testbench_stimulations(FILE* fp, 
                                        int num_clock,
                                        t_spice spice,
                                        t_ivec*** LL_rr_node_indices) {
  int ix, iy;

  /* Global GND */
  fprintf(fp, "***** Global VDD port *****\n");
  fprintf(fp, "Vgvdd gvdd 0 vsp\n");
  fprintf(fp, "***** Global GND port *****\n");
  fprintf(fp, "*Rggnd ggnd 0 0\n");

  /* Global set and reset */
  fprintf(fp, "***** Global Net for reset signal *****\n");
  fprintf(fp, "Vgvreset greset 0 0\n");
  fprintf(fp, "***** Global Net for set signal *****\n");
  fprintf(fp, "Vgvset gset 0 0\n");
  /* Global vdd load */
  fprintf(fp, "***** Global Net for load vdd *****\n");
  fprintf(fp, "Vgvdd_load gvdd_load 0 vsp\n");

  /* Global Vdd ports */
  fprintf(fp, "***** Global VDD for Local Interconnection *****\n");
  fprintf(fp, "Vgvdd_local_interc gvdd_local_interc 0 vsp\n");
  fprintf(fp, "***** Global VDD for local routing SRAMs *****\n");
  fprintf(fp, "Vgvdd_sram_local_routing gvdd_sram_local_routing 0 vsp\n");
  fprintf(fp, "***** Global VDD for LUTs SRAMs *****\n");
  fprintf(fp, "Vgvdd_sram_luts gvdd_sram_luts 0 vsp\n");

  /* Every Hardlogic use an independent Voltage source */
  fprintf(fp, "***** Global VDD for Hard Logics *****\n");
  fprint_splited_vdds_spice_model(fp, SPICE_MODEL_HARDLOGIC, spice);

  /* Every LUT use an independent Voltage source */
  fprintf(fp, "***** Global VDD for Look-Up Tables (LUTs) *****\n");
  fprint_splited_vdds_spice_model(fp, SPICE_MODEL_LUT, spice);

  /* Every FF use an independent Voltage source */
  fprintf(fp, "***** Global VDD for Flip-flops (FFs) *****\n");
  fprint_splited_vdds_spice_model(fp, SPICE_MODEL_FF, spice);

  /* Every SRAM inputs should have a voltage source */
  fprintf(fp, "***** Global Inputs for SRAMs *****\n");
  /*
  for (i = 0; i < sram_spice_model->cnt; i++) {
    fprintf(fp, "V%s[%d]->in %s[%d]->in 0 0\n", 
            sram_spice_model->prefix, i, sram_spice_model->prefix, i);
  }
  */
  fprintf(fp, "V%s->in %s->in 0 0\n", 
          sram_spice_model->prefix, sram_spice_model->prefix);
  fprintf(fp, ".nodeset V(%s->in) 0\n", sram_spice_model->prefix);

  fprintf(fp, "***** Global Clock signal *****\n");
  if (0 < num_clock) {
    /* First cycle reserved for measuring leakage */
    fprintf(fp, "***** pulse(vlow vhigh tdelay trise tfall pulse_width period *****\n");
    fprintf(fp, "Vgclock gclock 0 pulse(0 vsp 'clock_period'\n");
    fprintf(fp, "+                      'clock_slew_pct_rise*clock_period' 'clock_slew_pct_fall*clock_period'\n");
    fprintf(fp, "+                      '0.5*(1-clock_slew_pct_rise-clock_slew_pct_fall)*clock_period' 'clock_period')\n");
  } else {
    assert(0 == num_clock);
    fprintf(fp, "***** clock off *****\n");
    fprintf(fp, "Vgclock gclock 0 0\n");
  }

  /* For each grid input port, we generate the voltage pulses  */
  for (ix = 1; ix < (nx + 1); ix++) {
    for (iy = 1; iy < (ny + 1); iy++) {
      assert(IO_TYPE != grid[ix][iy].type);
      fprint_grid_testbench_one_grid_stimulation(fp, spice, LL_rr_node_indices,
                                                 ix, iy);
    }
  }

  return;
}

static 
void fprint_grid_testbench_measurements(FILE* fp, 
                                      t_spice spice,
                                      boolean leakage_only) {
  /* First cycle reserved for measuring leakage */
  int num_clock_cycle = spice.spice_params.meas_params.sim_num_clock_cycle + 1;
  
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
  
  fprint_spice_netlist_transient_setting(fp, spice, leakage_only);

  /* TODO: Measure the delay of each mapped net and logical block */

  /* Measure the power */
  /* Leakage ( the first cycle is reserved for leakage measurement) */
  if (TRUE == leakage_only) {
    /* Leakage power of SRAMs */
    fprintf(fp, ".measure tran leakage_power_sram_local_routing find p(Vgvdd_sram_local_routing) at=0\n");
    fprintf(fp, ".measure tran leakage_power_sram_luts find p(Vgvdd_sram_luts) at=0\n");
    /* Global power of Local Interconnections*/
    fprintf(fp, ".measure tran leakage_power_local_interc find p(Vgvdd_local_interc) at=0\n");
  } else {
    /* Leakage power of SRAMs */
    fprintf(fp, ".measure tran leakage_power_sram_local_routing avg p(Vgvdd_sram_local_routing) from=0 to='clock_period'\n");
    fprintf(fp, ".measure tran leakage_power_sram_luts avg p(Vgvdd_sram_luts) from=0 to='clock_period'\n");
    /* Global power of Local Interconnections*/
    fprintf(fp, ".measure tran leakage_power_local_interc avg p(Vgvdd_local_interc) from=0 to='clock_period'\n");
  }
  /* Leakge power of Hard logic */
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_HARDLOGIC, SPICE_MEASURE_LEAKAGE_POWER, num_clock_cycle, spice, leakage_only);
  /* Leakage power of LUTs*/
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_LUT, SPICE_MEASURE_LEAKAGE_POWER, num_clock_cycle, spice, leakage_only);
  /* Leakage power of FFs*/
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_FF, SPICE_MEASURE_LEAKAGE_POWER, num_clock_cycle, spice, leakage_only);

  if (TRUE == leakage_only) {
    return;
  }

  /* Dynamic power */
  /* Dynamic power of SRAMs */
  fprintf(fp, ".measure tran dynamic_power_sram_local_routing avg p(Vgvdd_sram_local_routing) from='clock_period' to='%d*clock_period'\n", num_clock_cycle);
  fprintf(fp, ".measure tran dynamic_power_sram_luts avg p(Vgvdd_sram_luts) from='clock_period' to='%d*clock_period'\n", num_clock_cycle);
  /* Dynamic power of Local Interconnections */
  fprintf(fp, ".measure tran dynamic_power_local_interc avg p(Vgvdd_local_interc) from='clock_period' to='%d*clock_period'\n", num_clock_cycle);
  /* Dynamic power of Hard Logic */
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_HARDLOGIC, SPICE_MEASURE_DYNAMIC_POWER, num_clock_cycle, spice, leakage_only);
  /* Dynamic power of LUTs */
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_LUT, SPICE_MEASURE_DYNAMIC_POWER, num_clock_cycle, spice, leakage_only);
  /* Dynamic power of FFs */
  fprint_measure_vdds_spice_model(fp, SPICE_MODEL_FF, SPICE_MEASURE_DYNAMIC_POWER, num_clock_cycle, spice, leakage_only);

  return;
}

/* Top-level function in this source file */
void fprint_spice_grid_testbench(char* formatted_spice_dir,
                                 char* circuit_name,
                                 char* grid_test_bench_name,
                                 char* include_dir_path,
                                 char* subckt_dir_path,
                                 t_ivec*** LL_rr_node_indices,
                                 int num_clock,
                                 t_arch arch,
                                 boolean leakage_only) {
  FILE* fp = NULL;
  char* formatted_subckt_dir_path = format_dir_path(subckt_dir_path);
  char* temp_include_file_path = NULL;
  char* title = my_strcat("FPGA Grid Testbench for Design: ", circuit_name);
  char* grid_testbench_file_path = my_strcat(formatted_spice_dir, grid_test_bench_name);

  /* Check if the path exists*/
  fp = fopen(grid_testbench_file_path,"w");
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s,LINE[%d])Failure in create Grid Testbench SPICE netlist %s!",__FILE__, __LINE__, grid_testbench_file_path); 
    exit(1);
  } 
  
  vpr_printf(TIO_MESSAGE_INFO, "Writing Grid Testbench for %s...\n", circuit_name);
 
  /* Print the title */
  fprint_spice_head(fp, title);
  my_free(title);

  /* print technology library and design parameters*/
  fprint_tech_lib(fp, arch.spice->tech_lib);

  /* Include parameter header files */
  fprint_spice_include_param_headers(fp, include_dir_path);

  /* Include Key subckts */
  fprint_spice_include_key_subckts(fp, subckt_dir_path);

  /* Include user-defined sub-circuit netlist */
  init_include_user_defined_netlists(*(arch.spice));
  fprint_include_user_defined_netlists(fp, *(arch.spice));
  
  /* Special subckts for Top-level SPICE netlist */
  fprintf(fp, "****** Include subckt netlists: Look-Up Tables (LUTs) *****\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, luts_spice_file_name);
  fprintf(fp, ".include \'%s\'\n", temp_include_file_path);
  my_free(temp_include_file_path);

  fprintf(fp, "****** Include subckt netlists: Logic Blocks *****\n");
  temp_include_file_path = my_strcat(formatted_subckt_dir_path, logic_block_spice_file_name);
  fprintf(fp, ".include \'%s\'\n", temp_include_file_path);
  my_free(temp_include_file_path);

  /* Print simulation temperature and other options for SPICE */
  fprint_spice_options(fp, arch.spice->spice_params);

  /* Global nodes: Vdd for SRAMs, Logic Blocks(Include IO), Switch Boxes, Connection Boxes */
  fprint_grid_testbench_global_ports(fp, num_clock, (*arch.spice));
 
  /* Quote defined Logic blocks subckts (Grids) */
  fprint_call_defined_core_grids(fp);

  /* Back-anotate activity information to each routing resource node 
   * (We should have activity of each Grid port) 
   */

  /* Add stimulations */
  fprint_grid_testbench_stimulations(fp, num_clock, (*arch.spice), LL_rr_node_indices);

  /* Add measurements */  
  fprint_grid_testbench_measurements(fp, (*arch.spice), leakage_only);

  /* SPICE ends*/
  fprintf(fp, ".end\n");

  /* Close the file*/
  fclose(fp);

  return;
}

