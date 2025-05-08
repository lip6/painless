# Define the main output files
# ===========================
PAINLESS_OUTPUT := painless
DEBUG_OUTPUT := $(PAINLESS_OUTPUT)_debug
RELEASE_OUTPUT := $(PAINLESS_OUTPUT)_release

# Compiler and flags
# ==================
CC := gcc
CXX := g++
COMMON_FLAGS := $(shell mpic++ --showme:compile) -fopenmp

# Check GCC version to determine proper C++ standard flag
GCC_MAJOR := $(shell gcc -dumpversion)

# Set appropriate C++20 flag based on GCC version
ifeq ($(shell expr $(GCC_MAJOR) \< 8), 1)
    $(error GCC version $(gcc --version) does not support C++20. Version 8 or higher required.)
else ifeq ($(shell expr $(GCC_MAJOR) \< 10), 1)
    # GCC 8 or 9 uses -std=c++2a
    CPP_STD_FLAG := -std=c++2a
else
    # GCC 10+ uses -std=c++20
    CPP_STD_FLAG := -std=c++20
endif

# Add standard flag to common flags
COMMON_FLAGS += $(CPP_STD_FLAG)

# Define debug and release flags 
DEBUG_FLAGS := $(COMMON_FLAGS) -g3 -O0 #-fsanitize=thread#-Wall -Wextra
RELEASE_FLAGS := $(COMMON_FLAGS) -O3 -DNDEBUG


# Directories
# ===========
SRC_DIR := src
SOLVERS_DIR := solvers
LIBS_DIR := libs
BUILD_DIR := build
DEBUG_BUILD_DIR := $(BUILD_DIR)/debug
RELEASE_BUILD_DIR := $(BUILD_DIR)/release

# Solver and library directories
# ==============================
MAPLE_BUILD := $(SOLVERS_DIR)/mapleCOMSPS/build/release/lib
GLUCOSE_BUILD := $(SOLVERS_DIR)/glucose/parallel
MINISAT_BUILD := $(SOLVERS_DIR)/minisat/build/release/lib
LINGELING_BUILD := $(SOLVERS_DIR)/lingeling
YALSAT_BUILD := $(SOLVERS_DIR)/yalsat
TASSAT_BUILD := $(SOLVERS_DIR)/tassat
KISSAT_BUILD := $(SOLVERS_DIR)/kissat/build
CADICAL_BUILD := $(SOLVERS_DIR)/cadical/build
KISSATMAB_BUILD := $(SOLVERS_DIR)/kissat_mab/build
KISSATINC_BUILD := $(SOLVERS_DIR)/kissat-inc/build
KISSATGASPI_BUILD := $(SOLVERS_DIR)/solvers/GASPIKISSAT/build

M4RI_DIR := $(LIBS_DIR)/m4ri-20200125

# Define dependencies
# ===================
DEPENDENCIES := $(MINISAT_BUILD)/libminisat.a \
                $(GLUCOSE_BUILD)/libglucose.a \
                $(LINGELING_BUILD)/liblgl.a \
                $(KISSAT_BUILD)/libkissat.a \
                $(KISSATMAB_BUILD)/libkissat_mab.a \
				$(KISSATINC_BUILD)/libkissat_inc.a \
                $(YALSAT_BUILD)/libyals.a \
				$(TASSAT_BUILD)/libtas.a \
                $(CADICAL_BUILD)/libcadical.a \
                $(MAPLE_BUILD)/libmapleCOMSPS.a \
                $(M4RI_DIR)/.libs/libm4ri.a

# Library flags
# =============
LIBS := -l:liblgl.a -L$(LINGELING_BUILD) \
		-l:libyals.a -L$(YALSAT_BUILD) \
		-l:libtas.a -L$(TASSAT_BUILD) \
		-l:libkissat.a -L$(KISSAT_BUILD) \
		-l:libminisat.a -L$(MINISAT_BUILD) \
		-l:libglucose.a -L$(GLUCOSE_BUILD) \
		-l:libcadical.a -L$(CADICAL_BUILD) \
		-l:libmapleCOMSPS.a -L$(MAPLE_BUILD) \
		-l:libkissat_mab.a -L$(KISSATMAB_BUILD) \
		-l:libkissat_inc.a -L$(KISSATINC_BUILD) \
		-l:libm4ri.a -L./libs/m4ri-20200125/.libs \
		-lpthread -lz -lm $(shell mpic++ --showme:link)
# -l:libgkissat.a -L$(KISSATGASPI_BUILD) \

# Include directories
# ===================
INCLUDES := -I$(SRC_DIR) \
            -I$(SOLVERS_DIR) \
            -I$(SOLVERS_DIR)/glucose \
            -I$(SOLVERS_DIR)/minisat \
            -I$(LIBS_DIR)/eigen-3.4.0 \
            -I$(M4RI_DIR)

# Source files
# ============
SRCS := $(shell find $(SRC_DIR) -name "*.cpp" -not -path "*/.ignore/*")
DEBUG_OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(RELEASE_BUILD_DIR)/%.o)

# All target
# ==============
.PHONY: all
all:
	$(MAKE) m4ri && $(MAKE) solvers && $(MAKE) painless

.DEFAULT_GOAL := all

# Painless target
# ==============
.PHONY: painless debug release

painless: debug release

# Create build directories
# ========================
$(shell mkdir -p $(DEBUG_BUILD_DIR) $(RELEASE_BUILD_DIR))

# Main targets
# ============
debug: $(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT)
	ln -sf $(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT) painlessd

release: $(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT)
	ln -sf $(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT) painless

$(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT): $(DEBUG_OBJS) $(DEPENDENCIES)
	$(CXX) -o $@ $(DEBUG_OBJS) $(DEBUG_FLAGS) $(INCLUDES) $(LIBS)

$(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT): $(RELEASE_OBJS) $(DEPENDENCIES)
	$(CXX) -o $@ $(RELEASE_OBJS) $(RELEASE_FLAGS) $(INCLUDES) $(LIBS)

# Pattern rules for object files
# ==============================
$(DEBUG_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(DEBUG_FLAGS) $(INCLUDES)

$(RELEASE_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(RELEASE_FLAGS) $(INCLUDES)

# Simplified library targets
# ==========================
.PHONY: minisat glucose lingeling kissat kissat_mab kissat_inc kissat_gaspi yalsat cadical maple m4ri tassat

solvers: minisat glucose lingeling kissat kissat_mab kissat_inc kissat_gaspi yalsat cadical maple tassat

libs: m4ri

minisat: $(MINISAT_BUILD)/libminisat.a
glucose: $(GLUCOSE_BUILD)/libglucose.a
lingeling: $(LINGELING_BUILD)/liblgl.a
kissat: $(KISSAT_BUILD)/libkissat.a
kissat_mab: $(KISSATMAB_BUILD)/libkissat_mab.a
kissat_inc: $(KISSATINC_BUILD)/libkissat_inc.a
# kissat_gaspi: $(KISSATGASPI_BUILD)/libgkissat.a
yalsat: $(YALSAT_BUILD)/libyals.a
tassat: $(TASSAT_BUILD)/libtas.a
cadical: $(CADICAL_BUILD)/libcadical.a
maple: $(MAPLE_BUILD)/libmapleCOMSPS.a
m4ri: $(M4RI_DIR)/.libs/libm4ri.a

# Library targets
# ===============
$(MINISAT_BUILD)/libminisat.a:
	$(MAKE) -C $(SOLVERS_DIR)/minisat

$(GLUCOSE_BUILD)/libglucose.a:
	cd $(SOLVERS_DIR)/glucose && $(MAKE) parallel/libglucose.a

$(LINGELING_BUILD)/liblgl.a: $(YALSAT_BUILD)/libyals.a
	cd $(SOLVERS_DIR)/lingeling && ./configure.sh
	$(MAKE) -C $(SOLVERS_DIR)/lingeling liblgl.a

$(KISSAT_BUILD)/libkissat.a:
	cd $(SOLVERS_DIR)/kissat && bash ./configure --no-proofs
	$(MAKE) -C $(SOLVERS_DIR)/kissat

$(KISSATMAB_BUILD)/libkissat_mab.a:
	cd $(SOLVERS_DIR)/kissat_mab && bash ./configure --no-proofs
	$(MAKE) -C $(SOLVERS_DIR)/kissat_mab

$(KISSATINC_BUILD)/libkissat_inc.a:
	cd $(SOLVERS_DIR)/kissat-inc && bash ./configure --no-proofs
	$(MAKE) -C $(SOLVERS_DIR)/kissat-inc

# $(KISSATGASPI_BUILD)/libgkissat.a:
# 	cd $(SOLVERS_DIR)/GASPIKISSAT && bash ./configure
# 	$(MAKE) -C $(SOLVERS_DIR)/GASPIKISSAT

$(YALSAT_BUILD)/libyals.a:
	cd $(SOLVERS_DIR)/yalsat && bash ./configure.sh
	$(MAKE) -C $(SOLVERS_DIR)/yalsat

$(TASSAT_BUILD)/libtas.a:
	cd $(SOLVERS_DIR)/tassat && bash ./configure.sh
	$(MAKE) -C $(SOLVERS_DIR)/tassat

$(CADICAL_BUILD)/libcadical.a:
	cd $(SOLVERS_DIR)/cadical && bash ./configure
	$(MAKE) -C $(SOLVERS_DIR)/cadical

$(MAPLE_BUILD)/libmapleCOMSPS.a:
	$(MAKE) -C $(SOLVERS_DIR)/mapleCOMSPS r

$(M4RI_DIR)/.libs/libm4ri.a:
	cd $(M4RI_DIR) && autoreconf --install && ./configure --enable-thread-safe
	$(MAKE) -C $(M4RI_DIR)

# Clean targets
# =============
.PHONY: clean cleanpainless cleansolvers clean cleanall
cleanpainless:
	rm -rf $(BUILD_DIR)
	rm -rf painless painlessd

cleansolvers:
	$(MAKE) clean -C $(SOLVERS_DIR)/kissat
	$(MAKE) clean -C $(SOLVERS_DIR)/kissat_mab
	$(MAKE) clean -C $(SOLVERS_DIR)/kissat-inc
	# $(MAKE) clean -C $(SOLVERS_DIR)/GASPIKISSAT
	$(MAKE) clean -C $(SOLVERS_DIR)/cadical
	$(MAKE) clean -C $(SOLVERS_DIR)/mapleCOMSPS
	$(MAKE) clean -C $(SOLVERS_DIR)/minisat
	$(MAKE) clean -C $(SOLVERS_DIR)/glucose
	$(MAKE) clean -C $(SOLVERS_DIR)/yalsat
	$(MAKE) clean -C $(SOLVERS_DIR)/tassat
	$(MAKE) -C $(SOLVERS_DIR)/lingeling clean

clean: cleanpainless cleansolvers

cleanall: clean
	$(MAKE) -C $(M4RI_DIR) clean

.PHONY: clean
