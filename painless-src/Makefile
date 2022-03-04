SRCS = $(shell find . -name "*.cpp")

OBJS = $(addsuffix .o, $(basename $(SRCS)))

EXEC = painless

LIBS = -lmapleCOMSPS -L../mapleCOMSPS/build/release/lib/ \
       -lm4ri -L../mapleCOMSPS/m4ri-20140914/.libs \
       -lslime -L../slime/build/release/lib/ \
       -lpthread -lz -lm -static

CXXFLAGS = -I../mapleCOMSPS -I../mapleCOMSPS/m4ri-20140914 \
           -I../mapleChronoBT -I../lstech -I../slime -I. \
           -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS \
           -std=c++11 -O3 -D NDEBUG

$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXEC)
