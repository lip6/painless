.DEFAULT_GOAL := all

# Protobuf
PROTO_SRCS := $(shell find . -name "*.proto" -not -path "./.ignores/*")
PROTO_DIR := ./protobuf/
PROTO_CC = $(addsuffix .pb.cc, $(basename $(PROTO_SRCS)))
PROTO_H = $(addsuffix .pb.h, $(basename $(PROTO_SRCS)))
PROTO_OBJS = $(addsuffix .pb.o, $(basename $(PROTO_SRCS)))

SRCS := $(shell find . -name "*.cpp" -not -path "./.ignores/*")
OBJS = $(addsuffix .o, $(basename $(SRCS)))

EXEC = painless

YALSAT_BUILD := ../solvers/yalsat
KISSAT_BUILD := ../solvers/kissat_mab/build
KISSATGASPI_BUILD := ../solvers/GASPIKISSAT/build

CXX := g++#mpic++
STATIC_FLAGS := #-static-libstdc++ -static-libgcc

LIBS := -l:libyals.a -L$(YALSAT_BUILD) \
       -l:libkissat.a -L$(KISSAT_BUILD) \
       -l:libgkissat.a -L$(KISSATGASPI_BUILD) \
       -l:libmapleCOMSPS.a -L../solvers/mapleCOMSPS/build/release/lib/ \
       -l:libm4ri.a -L../libs/m4ri-20200125/.libs \
       -lpthread -lz -lm -lprotobuf $(shell mpic++ --showme:link)

CXXFLAGS := -I. \
           -I../solvers \
           -I../libs/eigen-3.4.0 \
           -I../libs/m4ri-20200125 \
           $(shell mpic++ --showme:compile)\
           -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS \
           -fopenmp $(STATIC_FLAGS) -std=c++17 -O3 -DNDEBUG#-O0 -g3 #-O3 -DNDEBUG#-fsanitize=address#-fsanitize=thread #

# Rule to generate protocol buffer files
$(PROTO_CC) $(PROTO_H): $(PROTO_SRCS)
	@echo "Compiling Protocol Buffers files"
	protoc -I$(PROTO_DIR) --cpp_out=$(PROTO_DIR) $<
	@echo "Protocol Buffers compilation complete"

# Compile object files
%.pb.o: %.pb.cc $(PROTO_CC)
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

%.o: %.cpp $(PROTO_H)
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

$(EXEC): $(OBJS) $(PROTO_OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

.PHONY: all clean

all: $(PROTO_CC) $(PROTO_H) $(EXEC)

clean:
	rm -f $(OBJS) $(EXEC) $(PROTO_CC) $(PROTO_H) $(PROTO_OBJS)
