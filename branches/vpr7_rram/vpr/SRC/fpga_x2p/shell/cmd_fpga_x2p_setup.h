/* Command-line options for VPR place_and_route */
/* Add any option by following the format of t_opt_info */
t_opt_info fpga_x2p_setup_opts[] = {
  {"activity_file", 0, OPT_WITHVAL, OPT_CHAR, OPT_OPT, OPT_NONDEF, "Specify the activity file"},
  {"parasitic_net_estimation", 0, OPT_NONVAL, OPT_CHAR, OPT_OPT, OPT_NONDEF, "Enable the parasitic net estimation when building SPICE testbenches"},
  {"rename_illegal_port", 0, OPT_NONVAL, OPT_CHAR, OPT_OPT, OPT_NONDEF, "Rename illegal ports that violates Verilog syntax"},
  {"signal_density_weight", 0, OPT_WITHVAL, OPT_FLOAT, OPT_OPT, OPT_NONDEF, "Specify the signal density weight when doing the average number"},
  {"sim_window_size", 0, OPT_WITHVAL, OPT_FLOAT, OPT_OPT, OPT_NONDEF, "Specify the size of window when doing simulation"},
  {LAST_OPT_NAME, 0, OPT_NONVAL, OPT_CHAR, OPT_OPT, OPT_NONDEF, "Launch help desk"}
};

/* Function to execute the command */
void shell_execute_fpga_x2p_setup(t_shell_env* env, t_opt_info* opts);
