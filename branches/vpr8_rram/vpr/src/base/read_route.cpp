/* Read in the .route file generated by VPR.
 * This is done in order to perform timing analysis only using the
 * --analysis option. This file set up the routing tree. It also checks that
 * routing resource nodes and placement is consistent with the
 * .net, .place, or the routing resource graph files. It is assumed
 * that the .route file's formatting matches those as described in the
 * vtr documentation. For example, the coordinates is assumed to be
 * in (x,y) format. Appropriate error messages are displayed when
 * formats are incorrect or when the routing file does not match
 * other file's information*/

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>
using namespace std;

#include "atom_netlist.h"
#include "atom_netlist_utils.h"
#include "rr_graph.h"
#include "vtr_assert.h"
#include "vtr_util.h"
#include "tatum/echo_writer.hpp"
#include "vtr_log.h"
#include "check_route.h"
#include "route_common.h"
#include "vpr_types.h"
#include "globals.h"
#include "vpr_api.h"
#include "read_place.h"
#include "vpr_types.h"
#include "vpr_utils.h"
#include "vpr_error.h"
#include "place_and_route.h"
#include "timing_place.h"
#include "route_export.h"
#include "echo_files.h"
#include "route_common.h"
#include "read_route.h"

/*************Functions local to this module*************/
static void process_route(ifstream &fp, const char* filename, int& lineno);
static void process_nodes(ifstream &fp, ClusterNetId inet, const char* filename, int& lineno);
static void process_nets(ifstream &fp, ClusterNetId inet, string name, std::vector<std::string> input_tokens, const char* filename, int& lineno);
static void process_global_blocks(ifstream &fp, ClusterNetId inet, const char* filename, int& lineno);
static void format_coordinates(int &x, int &y, string coord, ClusterNetId net, const char* filename, const int lineno);
static void format_pin_info(string &pb_name, string & port_name, int & pb_pin_num, string input);
static string format_name(string name);

/*************Global Functions****************************/
bool read_route(const char* route_file, const t_router_opts& router_opts, bool verify_file_digests) {

    /* Reads in the routing file to fill in the trace.head and t_clb_opins_used data structure.
     * Perform a series of verification tests to ensure the netlist, placement, and routing
     * files match */
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& place_ctx = g_vpr_ctx.placement();
    /* Begin parsing the file */
    VTR_LOG("Begin loading FPGA routing file.\n");

    string header_str;

    ifstream fp;
    fp.open(route_file);

    int lineno = 0;

    if (!fp.is_open()) {
        vpr_throw(VPR_ERROR_ROUTE, route_file, lineno,
                "Cannot open %s routing file", route_file);
    }

    getline(fp, header_str);
    ++lineno;

    std::vector<std::string> header = vtr::split(header_str);
    if (header[0] == "Placement_File:" && header[2] == "Placement_ID:" && header[3] != place_ctx.placement_id) {
        auto msg = vtr::string_fmt("Placement file %s specified in the routing file"
                                   " does not match the loaded placement (ID %s != %s)",
                                   header[1].c_str(), header[3].c_str(), place_ctx.placement_id.c_str());
        if (verify_file_digests) {
            vpr_throw(VPR_ERROR_ROUTE, route_file, lineno, msg.c_str());
        } else {
            VTR_LOGF_WARN(route_file, lineno, "%s\n", msg.c_str());
        }
    }

    /*Allocate necessary routing structures*/
    alloc_and_load_rr_node_route_structs();
    init_route_structs(router_opts.bb_factor);

    /*Check dimensions*/
    getline(fp, header_str);
    ++lineno;
    header.clear();
    header = vtr::split(header_str);
    if (header[0] == "Array" && header[1] == "size:" &&
            (vtr::atou(header[2].c_str()) != device_ctx.grid.width() || vtr::atou(header[4].c_str()) != device_ctx.grid.height())) {
        vpr_throw(VPR_ERROR_ROUTE, route_file, lineno,
                "Device dimensions %sx%s specified in the routing file does not match given %dx%d ",
                header[2].c_str(), header[4].c_str(), device_ctx.grid.width(), device_ctx.grid.height());
    }

    /* Read in every net */
    process_route(fp, route_file, lineno);

    fp.close();

    /*Correctly set up the clb opins*/
    recompute_occupancy_from_scratch();

    /* Note: This pres_fac is not necessarily correct since it isn't the first routing iteration*/
    pathfinder_update_cost(router_opts.initial_pres_fac, router_opts.acc_fac);

    reserve_locally_used_opins(router_opts.initial_pres_fac,
            router_opts.acc_fac, true);

    /* Finished loading in the routing, now check it*/
	recompute_occupancy_from_scratch();
    bool is_feasible = feasible_routing();
    if (is_feasible) {
        check_route(router_opts.route_type, device_ctx.num_rr_switches);
    }
    get_serial_num();

    VTR_LOG("Finished loading route file\n");

    return is_feasible;
}

static void process_route(ifstream &fp, const char* filename, int& lineno) {
    /*Walks through every net and add the routing appropriately*/
    string input;
    std::vector<std::string> tokens;
    while (getline(fp, input)) {
        ++lineno;
        tokens.clear();
        tokens = vtr::split(input);
        if (tokens.empty()) {
            continue; //Skip blank lines
        } else if (tokens[0][0] == '#') {
            continue; //Skip commented lines
        } else if (tokens[0] == "Net") {
            ClusterNetId inet(atoi(tokens[1].c_str()));
            process_nets(fp, inet, tokens[2], tokens, filename, lineno);
        }
    }

    tokens.clear();
}

static void process_nets(ifstream &fp, ClusterNetId inet, string name, std::vector<std::string> input_tokens, const char* filename, int& lineno) {
    /* Check if the net is global or not, and process appropriately */
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();

    if (input_tokens.size() > 3 && input_tokens[3] == "global"
            && input_tokens[4] == "net" && input_tokens[5] == "connecting:") {
        /* Global net.  Never routed. */
        if (!cluster_ctx.clb_nlist.net_is_global(inet)) {
            vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                    "Net %lu should be a global net", size_t(inet));
        }
        /*erase an extra colon for global nets*/
        name.erase(name.end() - 1);
        name = format_name(name);

        if (0 != cluster_ctx.clb_nlist.net_name(inet).compare(name)) {
            vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                    "Net name %s for net number %lu specified in the routing file does not match given %s",
                    name.c_str(), size_t(inet), cluster_ctx.clb_nlist.net_name(inet).c_str());
        }

        process_global_blocks(fp, inet, filename, lineno);
    } else {
        /* Not a global net */
        if (cluster_ctx.clb_nlist.net_is_global(inet)) {
            VTR_LOG_WARN( "Net %lu (%s) is marked as global in the netlist, but is non-global in the .route file\n", size_t(inet), cluster_ctx.clb_nlist.net_name(inet).c_str());
        }

        name = format_name(name);

        if (0 != cluster_ctx.clb_nlist.net_name(inet).compare(name)) {
            vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                    "Net name %s for net number %lu specified in the routing file does not match given %s",
                    name.c_str(), size_t(inet), cluster_ctx.clb_nlist.net_name(inet).c_str());
        }

        process_nodes(fp, inet, filename, lineno);
    }
    input_tokens.clear();
    return;
}

static void process_nodes(ifstream & fp, ClusterNetId inet, const char* filename, int& lineno) {
    /* Not a global net. Goes through every node and add it into trace.head*/

    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& route_ctx = g_vpr_ctx.mutable_routing();
    auto& place_ctx = g_vpr_ctx.placement();

    t_trace *tptr = route_ctx.trace[inet].head;

    /*remember the position of the last line in order to go back*/
    streampos oldpos = fp.tellg();
    int inode, x, y, x2, y2, ptc, switch_id, offset;
    int node_count = 0;
    string input;
    std::vector<std::string> tokens;

    /*Walk through every line that begins with Node:*/
    while (getline(fp, input)) {
        ++lineno;

        tokens.clear();
        tokens = vtr::split(input);

        if (tokens.empty()) {
            continue; /*Skip blank lines*/
        } else if (tokens[0][0] == '#') {
            continue; /*Skip commented lines*/
        } else if (tokens[0] == "Net") {
            /*End of the nodes list,
             *  return by moving the position of next char of input stream to be before net*/
            fp.seekg(oldpos);
            return;
        } else if (input == "\n\nUsed in local cluster only, reserved one CLB pin\n\n") {
            if (cluster_ctx.clb_nlist.net_sinks(inet).size() != 0) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "Net %d should be used in local cluster only, reserved one CLB pin");
            }
            return;
        } else if (tokens[0] == "Node:") {
            /*An actual line, go through each node and add it to the route tree*/
            inode = atoi(tokens[1].c_str());
            auto& node = device_ctx.rr_nodes[inode];

            /*First node needs to be source. It is isolated to correctly set heap head.*/
            if (node_count == 0 && tokens[2] != "SOURCE") {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "First node in routing has to be a source type");
            }

            /*Check node types if match rr graph*/
            if (tokens[2] != node.type_string()) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "Node %d has a type that does not match the RR graph", inode);
            }

            format_coordinates(x, y, tokens[3], inet, filename, lineno);

            if (tokens[4] == "to") {
                format_coordinates(x2, y2, tokens[5], inet, filename, lineno);
                if (node.xlow() != x || node.xhigh() != x2 || node.yhigh() != y2 || node.ylow() != y) {
                    vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                            "The coordinates of node %d does not match the rr graph", inode);
                }
                offset = 2;
            } else {
                if (node.xlow() != x || node.xhigh() != x || node.yhigh() != y || node.ylow() != y) {
                    vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                            "The coordinates of node %d does not match the rr graph", inode);
                }
                offset = 0;
            }

            /* Verify types and ptc*/
            if (tokens[2] == "SOURCE" || tokens[2] == "SINK" || tokens[2] == "OPIN" || tokens[2] == "IPIN") {
                if (tokens[4 + offset] == "Pad:" && !is_io_type(device_ctx.grid[x][y].type)) {
                    vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                            "Node %d is of the wrong type", inode);
                }
            } else if (tokens[2] == "CHANX" || tokens[2] == "CHANY") {
                if (tokens[4 + offset] != "Track:") {
                    vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                            "A %s node have to have track info", tokens[2].c_str());
                }
            }

            ptc = atoi(tokens[5 + offset].c_str());
            if (node.ptc_num() != ptc) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "The ptc num of node %d does not match the rr graph", inode);
            }

            /*Process switches and pb pin info if it is ipin or opin type*/
            if (tokens[6 + offset] != "Switch:") {
                /*This is an opin or ipin, process its pin nums*/
                if (!is_io_type(device_ctx.grid[x][y].type) && (tokens[2] == "IPIN" || tokens[2] == "OPIN")) {
                    int pin_num = device_ctx.rr_nodes[inode].ptc_num();
                    int height_offset = device_ctx.grid[x][y].height_offset;
                    ClusterBlockId iblock = place_ctx.grid_blocks[x][y - height_offset].blocks[0];
                    t_pb_graph_pin *pb_pin = get_pb_graph_node_pin_from_block_pin(iblock, pin_num);
                    t_pb_type *pb_type = pb_pin->parent_node->pb_type;

                    string pb_name, port_name;
                    int pb_pin_num;

                    format_pin_info(pb_name, port_name, pb_pin_num, tokens[6 + offset]);

                    if (pb_name != pb_type->name || port_name != pb_pin->port->name ||
                            pb_pin_num != pb_pin->pin_number) {
                        vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                                "%d node does not have correct pins", inode);
                    }
                } else {
                    vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                            "%d node does not have correct pins", inode);
                }
                switch_id = atoi(tokens[8 + offset].c_str());
            } else {
                switch_id = atoi(tokens[7 + offset].c_str());
            }

            /* Allocate and load correct values to trace.head*/
            if (node_count == 0) {
                route_ctx.trace[inet].head = alloc_trace_data();
                route_ctx.trace[inet].head->index = inode;
                route_ctx.trace[inet].head->iswitch = switch_id;
                route_ctx.trace[inet].head->next = nullptr;
                tptr = route_ctx.trace[inet].head;
                node_count++;
            } else {
                tptr->next = alloc_trace_data();
                tptr = tptr -> next;
                tptr->index = inode;
                tptr->iswitch = switch_id;
                tptr->next = nullptr;
                node_count++;
            }
        }
        /*stores last line so can easily go back to read*/
        oldpos = fp.tellg();
    }
}

/*This function goes through all the blocks in a global net and verify it with the
* clustered netlist and the placement */
static void process_global_blocks(ifstream &fp, ClusterNetId inet, const char* filename, int& lineno) {
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    auto& place_ctx = g_vpr_ctx.placement();

    string block, bnum_str;
    int x, y;
    std::vector<std::string> tokens;
    int pin_counter = 0;

    streampos oldpos = fp.tellg();
    /*Walk through every block line*/
    while (getline(fp, block)) {
        ++lineno;
        tokens.clear();
        tokens = vtr::split(block);

        if (tokens.empty()) {
            continue; /*Skip blank lines*/
        } else if (tokens[0][0] == '#') {
            continue; /*Skip commented lines*/
        } else if (tokens[0] != "Block") {
            /*End of blocks, go back to reading nets*/
            fp.seekg(oldpos);
            return;
        } else {
            format_coordinates(x, y, tokens[4], inet, filename, lineno);

            /*remove ()*/
            bnum_str = format_name(tokens[2]);
            /*remove #*/
            bnum_str.erase(bnum_str.begin());
			ClusterBlockId bnum(atoi(bnum_str.c_str()));

            /*Check for name, coordinate, and pins*/
            if (0 != cluster_ctx.clb_nlist.block_name(bnum).compare(tokens[1])) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "Block %s for block number %lu specified in the routing file does not match given %s",
                        tokens[1].c_str(), size_t(bnum), cluster_ctx.clb_nlist.block_name(bnum).c_str());
            }
            if (place_ctx.block_locs[bnum].x != x || place_ctx.block_locs[bnum].y != y) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "The placement coordinates (%d, %d) of %d block does not match given (%d, %d)",
                        x, y, place_ctx.block_locs[bnum].x, place_ctx.block_locs[bnum].y);
            }

			int pin_index = cluster_ctx.clb_nlist.net_pin_physical_index(inet, pin_counter);
            if (cluster_ctx.clb_nlist.block_type(bnum)->pin_class[pin_index] != atoi(tokens[7].c_str())) {
                vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                        "The pin class %d of %lu net does not match given ",
                        atoi(tokens[7].c_str()), size_t(inet), cluster_ctx.clb_nlist.block_type(bnum)->pin_class[pin_index]);
            }
            pin_counter++;
        }
        oldpos = fp.tellg();
    }
}

static void format_coordinates(int &x, int &y, string coord, ClusterNetId net, const char* filename, const int lineno) {
    /*Parse coordinates in the form of (x,y) into correct x and y values*/
    coord = format_name(coord);
    stringstream coord_stream(coord);
    if (!(coord_stream >> x)) {

        vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                "Net %lu has coordinates that is not in the form (x,y)", size_t(net));
    }
    coord_stream.ignore(1, ' ');
    if (!(coord_stream >> y)) {

        vpr_throw(VPR_ERROR_ROUTE, filename, lineno,
                "Net %lu has coordinates that is not in the form (x,y)", size_t(net));
    }
}

static void format_pin_info(string &pb_name, string & port_name, int & pb_pin_num, string input) {
    /*Parse the pin info in the form of pb_name.port_name[pb_pin_num]
     *into its appropriate variables*/
    stringstream pb_info(input);
    getline(pb_info, pb_name, '.');
    getline(pb_info, port_name, '[');
    pb_info >> pb_pin_num;
    if (!pb_info) {
        vpr_throw(VPR_ERROR_ROUTE, __FILE__, __LINE__,
                "Format of this pin info %s is incorrect",
                input.c_str());
    }
}

/*Return actual name by extracting it out of the form of (name)*/
static string format_name(string name) {
    if (name.length() > 2) {
        name.erase(name.begin());
        name.erase(name.end() - 1);
        return name;
    } else {
        vpr_throw(VPR_ERROR_ROUTE, __FILE__, __LINE__,
                "%s should be enclosed by parenthesis",
                name.c_str());
        return nullptr;
    }
}