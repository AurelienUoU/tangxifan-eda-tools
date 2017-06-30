/***********************************/
/* Synthesizable Verilog Dumping   */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
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

/* FPGA-SPICE utils */
#include "read_xml_spice_util.h"
#include "linkedlist.h"
#include "fpga_spice_utils.h"
#include "fpga_spice_globals.h"

/* syn_verilog globals */

/****** Subroutines *******/
void init_list_include_verilog_netlists(t_spice* spice) { 
  int i, j, cur;
  int to_include = 0;
  int num_to_include = 0;

  /* Initialize */
  for (i = 0; i < spice->num_include_netlist; i++) { 
    FreeSpiceModelNetlist(&(spice->include_netlists[i]));
  }
  my_free(spice->include_netlists);
  spice->include_netlists = NULL;
  spice->num_include_netlist = 0;

  /* Generate include netlist list */
  vpr_printf(TIO_MESSAGE_INFO, "Listing Verilog Netlist Names to be included...\n");
  for (i = 0; i < spice->num_spice_model; i++) {
    if (NULL != spice->spice_models[i].verilog_netlist) {
      /* Check if this netlist name has already existed in the list */
      to_include = 1;
      for (j = 0; j < i; j++) {
        if (NULL == spice->spice_models[j].verilog_netlist) {
          continue;
        }
        if (0 == strcmp(spice->spice_models[j].verilog_netlist, spice->spice_models[i].verilog_netlist)) {
          to_include = 0;
          break;
        }
      }
      /* Increamental */
      if (1 == to_include) {
        num_to_include++;
      }
    }
  }

  /* realloc */
  spice->include_netlists = (t_spice_model_netlist*)my_realloc(spice->include_netlists, 
                              sizeof(t_spice_model_netlist)*(num_to_include + spice->num_include_netlist));

  /* Fill the new included netlists */
  cur = spice->num_include_netlist;
  for (i = 0; i < spice->num_spice_model; i++) {
    if (NULL != spice->spice_models[i].verilog_netlist) {
      /* Check if this netlist name has already existed in the list */
      to_include = 1;
      for (j = 0; j < i; j++) {
        if (NULL == spice->spice_models[j].verilog_netlist) {
          continue;
        }
        if (0 == strcmp(spice->spice_models[j].verilog_netlist, spice->spice_models[i].verilog_netlist)) {
          to_include = 0;
          break;
        }
      }
      /* Increamental */
      if (1 == to_include) {
        spice->include_netlists[cur].path = my_strdup(spice->spice_models[i].verilog_netlist); 
        spice->include_netlists[cur].included = 0;
        vpr_printf(TIO_MESSAGE_INFO, "[%d] %s\n", cur+1, spice->include_netlists[cur].path);
        cur++;
      }
    }
  }
  /* Check */
  assert(cur == (num_to_include + spice->num_include_netlist));
  /* Update */
  spice->num_include_netlist += num_to_include;
  
  return;
}


void init_include_user_defined_verilog_netlists(t_spice spice) {
  int i;

  /* Include user-defined sub-circuit netlist */
  for (i = 0; i < spice.num_include_netlist; i++) {
    spice.include_netlists[i].included = 0;
  }

  return;
}

void dump_include_user_defined_verilog_netlists(FILE* fp,
                                                t_spice spice) {
  int i;

  /* A valid file handler*/
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s, [LINE%d])Invalid File Handler!\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Include user-defined sub-circuit netlist */
  for (i = 0; i < spice.num_include_netlist; i++) {
    if (0 == spice.include_netlists[i].included) {
      assert(NULL != spice.include_netlists[i].path);
      fprintf(fp, "// `include \"%s\"\n", spice.include_netlists[i].path);
      spice.include_netlists[i].included = 1;
    } else {
      assert(1 == spice.include_netlists[i].included);
    }
  } 

  return;
}

void dump_verilog_file_header(FILE* fp,
                              char* usage) {
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(FILE:%s, LINE[%d]) FileHandle is NULL!\n",__FILE__,__LINE__); 
    exit(1);
  } 
  fprintf(fp,"//-------------------------------------------\n");
  fprintf(fp,"//    FPGA Synthesizable Verilog Netlist     \n");
  fprintf(fp,"//    Description: %s \n",usage);
  fprintf(fp,"//           Author: Xifan TANG              \n");
  fprintf(fp,"//        Organization: EPFL/IC/LSI          \n");
  fprintf(fp,"//    Date: %s \n",my_gettime());
  fprintf(fp,"//-------------------------------------------\n");
  fprintf(fp,"//----- Time scale -----\n");
  fprintf(fp,"`timescale 1ns / 1ps\n");
  fprintf(fp,"\n");

  return;
}

/* Decode BL and WL bits for a SRAM
 * SRAM could be
 * 1. NV SRAM
 * or
 * 2. SRAM 
 */
void decode_verilog_memory_bank_sram(t_spice_model* cur_sram_spice_model, int sram_bit,
                                     int bl_len, int wl_len, int bl_offset, int wl_offset,
                                     int* bl_conf_bits, int* wl_conf_bits) {
  int i;

  /* Check */
  assert(NULL != cur_sram_spice_model);
  assert(NULL != bl_conf_bits);
  assert(NULL != wl_conf_bits);
  assert((1 == sram_bit)||(0 == sram_bit));

  /* All the others should be zero */
  for (i = 0; i < bl_len; i++) {
    bl_conf_bits[i] = 0;
  }
  for (i = 0; i < wl_len; i++) {
    wl_conf_bits[i] = 0;
  }
  
  /* Depending on the design technology of SRAM */
  switch (cur_sram_spice_model->design_tech) {
  case SPICE_MODEL_DESIGN_CMOS:
    /* CMOS SRAM */
    /* Make sure there is only 1 BL and 1 WL */
    assert((1 == bl_len)&&(1 == wl_len));
    /* We always assume that BL is a write-enable signal
     * While WL contains what data will be written into SRAM 
     */
    bl_conf_bits[0] = 1;
    wl_conf_bits[0] = sram_bit;
    break;
  case SPICE_MODEL_DESIGN_RRAM:
    /* NV SRAM (RRAM-based) */
    /* We need at least 2 BLs and 2 WLs but no more than 3, See schematic in manual */
    /* Whatever the number of BLs and WLs, (RRAM0)
     * when sram bit is 1, last bit of BL should be enabled
     * while first bit of WL should be enabled at the same time
     * when sram bit is 0, last bit of WL should be enabled
     * while first bit of BL should be enabled at the same time
     */
    assert((1 < bl_len)&&(bl_len < 4)); 
    assert((1 < wl_len)&&(wl_len < 4)); 
    assert((-1 < bl_offset)&&(bl_offset < bl_len));
    assert((-1 < wl_offset)&&(wl_offset < wl_len));
    /* In addition, we will may need two programing cycles.
     * The first cycle is dedicated to programming RRAM0
     * The second cycle is dedicated to programming RRAM1
     */
    if (1 == sram_bit) {
      bl_conf_bits[bl_len-1] = 1;
      wl_conf_bits[0 + wl_offset] = 1;
    } else {
      assert(0 == sram_bit);
      bl_conf_bits[0 + bl_offset] = 1;
      wl_conf_bits[wl_len-1] = 1;
    } 
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid design technology for SRAM!\n", 
               __FILE__, __LINE__);
    exit(1);
  }

  return;
}

/** Decode 1-level 4T1R MUX
 */
void decode_verilog_one_level_4t1r_mux(int path_id, 
                                       int bit_len, int* conf_bits) { 
  int i; 

  /* Check */
  assert(0 < bit_len);
  assert(NULL != conf_bits);
  assert((-1 < path_id)&&((path_id < bit_len/2 - 1)||(path_id == bit_len/2 - 1)));

  /* All the others should be zero */
  for (i = 0; i < bit_len; i++) {
    conf_bits[i] = 0;
  }

  /* Last bit of WL should be 1 */
  conf_bits[bit_len-1] = 1;
  /* determine  which BL should be 1*/
  conf_bits[path_id] = 1;

  return;
}

/** Decode multi-level 4T1R MUX
 */
void decode_verilog_multilevel_4t1r_mux(int num_level, int num_input_basis,
                                        int mux_size, int path_id, 
                                        int bit_len, int* conf_bits) { 
  int i, active_basis_path_id;

  /* Check */
  assert(0 < bit_len);
  assert(NULL != conf_bits);
  /* assert((-1 < path_id)&&(path_id < bit_len/2 - 1)); */
  /* Start from first level to the last level */
  active_basis_path_id = path_id;
  for (i = 0; i < num_level; i++) {
    /* Treat each basis as a 1-level 4T1R MUX */
    active_basis_path_id = active_basis_path_id % num_input_basis;
    /* Last bit of WL should be 1 */
    conf_bits[bit_len/2 + (num_input_basis+1)*(i+1) - 1] = 1;
    /* determine  which BL should be 1*/
    conf_bits[(num_input_basis+1)*i + active_basis_path_id] = 1;
  }

  return;
}

/** Decode the configuration bits for a 4T1R-based MUX
 *  Determine the number of configuration bits 
 *  Configuration bits are decoded depending on the MUX structure:
 *  1. 1-level; 2. multi-level (tree-like);
 */
void decode_verilog_rram_mux(t_spice_model* mux_spice_model,
                             int mux_size, int path_id,
                             int* bit_len, int** conf_bits, int* mux_level) {
  int num_level, num_input_basis;

  /* Check */
  assert(NULL != mux_level);
  assert(NULL != bit_len);
  assert(NULL != conf_bits);
  assert((-1 < path_id)&&(path_id < mux_size));
  assert(SPICE_MODEL_MUX == mux_spice_model->type);
  assert(SPICE_MODEL_DESIGN_RRAM == mux_spice_model->design_tech);
  
  /* Initialization */
  (*mux_level) = 0;
  (*bit_len) = 0;
  (*conf_bits) = NULL;

  (*bit_len) = 2 * count_num_sram_bits_one_spice_model(mux_spice_model, mux_size);
  
  /* Switch cases: MUX structure */
  switch (mux_spice_model->design_tech_info.structure) {
  case SPICE_MODEL_STRUCTURE_ONELEVEL:
    /* Number of configuration bits is 2*(input_size+1) */
    num_level = 1;
    break;
  case SPICE_MODEL_STRUCTURE_TREE:
    /* Number of configuration bits is num_level* 2*(basis+1) */
    num_level = determine_tree_mux_level(mux_size); 
    num_input_basis = 2;
    break;
  case SPICE_MODEL_STRUCTURE_MULTILEVEL:
    /* Number of configuration bits is num_level* 2*(basis+1) */
    num_level = mux_spice_model->design_tech_info.mux_num_level; 
    num_input_basis = determine_num_input_basis_multilevel_mux(mux_size, num_level);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid MUX structure!\n", 
               __FILE__, __LINE__);
    exit(1);
  }
 
  /* Malloc configuration bits */
  (*conf_bits) = (int*)my_calloc((*bit_len), sizeof(int));

  /* Decode configuration bits : BL & WL*/
  /* Switch cases: MUX structure */
  switch (mux_spice_model->design_tech_info.structure) {
  case SPICE_MODEL_STRUCTURE_ONELEVEL:
    decode_verilog_one_level_4t1r_mux(path_id, (*bit_len), (*conf_bits)); 
    break;
  case SPICE_MODEL_STRUCTURE_TREE:
  case SPICE_MODEL_STRUCTURE_MULTILEVEL:
    decode_verilog_multilevel_4t1r_mux(num_level, num_input_basis, mux_size, 
                                       path_id, (*bit_len), (*conf_bits)); 
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid MUX structure!\n", 
               __FILE__, __LINE__);
    exit(1);
  }
  
  (*mux_level) = num_level;
  
  return;
}

/* Determine the size of input address of a decoder */
int determine_decoder_size(int num_addr_out) {
  return ceil(log(num_addr_out)/log(2.));
}

char* chomp_verilog_node_prefix(char* verilog_node_prefix) {
  int len = 0;
  char* ret = NULL;

  if (NULL == verilog_node_prefix) {
    return NULL;
  }

  len = strlen(verilog_node_prefix); /* String length without the last "\0"*/
  ret = (char*)my_malloc(sizeof(char)*(len+1));
  
  /* Don't do anything when input is NULL*/
  if (NULL == verilog_node_prefix) {
    my_free(ret);
    return NULL;
  }

  strcpy(ret,verilog_node_prefix);
  /* If the path end up with "_" we should remove it*/
  /*
  if ('_' == ret[len-1]) {
    ret[len-1] = ret[len];
  }
  */

  return ret;
}

char* format_verilog_node_prefix(char* verilog_node_prefix) {
  int len = strlen(verilog_node_prefix); /* String length without the last "\0"*/
  char* ret = (char*)my_malloc(sizeof(char)*(len+1));
 
  /* Don't do anything when input is NULL*/ 
  if (NULL == verilog_node_prefix) {
    my_free(ret);
    return NULL;
  }

  strcpy(ret,verilog_node_prefix);
  /* If the path does not end up with "_" we should complete it*/
  /*
  if (ret[len-1] != '_') {
    strcat(ret, "_");
  }
  */
  return ret;
}

/* Return the port_type in a verilog format */
char* verilog_convert_port_type_to_string(enum e_spice_model_port_type port_type) {
  switch (port_type) {
  case SPICE_MODEL_PORT_INPUT: 
  case SPICE_MODEL_PORT_CLOCK: 
  case SPICE_MODEL_PORT_SRAM:
  case SPICE_MODEL_PORT_BL:
  case SPICE_MODEL_PORT_WL:
    return "input";
  case SPICE_MODEL_PORT_OUTPUT: 
    return "output";
  case SPICE_MODEL_PORT_INOUT: 
    return "inout";
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s, [LINE%d])Invalid port type!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  return NULL;
}

/* Dump all the global ports that are stored in the linked list 
 * Return the number of ports that have been dumped 
 */
int rec_dump_verilog_spice_model_global_ports(FILE* fp, 
                                              t_spice_model* cur_spice_model,
                                              boolean dump_port_type, boolean recursive) {
  int iport, dumped_port_cnt;
  boolean dump_comma = FALSE;

  dumped_port_cnt = 0;

  /* Check */
  assert(NULL != cur_spice_model);
  if (0 < cur_spice_model->num_port) {
    assert(NULL != cur_spice_model->ports);
  }

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
  }

  fprintf(fp, "//----- BEGIN Global ports of SPICE_MODEL(%s) -----\n",
          cur_spice_model->name);

  for (iport = 0; iport < cur_spice_model->num_port; iport++) {
    /* if this spice model requires customized netlist to be included, we do not go recursively */
    if (TRUE == recursive) { 
      /* GO recursively first, and meanwhile count the number of global ports */
      /* For the port that requires another spice_model, i.e., SRAM
       * We need include any global port in that spice model
       */
      if (NULL != cur_spice_model->ports[iport].spice_model) {
        /* Check if we need to dump a comma */
        if (TRUE == dump_comma) {
          fprintf(fp, ",\n");
        }
        dumped_port_cnt += 
           rec_dump_verilog_spice_model_global_ports(fp, cur_spice_model->ports[iport].spice_model, 
                                                     dump_port_type, recursive);
        /* Decide if we need a comma */
        dump_comma = TRUE; 
        continue;
      }
    }
    /* By pass non-global ports*/
    if (FALSE == cur_spice_model->ports[iport].is_global) {
      continue;
    }
    /* Check if we need to dump a comma */
    if (TRUE == dump_comma) {
      fprintf(fp, ",\n");
    }
    if (TRUE == dump_port_type) {
      fprintf(fp, "%s [0:%d] %s", 
              verilog_convert_port_type_to_string(cur_spice_model->ports[iport].type),
              cur_spice_model->ports[iport].size - 1, 
              cur_spice_model->ports[iport].prefix);
    } else {
      fprintf(fp, "%s[0:%d]", 
            cur_spice_model->ports[iport].prefix,
            cur_spice_model->ports[iport].size - 1); 
    }
    /* Decide if we need a comma */
    dump_comma = TRUE; 
    /* Update counter */
    dumped_port_cnt++;
  }
  
  fprintf(fp, "\n");
  fprintf(fp, "//----- END Global ports of SPICE_MODEL(%s)-----\n",
          cur_spice_model->name);

  return dumped_port_cnt;
}

/* Dump all the global ports that are stored in the linked list */
int dump_verilog_global_ports(FILE* fp, t_llist* head,
                               boolean dump_port_type) {
  t_llist* temp = head;
  t_spice_model_port* cur_global_port = NULL;
  int dumped_port_cnt = 0;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
  }

  fprintf(fp, "//----- BEGIN Global ports -----\n");
  while(NULL != temp) {
    cur_global_port = (t_spice_model_port*)(temp->dptr); 
    if (TRUE == dump_port_type) {
      fprintf(fp, "%s [0:%d] %s", 
              verilog_convert_port_type_to_string(cur_global_port->type),
              cur_global_port->size - 1, 
              cur_global_port->prefix);
    } else {
      fprintf(fp, "%s[0:%d]", 
              cur_global_port->prefix,
              cur_global_port->size - 1); 
    }
    /* if this is the tail, we do not dump a comma */
    if (NULL != temp->next) {
     fprintf(fp, ",");
    }
    fprintf(fp, "\n");
    /* Update counter */
    dumped_port_cnt++;
    /* Go to the next */
    temp = temp->next;
  }
  fprintf(fp, "//----- END Global ports -----\n");

  return dumped_port_cnt;
}

void dump_verilog_sram_one_port(FILE* fp, 
                                t_sram_orgz_info* cur_sram_orgz_info,
                                int sram_lsb, int sram_msb,
                                int port_type_index, boolean dump_port_type) {
  t_spice_model* mem_model = NULL;
  char* port_name = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
 
  switch (cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
    mem_model = cur_sram_orgz_info->standalone_sram_info->mem_model;
    if (0 == port_type_index) {
      port_name = "out";
    } else {
      assert(1 == port_type_index);
      port_name = "outb";
    }
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    mem_model = cur_sram_orgz_info->scff_info->mem_model;
    if (0 == port_type_index) {
      port_name = "scff_in";
    } else {
      assert(1 == port_type_index);
      port_name = "scff_out";
    }
    break;
  case SPICE_SRAM_MEMORY_BANK:
    mem_model = cur_sram_orgz_info->mem_bank_info->mem_model;
    if (0 == port_type_index) {
      port_name = "bl";
    } else {
      assert(1 == port_type_index);
      port_name = "wl";
    }
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization !\n",
               __FILE__, __LINE__);
    exit(1);
  }

  if (TRUE == dump_port_type) {
    fprintf(fp,"input [%d:%d] %s_%s ", 
            sram_lsb, sram_msb,
            mem_model->prefix, port_name);
  } else {
    fprintf(fp,"%s_%s[%d:%d] ", 
            mem_model->prefix, port_name,
            sram_lsb, sram_msb);
  }

  /* Free */
  /* Local variables such as port1_name and port2 name are automatically freed  */

  return;
}

/* Dump SRAM ports, which is supposed to be the last port in the port list */
void dump_verilog_sram_ports(FILE* fp, 
                             t_sram_orgz_info* cur_sram_orgz_info,
                             int sram_lsb, int sram_msb,
                             boolean dump_port_type) {
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  if (0 > (sram_msb - sram_lsb)) {
    return;
  }

  if ((sram_lsb < 0)||(sram_msb < 0)) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid sram_lsb(%d) and sram_msb(%d)!\n",
               __FILE__, __LINE__, sram_lsb, sram_msb);
    return;
  }

  /* Dump the first port: SRAM_out of CMOS MUX or BL of RRAM MUX */ 
  dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                             sram_lsb, sram_msb, 
                             0, dump_port_type);
  fprintf(fp, ",\n");
  /* Dump the first port: SRAM_outb of CMOS MUX or WL of RRAM MUX */ 
  dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                             sram_lsb, sram_msb, 
                             1, dump_port_type);
  
  return;
}

void dump_verilog_reserved_sram_one_port(FILE* fp, 
                                         t_sram_orgz_info* cur_sram_orgz_info,
                                         int sram_lsb, int sram_msb,
                                         int port_type_index, boolean dump_port_type) {
  t_spice_model* mem_model = NULL;
  char* port_name = NULL;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }
 
  switch (cur_sram_orgz_info->type) {
  case SPICE_SRAM_STANDALONE:
  case SPICE_SRAM_SCAN_CHAIN:
    return;
  case SPICE_SRAM_MEMORY_BANK:
    get_sram_orgz_info_mem_model(cur_sram_orgz_info, &mem_model);
    if (0 == port_type_index) {
      port_name = "bl";
      port_name = "reserved_bl";
    } else {
      assert(1 == port_type_index);
      port_name = "reserved_wl";
    }
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid type of SRAM organization !\n",
               __FILE__, __LINE__);
    exit(1);
  }

  if (TRUE == dump_port_type) {
    fprintf(fp,"input [%d:%d] %s_%s ", 
            sram_lsb, sram_msb,
            mem_model->prefix, port_name);
  } else {
    fprintf(fp,"%s_%s[%d:%d] ", 
            mem_model->prefix, port_name,
            sram_lsb, sram_msb);
  }

  /* Free */
  /* Local variables such as port1_name and port2 name are automatically freed  */

  return;
}


/* Dump SRAM ports, which is supposed to be the last port in the port list */
void dump_verilog_reserved_sram_ports(FILE* fp, 
                                      t_sram_orgz_info* cur_sram_orgz_info,
                                      int sram_lsb, int sram_msb,
                                      boolean dump_port_type) {
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  if (0 > (sram_msb - sram_lsb)) {
    return;
  }

  /* Dump the first port: SRAM_out of CMOS MUX or BL of RRAM MUX */ 
  
  if ((sram_lsb < 0)||(sram_msb < 0)) {
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid sram_lsb(%d) and sram_msb(%d)!\n",
               __FILE__, __LINE__, sram_lsb, sram_msb);
    return;
  }

  dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info,
                                      sram_lsb, sram_msb,
                                      0, dump_port_type);
  fprintf(fp, ",\n");
  /* Dump the first port: SRAM_outb of CMOS MUX or WL of RRAM MUX */ 
  dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info,
                                      sram_lsb, sram_msb,
                                      1, dump_port_type);
 
  return;
}

/* Dump a verilog submodule of SRAM, according to SRAM organization type */
void dump_verilog_sram_submodule(FILE* fp, t_sram_orgz_info* cur_sram_orgz_info,
                                 t_spice_model* cur_sram_verilog_model) {
  int cur_bl, cur_wl, num_bl_ports, num_wl_ports, cur_num_sram;
  t_spice_model_port** bl_port = NULL;
  t_spice_model_port** wl_port = NULL;
  int num_bl_per_sram = 0;
  int num_wl_per_sram = 0;

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  assert(NULL != cur_sram_orgz_info);
  assert(NULL != cur_sram_verilog_model);
  assert((SPICE_MODEL_SRAM == cur_sram_verilog_model->type)
        || (SPICE_MODEL_SCFF == cur_sram_verilog_model->type));

  /* Get current index of SRAM module */
  cur_num_sram = get_sram_orgz_info_num_mem_bit(cur_sram_orgz_info); 

  switch (cur_sram_orgz_info->type) {
  case SPICE_SRAM_MEMORY_BANK:
    /* Detect the SRAM SPICE model linked to this SRAM port */
    find_bl_wl_ports_spice_model(cur_sram_verilog_model, 
                                 &num_bl_ports, &bl_port, &num_wl_ports, &wl_port); 
    assert(1 == num_bl_ports);
    assert(1 == num_wl_ports);
    num_bl_per_sram = bl_port[0]->size; 
    num_wl_per_sram = wl_port[0]->size; 

    /* SRAM subckts*/
    fprintf(fp, "%s %s_%d_ (", cur_sram_verilog_model->name, cur_sram_verilog_model->prefix, 
                               cur_sram_verilog_model->cnt); 
    /* Only dump the global ports belonging to a spice_model */
    if (0 < rec_dump_verilog_spice_model_global_ports(fp, cur_sram_verilog_model, FALSE, TRUE)) {
      fprintf(fp, ",\n");
    }
    fprintf(fp, "%s_out[%d], ", cur_sram_verilog_model->prefix, cur_num_sram); /* Input*/
    fprintf(fp, "%s_out[%d], %s_outb[%d], ", 
            cur_sram_verilog_model->prefix, cur_num_sram, 
            cur_sram_verilog_model->prefix, cur_num_sram); /* Outputs */
    get_sram_orgz_info_num_blwl(cur_sram_orgz_info, &cur_bl, &cur_wl); 
    /* Connect to Bit lines and Word lines, consider each conf_bit */
    fprintf(fp, "%s_%d_configbus0[%d:%d], ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt, 
            cur_bl, cur_bl + num_bl_per_sram - 1); 
    fprintf(fp, "%s_%d_configbus1[%d:%d] ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt, 
            cur_wl, cur_wl + num_wl_per_sram - 1); /* Outputs */
    fprintf(fp, ");\n");  //
    /* Update the counter */
    update_sram_orgz_info_num_mem_bit(cur_sram_orgz_info,
                                      cur_num_sram + 1);
    update_sram_orgz_info_num_blwl(cur_sram_orgz_info, 
                                   cur_bl + 1,
                                   cur_wl + 1);
    break;
  case SPICE_SRAM_STANDALONE:
    /* SRAM subckts*/
    fprintf(fp, "%s %s_%d_ (", cur_sram_verilog_model->name, cur_sram_verilog_model->prefix, 
                               cur_sram_verilog_model->cnt); 
    /* Only dump the global ports belonging to a spice_model */
    if (0 < rec_dump_verilog_spice_model_global_ports(fp, cur_sram_verilog_model, FALSE, TRUE)) {
      fprintf(fp, ",\n");
    }
    fprintf(fp, "%s_out[%d], ", cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt); /* Input*/
    fprintf(fp, "%s_out[%d], %s_outb[%d] ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt, 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt); /* Outputs */
    fprintf(fp, ");\n");  //
    /* Update the counter */
    update_sram_orgz_info_num_mem_bit(cur_sram_orgz_info,
                                      cur_num_sram + 1);
    break;
  case SPICE_SRAM_SCAN_CHAIN:
    /* Add a scan-chain DFF module here ! */
    fprintf(fp, "%s %s_%d_ (", cur_sram_verilog_model->name, cur_sram_verilog_model->prefix, 
                               cur_sram_verilog_model->cnt); 
    /* Only dump the global ports belonging to a spice_model */
    if (0 < rec_dump_verilog_spice_model_global_ports(fp, cur_sram_verilog_model, FALSE, TRUE)) {
      fprintf(fp, ",\n");
    }
    /* Input of Scan-chain DFF, should be connected to the output of its precedent */
    fprintf(fp, "%s_scff_in[%d], ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt); /* Input*/
    /* Output of Scan-chain DFF, should be connected to the output of its successor */
    fprintf(fp, "%s_scff_out[%d], ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt,
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt); /* Regular Outputs */
    /* Memory outputs of Scan-chain DFF, should be connected to the SRAM(memory port) of IOPAD, MUX and LUT */
    fprintf(fp, "%s_out[%d], %s_outb[%d] ", 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt, 
            cur_sram_verilog_model->prefix, cur_sram_verilog_model->cnt); /* Memory Outputs */
    fprintf(fp, ");\n");  //
    /* Update the counter */
    update_sram_orgz_info_num_mem_bit(cur_sram_orgz_info,
                                      cur_num_sram + 1);
    break;
  default:
    vpr_printf(TIO_MESSAGE_ERROR, "(File:%s,[LINE%d])Invalid SRAM organization type!\n",
               __FILE__, __LINE__);
    exit(1);
  }

  /* Update the counter */
  cur_sram_verilog_model->cnt++;

  return;
}

/* Dump MUX reserved and normal configuration wire bus */
void dump_verilog_mem_config_bus(FILE* fp, t_spice_model* mem_spice_model, 
                                 t_sram_orgz_info* cur_sram_orgz_info,
                                 int cur_num_sram,
                                 int num_mem_reserved_conf_bits,
                                 int num_mem_conf_bits) { 
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check */
  assert(NULL != mem_spice_model);
  assert((SPICE_MODEL_SRAM == mem_spice_model->type)
        || (SPICE_MODEL_SCFF == mem_spice_model->type));

  /* configuration wire bus */
  if (0 < (num_mem_reserved_conf_bits + num_mem_conf_bits)) {
    /* First bus is for sram_out in CMOS MUX or BL in RRAM MUX */
    fprintf(fp, "wire [%d:%d] %s_%d_configbus0;\n",
            cur_num_sram,
            cur_num_sram + num_mem_reserved_conf_bits + num_mem_conf_bits - 1,
            mem_spice_model->prefix, mem_spice_model->cnt);
    /* Second bus is for sram_out_inv in CMOS MUX or WL in RRAM MUX */
    fprintf(fp, "wire [%d:%d] %s_%d_configbus1;\n",
            cur_num_sram,
            cur_num_sram + num_mem_reserved_conf_bits + num_mem_conf_bits - 1,
            mem_spice_model->prefix, mem_spice_model->cnt);
  }
  /* Connect wires to config bus */
  /* reserved configuration bits */
  if (0 < num_mem_reserved_conf_bits) {
    fprintf(fp, "assign %s_%d_configbus0[%d:%d] = ",
            mem_spice_model->prefix, mem_spice_model->cnt,
            cur_num_sram,
            cur_num_sram + num_mem_reserved_conf_bits - 1);
    dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info, 
                                        0, num_mem_reserved_conf_bits - 1,
                                        0, FALSE);
    fprintf(fp, ";\n");
    fprintf(fp, "assign %s_%d_configbus1[%d:%d] = ",
            mem_spice_model->prefix, mem_spice_model->cnt,
            cur_num_sram,
            cur_num_sram + num_mem_reserved_conf_bits - 1);
    dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info, 
                                        0, num_mem_reserved_conf_bits - 1,
                                        1, FALSE);
    fprintf(fp, ";\n");
  }
  /* normal configuration bits */
  if (0 < num_mem_conf_bits) {
    fprintf(fp, "assign %s_%d_configbus0[%d:%d] = ",
            mem_spice_model->prefix, mem_spice_model->cnt,
            cur_num_sram + num_mem_reserved_conf_bits,
            cur_num_sram + num_mem_reserved_conf_bits + num_mem_conf_bits - 1);
    dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                               cur_num_sram, cur_num_sram + num_mem_conf_bits - 1,
                               0, FALSE);
    fprintf(fp, ";\n");
    fprintf(fp, "assign %s_%d_configbus1[%d:%d] = ",
            mem_spice_model->prefix,  mem_spice_model->cnt,
            cur_num_sram + num_mem_reserved_conf_bits,
            cur_num_sram + num_mem_reserved_conf_bits + num_mem_conf_bits - 1);
    dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                               cur_num_sram, cur_num_sram + num_mem_conf_bits - 1,
                               1, FALSE);
    fprintf(fp, ";\n");
  }

  return;
}


/* Dump MUX reserved and normal configuration wire bus */
void dump_verilog_mux_config_bus(FILE* fp, t_spice_model* mux_spice_model, 
                                 t_sram_orgz_info* cur_sram_orgz_info,
                                 int mux_size, int cur_num_sram,
                                 int num_mux_reserved_conf_bits,
                                 int num_mux_conf_bits) { 
  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  /* Check */
  assert(NULL != mux_spice_model);
  assert(SPICE_MODEL_MUX == mux_spice_model->type);

  /* configuration wire bus */
  /* First bus is for sram_out in CMOS MUX or BL in RRAM MUX */
  fprintf(fp, "wire [0:%d] %s_size%d_%d_configbus0;\n",
          num_mux_reserved_conf_bits + num_mux_conf_bits - 1,
          mux_spice_model->prefix, mux_size, mux_spice_model->cnt);
  /* Second bus is for sram_out_inv in CMOS MUX or WL in RRAM MUX */
  fprintf(fp, "wire [0:%d] %s_size%d_%d_configbus1;\n",
          num_mux_reserved_conf_bits + num_mux_conf_bits - 1,
          mux_spice_model->prefix, mux_size, mux_spice_model->cnt);
  /* Connect wires to config bus */
  /* reserved configuration bits */
  if (0 < num_mux_reserved_conf_bits) {
    fprintf(fp, "assign %s_size%d_%d_configbus0[%d:%d] = ",
            mux_spice_model->prefix, mux_size, mux_spice_model->cnt,
            0, num_mux_reserved_conf_bits - 1);
    dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info, 
                                        0, num_mux_reserved_conf_bits - 1,
                                        0, FALSE);
    fprintf(fp, ";\n");
    fprintf(fp, "assign %s_size%d_%d_configbus1[%d:%d] = ",
            mux_spice_model->prefix, mux_size, mux_spice_model->cnt,
            0, num_mux_reserved_conf_bits - 1);
    dump_verilog_reserved_sram_one_port(fp, cur_sram_orgz_info, 
                                        0, num_mux_reserved_conf_bits - 1,
                                        1, FALSE);
    fprintf(fp, ";\n");
  }
  /* normal configuration bits */
  if (0 < num_mux_conf_bits) {
    fprintf(fp, "assign %s_size%d_%d_configbus0[%d:%d] = ",
            mux_spice_model->prefix, mux_size, mux_spice_model->cnt,
            num_mux_reserved_conf_bits,
            num_mux_reserved_conf_bits + num_mux_conf_bits - 1);
    dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                               cur_num_sram, cur_num_sram + num_mux_conf_bits - 1,
                               0, FALSE);
    fprintf(fp, ";\n");
    fprintf(fp, "assign %s_size%d_%d_configbus1[%d:%d] = ",
            mux_spice_model->prefix, mux_size, mux_spice_model->cnt,
            num_mux_reserved_conf_bits,
            num_mux_reserved_conf_bits + num_mux_conf_bits - 1);
    dump_verilog_sram_one_port(fp, cur_sram_orgz_info, 
                               cur_num_sram, cur_num_sram + num_mux_conf_bits - 1,
                               1, FALSE);
    fprintf(fp, ";\n");
  }

  return;
}

/* Dump common ports of each pb_type in physical mode,
 * common ports include:
 * 1. inpad; 2. outpad; 3. iopad; TODO: merge other two to iopad 
 * 4. SRAMs (standalone)
 * 5. BL/WLs
 * 6. Scan-chain FFs 
 */
void dump_verilog_grid_common_port(FILE* fp, t_spice_model* cur_verilog_model,
                                   char* general_port_prefix, int lsb, int msb,
                                   boolean dump_port_type) {

  /* Check the file handler*/ 
  if (NULL == fp) {
    vpr_printf(TIO_MESSAGE_ERROR,"(File:%s,[LINE%d])Invalid file handler.\n", 
               __FILE__, __LINE__); 
    exit(1);
  }

  assert(NULL != cur_verilog_model);
  if (0 >  msb- lsb) {
    return;
  }

  if (TRUE == dump_port_type) {
    fprintf(fp, ",\n");
    fprintf(fp, "  input [%d:%d] %s%s ", 
            msb, lsb, general_port_prefix,
            cur_verilog_model->prefix); 
  } else {
    fprintf(fp, ",\n");
    fprintf(fp, " %s%s [%d:%d] ", 
            general_port_prefix,
            cur_verilog_model->prefix, 
            msb, lsb); 
  }

  return;
}
