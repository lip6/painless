# Define the main output files
# ===========================
PAINLESS_OUTPUT := painless
DEBUG_OUTPUT := $(PAINLESS_OUTPUT)_debug
RELEASE_OUTPUT := $(PAINLESS_OUTPUT)_release
LIB_OUTPUT := libpainless.so
DEBUG_LIB_OUTPUT := $(LIB_OUTPUT).debug
RELEASE_LIB_OUTPUT := $(LIB_OUTPUT)
VERSION_SCRIPT := libpainless.map

# Compiler and flags
# ==================
CC := gcc
CXX := g++
COMMON_FLAGS := 

# Check if mpic++ is available and user wants distributed mode
USE_DIST ?= $(shell command -v mpic++ 2>/dev/null)

# Check if USE_DIST is disabled (0 or empty)
ifeq ($(USE_DIST),0)
    $(warning Distributed mode disabled by user, compiling in NDIST mode)
    DIST_FLAG := -DNDIST
else ifeq ($(USE_DIST),)
    $(warning mpic++ was not found, compiling in NDIST mode)
    DIST_FLAG := -DNDIST
else
    DIST_FLAG := $(shell mpic++ --showme:compile)
endif

COMMON_FLAGS += $(DIST_FLAG)

# Temp before making everything -fPIC
AR = ar              # The archiver program
ARFLAGS = rcs        # Archiver flags

# Check GCC version to determine proper C++ standard flag
GCC_MAJOR := $(shell gcc -dumpfullversion -dumpversion | cut -d. -f1)

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
COMMON_FLAGS += $(CPP_STD_FLAG) -fvisibility=hidden -fvisibility-inlines-hidden -DIPASIR_SHARED_LIB -DBUILDING_IPASIR_SHARED_LIB

# Define debug and release flags 
DEBUG_FLAGS := $(COMMON_FLAGS) -fPIC -g3 -O0 -DBUILDING_PAINLESS #-fanalyzer -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS#-fsanitize=undefined#-Wall -Wextra
RELEASE_FLAGS := $(COMMON_FLAGS) -fPIC -O3 -DNDEBUG -DBUILDING_PAINLESS


# Directories
# ===========
SRC_DIR := src
INCLUDE_DIR := include
SOLVERS_DIR := solvers
LIBS_DIR := libs
BUILD_DIR := build
DEBUG_BUILD_DIR := $(BUILD_DIR)/debug
RELEASE_BUILD_DIR := $(BUILD_DIR)/release
SCRIPTS_DIR := scripts/bin
INSTALL_DIR := ~/.local/bin

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
M4RI_DIR := $(LIBS_DIR)/m4ri-20200125

# Define dependencies
# ===================

# All made .so will be linked in libs for centralized library path
LIBS_DIR := libs

# M4RI is used by MapleCOMSPS
PAINLESS_DEPENDENCIES := $(GLUCOSE_BUILD)/libglucose.a \
                $(LINGELING_BUILD)/liblgl.a \
                $(YALSAT_BUILD)/libyals.a \
				$(TASSAT_BUILD)/libtas.a \
                $(MAPLE_BUILD)/libmapleCOMSPS.a \
				$(MINISAT_BUILD)/libminisat.a \
				$(LIBS_DIR)/libkissat.so \
				$(LIBS_DIR)/libcadical.so \
				$(LIBS_DIR)/libm4ri.so


# Library flags
# =============

LIBS := -Wl,--whole-archive \
		-l:liblgl.a -L$(LINGELING_BUILD) \
		-l:libyals.a -L$(YALSAT_BUILD) \
		-l:libtas.a -L$(TASSAT_BUILD) \
		-l:libglucose.a -L$(GLUCOSE_BUILD) \
		-l:libminisat.a -L$(MINISAT_BUILD)\
		-l:libmapleCOMSPS.a -L$(MAPLE_BUILD)\
		-Wl,--no-whole-archive \
		-L$(LIBS_DIR) -l:libkissat.so -l:libm4ri.so -l:libcadical.so \
		-lpthread -lz -lm $(shell mpic++ --showme:link)

# Include directories
# ===================
INCLUDES := -I$(INCLUDE_DIR) \
			-I$(SRC_DIR) \
            -I$(SOLVERS_DIR) \
            -I$(SOLVERS_DIR)/glucose \
            -I$(SOLVERS_DIR)/minisat \
            -I$(LIBS_DIR)/eigen-3.4.0 \
			-I$(M4RI_DIR)

# Source files
# ============
LIB_SRCS := $(shell find $(SRC_DIR) -name "*.cpp" -not -name "main.cpp" -not -path "*/.ignore/*")
MAIN_SRC := $(SRC_DIR)/main.cpp

DEBUG_LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(DEBUG_BUILD_DIR)/%.o)
RELEASE_LIB_OBJS := $(LIB_SRCS:$(SRC_DIR)/%.cpp=$(RELEASE_BUILD_DIR)/%.o)

DEBUG_MAIN_OBJ := $(DEBUG_BUILD_DIR)/main.o
RELEASE_MAIN_OBJ := $(RELEASE_BUILD_DIR)/main.o

# All target
# ==============
.PHONY: all
all: solvers painless m4ri

.DEFAULT_GOAL := all

# Painless target
# ==============
.PHONY: painless debug release lib lib-debug lib-release scripts install

lib: lib-debug lib-release

painless: debug release lib

# Create build directories
# ========================
$(shell mkdir -p $(DEBUG_BUILD_DIR) $(RELEASE_BUILD_DIR))

# Main targets
# ============
debug: $(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT)
	$(call epilogue,Painless Debug built)

release: $(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT)
	$(call epilogue,Painless Release built)

lib-debug: $(DEBUG_BUILD_DIR)/$(DEBUG_LIB_OUTPUT)
	$(call cecho_run,mv $< $(LIBS_DIR)/$(DEBUG_LIB_OUTPUT))
	$(call epilogue,Painless Debug Library built)

lib-release: $(RELEASE_BUILD_DIR)/$(RELEASE_LIB_OUTPUT)
	$(call cecho_run,mv $< $(LIBS_DIR)/$(RELEASE_LIB_OUTPUT))
	$(call epilogue,Painless Release Library built)

scripts: $(SCRIPTS_DIR)/painless $(SCRIPTS_DIR)/painlessd debug release
	$(call prologue,Updating wrapper scripts with absolute paths)
	$(call cecho_run,echo "BINARY='$(CURDIR)/$(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT)'" > $(SCRIPTS_DIR)/painless_bin_path)
	$(call cecho_run,echo "BINARY_DEBUG='$(CURDIR)/$(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT)'" >> $(SCRIPTS_DIR)/painless_bin_path)
	$(call cecho_run,chmod +x $(SCRIPTS_DIR)/painless $(SCRIPTS_DIR)/painlessd)
	$(call epilogue,Scripts configured for: $(CURDIR))


install: scripts
	$(call prologue,Installing wrapper scripts)
	$(call prologue,Make sure $(INSTALL_DIR) is in your PATH)
	@mkdir -p $(INSTALL_DIR)
	$(call cecho_run,ln -sf "$(CURDIR)/$(SCRIPTS_DIR)/painless" $(INSTALL_DIR)/painless)
	$(call cecho_run,ln -sf "$(CURDIR)/$(SCRIPTS_DIR)/painlessgdb" $(INSTALL_DIR)/painlessgdb)
	$(call cecho_run,ln -sf "$(CURDIR)/$(SCRIPTS_DIR)/painless_bin_path" $(INSTALL_DIR)/painless_bin_path)
	$(call epilogue,Scripts installed to $(INSTALL_DIR)/)

# In order to find the libraries at runtime
RPATH_FLAGS := -Wl,-rpath,'$$ORIGIN/../../$(LIBS_DIR)'
LIB_RPATH_FLAGS := -Wl,-rpath,'$$ORIGIN'
VERSION_SCRIPT_FLAGS := -Wl,--version-script=$(VERSION_SCRIPT)

$(DEBUG_BUILD_DIR)/$(DEBUG_OUTPUT): $(DEBUG_LIB_OBJS) $(DEBUG_MAIN_OBJ) $(PAINLESS_DEPENDENCIES)
	$(call cecho,Linking debug executable)
	@$(CXX) -o $@ $(DEBUG_LIB_OBJS) $(DEBUG_MAIN_OBJ) $(DEBUG_FLAGS) $(LIBS) $(RPATH_FLAGS)

$(RELEASE_BUILD_DIR)/$(RELEASE_OUTPUT): $(RELEASE_LIB_OBJS) $(RELEASE_MAIN_OBJ) $(PAINLESS_DEPENDENCIES)
	$(call cecho,Linking release executable)
	@$(CXX) -o $@ $(RELEASE_LIB_OBJS) $(RELEASE_MAIN_OBJ) $(RELEASE_FLAGS) $(LIBS) $(RPATH_FLAGS)


$(DEBUG_BUILD_DIR)/$(DEBUG_LIB_OUTPUT): $(DEBUG_LIB_OBJS) $(PAINLESS_DEPENDENCIES) $(VERSION_SCRIPT)
	$(call cecho,Linking debug shared library)
	@$(CXX) -o $@ $(DEBUG_LIB_OBJS) -shared $(DEBUG_FLAGS) $(LIBS) $(LIB_RPATH_FLAGS) $(VERSION_SCRIPT_FLAGS)
# 	$(AR) $(ARFLAGS) $@ $(DEBUG_LIB_OBJS)

$(RELEASE_BUILD_DIR)/$(RELEASE_LIB_OUTPUT): $(RELEASE_LIB_OBJS) $(PAINLESS_DEPENDENCIES) $(VERSION_SCRIPT)
	$(call cecho,Linking release shared library)
	@$(CXX) -o $@ $(RELEASE_LIB_OBJS) -shared $(RELEASE_FLAGS) $(LIBS) $(LIB_RPATH_FLAGS) $(VERSION_SCRIPT_FLAGS)
# 	$(AR) $(ARFLAGS) $@ $(RELEASE_LIB_OBJS)


# Pattern rules for object files
# ==============================
$(DEBUG_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo "  [DEBUG]   Compiling $(notdir $<)..."
	@$(CXX) -c $< -o $@ $(DEBUG_FLAGS) $(INCLUDES)

$(RELEASE_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo "  [RELEASE] Compiling $(notdir $<)..."
	@$(CXX) -c $< -o $@ $(RELEASE_FLAGS) $(INCLUDES)

# Simplified library targets
# ==========================
.PHONY: minisat glucose lingeling kissat  yalsat cadical tassat maple m4ri

solvers: minisat glucose lingeling kissat  yalsat cadical tassat maple


m4ri: $(LIBS_DIR)/libm4ri.so
kissat: $(LIBS_DIR)/libkissat.so
cadical: $(LIBS_DIR)/libcadical.so
minisat: $(MINISAT_BUILD)/libminisat.a
glucose: $(GLUCOSE_BUILD)/libglucose.a
lingeling: $(LINGELING_BUILD)/liblgl.a
maple: $(MAPLE_BUILD)/libmapleCOMSPS.a
yalsat: $(YALSAT_BUILD)/libyals.a
tassat: $(TASSAT_BUILD)/libtas.a


# Library targets
# ===============
include pretty-print.mk

$(LIBS_DIR)/libcadical.so:
	$(call prologue,CaDiCaL)
	$(call silent_build,cd $(SOLVERS_DIR)/cadical && bash ./configure -shared)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/cadical)
	$(call silent_build,ln -sf ../$(CADICAL_BUILD)/libcadical.so $(LIBS_DIR)/libcadical.so)
	$(call epilogue,CaDiCaL built)

$(LIBS_DIR)/libkissat.so:
	$(call prologue,Kissat)
	$(call silent_build,cd $(SOLVERS_DIR)/kissat && bash ./configure -shared)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/kissat)
	$(call silent_build,ln -sf ../$(KISSAT_BUILD)/libkissat.so $(LIBS_DIR)/libkissat.so)
	$(call epilogue,Kissat built)


$(LIBS_DIR)/libm4ri.so:
	$(call prologue,M4RI)
	$(call silent_build,cd $(M4RI_DIR) && autoreconf --install && ./configure --enable-thread-safe --with-cachesize=32768:262144:8388608 CFLAGS="-O2 -msse2")
	$(call silent_build,$(MAKE) -C $(M4RI_DIR))
	$(call silent_build,ln -sf ../$(M4RI_DIR)/.libs/libm4ri.so $(LIBS_DIR)/libm4ri.so)
	$(call silent_build,ln -sf ../$(M4RI_DIR)/.libs/libm4ri.so $(LIBS_DIR)/libm4ri-0.0.20200125.so)
	$(call epilogue,M4RI built)

$(GLUCOSE_BUILD)/libglucose.a:
	$(call prologue,Glucose)
	$(call silent_build,cd $(SOLVERS_DIR)/glucose/parallel && $(MAKE) libs COPTIMIZE="-O3 -fPIC -fvisibility=hidden" GLUCOSE_DEBUG_FLAGS="")
	$(call silent_build,cd $(SOLVERS_DIR)/glucose/parallel && mv lib.a libglucose.a)
	$(call epilogue,Glucose built)

$(LINGELING_BUILD)/liblgl.a:
	$(call prologue,Lingeling)
	$(call silent_build,cd $(SOLVERS_DIR)/lingeling && ./configure.sh --no-yalsat -fPIC -fvisibility=hidden)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/lingeling liblgl.a )
	$(call epilogue,Lingeling built)

$(YALSAT_BUILD)/libyals.a:
	$(call prologue,YalSAT)
	$(call silent_build,cd $(SOLVERS_DIR)/yalsat && bash ./configure.sh -f)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/yalsat libyals.a )
	$(call epilogue,YalSAT built)

$(TASSAT_BUILD)/libtas.a:
	$(call prologue,TaSSAT)
	$(call silent_build,cd $(SOLVERS_DIR)/tassat && bash ./configure.sh -f)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/tassat )
	$(call epilogue,TaSSAT built)

$(MINISAT_BUILD)/libminisat.a:
	$(call prologue,MiniSat)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/minisat MINISAT_RELSYM="-fPIC -fvisibility=hidden")
	$(call epilogue,MiniSat built)

$(MAPLE_BUILD)/libmapleCOMSPS.a: $(LIBS_DIR)/libm4ri.so
	$(call prologue,MapleCOMSPS)
	$(call silent_build,$(MAKE) -C $(SOLVERS_DIR)/mapleCOMSPS MAPLE_RELSYM="-fPIC -fvisibility=hidden")
	$(call epilogue,MapleCOMSPS built)


# Clean targets
# =============
.PHONY: clean cleanpainless cleansolvers cleanm4ri uninstall
cleanpainless:
	$(call prologue,Cleaning Painless)
	@rm -rf $(BUILD_DIR)
	@rm -rf $(LIBS_DIR)/libpainless.so $(LIBS_DIR)/libpainless.so.debug
	$(call epilogue,Painless Cleaned)

cleankissat:
	$(call prologue,Cleaning Kissat)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/kissat
	-@rm -rf $(LIBS_DIR)/libkissat.so
	$(call epilogue,Kissat Cleaning)

cleancadical:
	$(call prologue,Cleaning CaDiCaL)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/cadical
	-@rm -rf $(LIBS_DIR)/libcadical.so
	$(call epilogue,CaDiCaL Cleaning)

cleanglucose:
	$(call prologue,Cleaning Glucose)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/glucose
	$(call epilogue,Glucose Cleaning)

cleanyalsat:
	$(call prologue,Cleaning YalSAT)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/yalsat
	$(call epilogue,YalSAT Cleaning)

cleantassat:
	$(call prologue,Cleaning TasSAT)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/tassat
	$(call epilogue,TasSAT Cleaning)

cleanlingeling:
	$(call prologue,Cleaning Lingeling)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/lingeling
	$(call epilogue,Lingeling Cleaning)

cleanmaple:
	$(call prologue,Cleaning MapleCOMSPS)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/mapleCOMSPS
	$(call epilogue,MapleCOMSPS Cleaning)

cleanminisat:
	$(call prologue,Cleaning MiniSat)
	-@$(MAKE) clean -C $(SOLVERS_DIR)/minisat
	$(call epilogue,MiniSat Cleaning)

cleansolvers: cleanpainless cleancadical cleanglucose cleanyalsat cleantassat cleanlingeling cleanmaple cleankissat cleanminisat
	$(call prologue,Cleaning Backend Solvers)
	$(call epilogue,Solvers Cleaning)

cleanm4ri:
	$(call prologue,Cleaning M4RI)
	@$(MAKE) -C $(M4RI_DIR) clean
	@rm -rf $(LIBS_DIR)/libm4ri.so
	@rm -rf $(LIBS_DIR)/libm4ri-0.0.20200125.so
	$(call epilogue,M4RI Cleaned)

uninstall:
	$(call prologue,Uninstalling Painless)
	@rm -f $(INSTALL_DIR)/painless $(INSTALL_DIR)/painlessgdb $(INSTALL_DIR)/painless_bin_path
	@rm -f $(SCRIPTS_DIR)/painless_bin_path
	$(call epilogue,Uninstalling Painless from $(INSTALL_DIR)/)

clean: cleanpainless cleansolvers cleanm4ri

# Info target - Display build configuration
# =========================================
.PHONY: info show-config

info: show-config
infov: show-config-verbose

show-config:
	@echo "========================================"
	@echo "  Painless Build Configuration"
	@echo "========================================"
	@echo ""
	@echo "COMPILERS:"
	@echo "  CC              = $(CC)"
	@echo "  CXX             = $(CXX)"
	@echo "  GCC_MAJOR       = $(GCC_MAJOR)"
	@echo ""
	@echo "FLAGS:"
	@echo "  CPP_STD_FLAG    = $(CPP_STD_FLAG)"
	@echo "  DIST_FLAG       = $(DIST_FLAG)"
	@echo "  USE_DIST        = $(USE_DIST)"
	@echo "  DEBUG_FLAGS     = $(DEBUG_FLAGS)"
	@echo "  RELEASE_FLAGS   = $(RELEASE_FLAGS)"
	@echo ""
	@echo "DIRECTORIES:"
	@echo "  SRC_DIR         = $(SRC_DIR)"
	@echo "  INCLUDE_DIR     = $(INCLUDE_DIR)"
	@echo "  BUILD_DIR       = $(BUILD_DIR)"
	@echo "  DEBUG_BUILD     = $(DEBUG_BUILD_DIR)"
	@echo "  RELEASE_BUILD   = $(RELEASE_BUILD_DIR)"
	@echo "  SOLVERS_DIR     = $(SOLVERS_DIR)"
	@echo "  LIBS_DIR        = $(LIBS_DIR)"
	@echo "  SCRIPTS_DIR     = $(SCRIPTS_DIR)"
	@echo "  INSTALL_DIR     = $(INSTALL_DIR)"
	@echo ""
	@echo "TARGETS:"
	@echo "  DEBUG_OUTPUT    = $(DEBUG_OUTPUT)"
	@echo "  RELEASE_OUTPUT  = $(RELEASE_OUTPUT)"
	@echo "  DEBUG_LIB       = $(DEBUG_LIB_OUTPUT)"
	@echo "  RELEASE_LIB     = $(RELEASE_LIB_OUTPUT)"
	@echo ""
	@echo "INSTALLABLE EXECUTION SCRIPTS:"
	@echo "  painless        = $(SCRIPTS_DIR)/painless"
	@echo "  painlessd       = $(SCRIPTS_DIR)/painlessd"
	@echo ""
	@echo "DEPENDENCIES:"
	@$(foreach dep,$(PAINLESS_DEPENDENCIES),echo "  - $(dep)";)
	@echo ""
	@echo "LIBRARIES:"
	@echo "  LIBS = $(LIBS)"
	@echo ""
	@echo "INCLUDES:"
	@$(foreach inc,$(INCLUDES),echo "  $(inc)";)
	@echo ""
	@echo "SOURCE FILES:"
	@echo "  Total library sources: $(words $(LIB_SRCS))"
	@echo "  Main source: $(MAIN_SRC)"
	@echo ""
	@echo "OBJECT FILES:"
	@echo "  Debug objects:   $(words $(DEBUG_LIB_OBJS)) files"
	@echo "  Release objects: $(words $(RELEASE_LIB_OBJS)) files"
	@echo ""

show-config-verbose: show-config
	@echo "ALL SOURCE FILES:"
	@$(foreach src,$(LIB_SRCS),echo "  - $(src)";)
	@echo "  - $(MAIN_SRC)"
	@echo ""