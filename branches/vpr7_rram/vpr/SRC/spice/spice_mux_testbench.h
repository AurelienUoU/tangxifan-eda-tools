
void fprint_spice_mux_testbench(char* formatted_spice_dir,
                                char* circuit_name,
                                char* mux_testbench_name,
                                char* include_dir_path,
                                char* subckt_dir_path,
                                t_ivec*** LL_rr_node_indices,
                                int num_clock,
                                t_spice spice,
                                boolean leakage_only);
