

void fprint_spice_lut_testbench(char* formatted_spice_dir,
                                char* circuit_name,
                                char* lut_testbench_name,
                                char* include_dir_path,
                                char* subckt_dir_path,
                                t_ivec*** LL_rr_node_indices,
                                int num_clock,
                                t_arch arch,
                                boolean leakage_only);
