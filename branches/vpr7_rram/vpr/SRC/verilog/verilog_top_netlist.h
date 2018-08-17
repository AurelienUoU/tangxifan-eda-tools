
void dump_verilog_top_netlist_ports(FILE* fp,
                                    int num_clocks,
                                    char* circuit_name,
                                    t_spice verilog);

void dump_verilog_top_netlist_internal_wires(FILE* fp);

void dump_verilog_defined_channels(FILE* fp,
                                   int LL_num_rr_nodes, t_rr_node* LL_rr_node,
                                   t_ivec*** LL_rr_node_indices);

void dump_verilog_defined_grids(FILE* fp);

void dump_verilog_defined_connection_boxes(FILE* fp);

void dump_verilog_defined_switch_boxes(FILE* fp);

void dump_verilog_clb2clb_directs(FILE* fp, 
                                  int num_directs, t_clb_to_clb_directs* direct);

void dump_verilog_configuration_circuits(FILE* fp);

void dump_verilog_top_netlist(char* circuit_name,
                              char* top_netlist_name,
                              char* include_dir_path,
                              char* subckt_dir_path,
                              int LL_num_rr_nodes,
                              t_rr_node* LL_rr_node,
                              t_ivec*** LL_rr_node_indices,
                              int num_clock,
                              t_spice spice);

void dump_verilog_top_testbench(char* circuit_name,
                                char* top_netlist_name,
                                int num_clock,
                                t_syn_verilog_opts syn_verilog_opts,
                                t_spice verilog);

void dump_verilog_input_blif_testbench(char* circuit_name,
                                       char* top_netlist_name,
                                       int num_clock,
                                       t_syn_verilog_opts syn_verilog_opts,
                                       t_spice verilog);
