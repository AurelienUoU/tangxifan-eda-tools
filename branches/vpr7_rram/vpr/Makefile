################################   MAKEFILE OPTIONS     ####################################

# If ENABLE_GRAPHICS is set to true, VPR requires X11 to compile.  
# On Ubuntu, type 'make packages' or 
# 'sudo apt-get install libx11-dev' to install.  
# Please look online for information on how to install X11 on other Linux distributions.

# Please note that a Mac can run the graphics if the X11 library is installed. 

ENABLE_GRAPHICS = false
# can be true or false

export BUILD_TYPE = debug
# can be debug or release - this option gets inherited by the library libarchfpga 
# by default, though libarchfpga's build type can be set independently in libarchfpga's Makefile.

COMPILER = g++

OPTIMIZATION_LEVEL_FOR_RELEASE_BUILD = -O3
# can be -O0 (no optimization) to -O3 (full optimization), or -Os (optimize space)

#############################################################################################

CC = $(COMPILER)
LIB_DIR = -L.
LIB = -lm -lvpr -lreadline
SHELL_LIB = -lm -lvpr_shell -lreadline
FPGA_SPICE_SRC_DIR = SRC/fpga_x2p
SRC_DIR = SRC
OBJ_DIR = OBJ
OTHER_DIR = -ISRC/util -ISRC/timing -ISRC/pack -ISRC/place -ISRC/base -ISRC/route -ISRC/power -ISRC/mrfpga -ISRC/fpga_spice -I$(FPGA_SPICE_SRC_DIR)/base -I$(FPGA_SPICE_SRC_DIR)/bitstream -I$(FPGA_SPICE_SRC_DIR)/bitstream -I$(FPGA_SPICE_SRC_DIR)/clb_pin_remap -I$(FPGA_SPICE_SRC_DIR)/spice -I$(FPGA_SPICE_SRC_DIR)/verilog  -I$(FPGA_SPICE_SRC_DIR)/router -I../libarchfpga -I../printhandler/SRC/TIO_InputOutputHandlers -I$(FPGA_SPICE_SRC_DIR)/shell

WARN_FLAGS = -Wall -Wpointer-arith -Wcast-qual -D__USE_FIXED_PROTOTYPES__ -ansi -pedantic -Wshadow -Wcast-align -D_POSIX_SOURCE -Wno-write-strings

DEBUG_FLAGS = -g 
OPT_FLAGS = $(OPTIMIZATION_LEVEL_FOR_RELEASE_BUILD)
INC_FLAGS = -I../libarchfpga/include -I../libarchfpga/fpga_spice_include

FLAGS = $(INC_FLAGS) $(WARN_FLAGS) -D EZXML_NOMMAP -D_POSIX_C_SOURCE

UNAME := $(shell uname)
# determine build env
ifeq ($(UNAME), Darwin)
	MAC_OS = true
else
	MAC_OS = false
endif

ifneq (,$(findstring release, $(BUILD_TYPE)))
  FLAGS := $(FLAGS) $(OPT_FLAGS)
else                              # DEBUG build
  FLAGS := $(FLAGS) $(DEBUG_FLAGS)
endif

ifneq (,$(findstring true, $(ENABLE_GRAPHICS)))
  # The following block defines the X11 directories. If X11 library
  # is located elsewhere, change it here.
  ifneq (,$(findstring true, $(MAC_OS)))
    LIB_DIR := $(LIB_DIR) -L/usr/X11/lib
	INC_FLAGS := $(INC_FLAGS) -I/opt/X11/include
  else  
    LIB_DIR := $(LIB_DIR) -L/usr/lib/X11
    PACKAGEINSTALL := if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libx11-dev -c >>/dev/null; then sudo apt-get install libx11-dev; fi; fi
    PACKAGENOTIFICATION := if cat /etc/issue | grep Ubuntu -c >>/dev/null; then if ! dpkg -l | grep libx11-dev -c >>/dev/null; then echo "\n\n\n\n*****************************************************\n* VPR has detected that graphics are enabled,       *\n* but the required graphics package libx11-dev      *\n* is missing. Try:                                  *\n* a) Type 'make packages' to install libx11-dev     *\n*    automatically if not already installed.        *\n* b) Type 'sudo apt-get install libx11-dev' to      *\n*    install manually.                              *\n* c) If libx11-dev is installed, point the Makefile *\n*    to where your X11 libraries are installed.     *\n* d) If you wish to run VPR without graphics, set   *\n*    the flag ENABLE_GRAPHICS = false at the top    *\n*    of the Makefile in VPR's parent directory.     *\n*****************************************************\n\n\n\n"; fi; fi
  endif
  LIB := $(LIB) -lX11
else
  FLAGS := $(FLAGS) -DNO_GRAPHICS
endif

EXE = vpr

OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c $(FPGA_SPICE_SRC_DIR)/*/*.c))
OBJ_DIRS=$(sort $(dir $(OBJ)))
DEP := $(OBJ:.o=.d)

# notify is an order-only prerequisite - note the "|"
$(EXE): $(OBJ) Makefile libvpr.a | notify
	$(CC) $(FLAGS) OBJ/main.o -o $(EXE) $(LIB_DIR) $(LIB)

SHELL_EXE = vpr_shell

# cmd-shell interface main
$(SHELL_EXE): $(OBJ) Makefile libvpr_shell.a | notify
	$(CC) $(FLAGS) OBJ/shell_main.o -o $(SHELL_EXE) $(LIB_DIR) $(SHELL_LIB)

# if graphics enabled but libx11-dev is not installed, notify the user
notify:
	@ $(PACKAGENOTIFICATION) 

#if graphics enabled but libx11-dev is not installed, install it
packages: 
	@ $(PACKAGEINSTALL)

libarchfpga:
	@ cd ../libarchfpga && make


libvpr.a: $(OBJ) Makefile libarchfpga
	@ cp ../libarchfpga/libarchfpga.a $@
	@ ar rcs $@ $(OBJ)
	@ ar d $@ main.o

libvpr_shell.a: $(OBJ) Makefile libarchfpga
	@ cp ../libarchfpga/libarchfpga.a $@
	@ ar rcs $@ $(OBJ)
	@ ar d $@ shell_main.o

# Enable a second round of expansion so that we may include
# the target directory as a prerequisite of the object file.
.SECONDEXPANSION:

# The directory follows a "|" to use an existence check instead of the usual
# timestamp check.  Every write to the directory updates the timestamp thus
# without this, all but the last file written to a directory would appear
# to be out of date.
$(OBJ): OBJ/%.o:$(SRC_DIR)/%.c | $$(dir $$@D) 
	$(CC) $(FLAGS) -MD -MP -I$(OTHER_DIR) -ISRC/util -c $< -o $@

# Silently create target directories as need
$(OBJ_DIRS):
	@ mkdir -p $@

-include $(DEP)

.PHONY: all vpr shell_vpr

all: $(EXE) $(SHELL_EXE)

vpr: $(EXE)

shell_vpr: $(SHELL_EXE)

clean:
	rm -f $(SHELL_EXE) $(EXE) $(OBJ) $(DEP) libvpr.a libvpr_shell.a 
	cd ../libarchfpga && make clean

clean_coverage: clean
	rm -rf ./usr
	find ./OBJ -regex ".*.\(gcda\|gcno\)" -exec rm -f {} \;
	rm -f *.html
	find ./SRC -iname "*.html" -exec rm -f {} \;


ctags:
	cd $(SRC_DIR) && ctags *.[ch]

# This is using Target-specific variable values. See: http://www.gnu.org/software/make/manual/make.html#Target_002dspecific
coverage: FLAGS = $(DEBUG_FLAGS) $(INC_FLAGS) -pedantic  -D EZXML_NOMMAP -fprofile-arcs -ftest-coverage -lgcov
coverage: $(EXE)
	./coverage_reset.sh
