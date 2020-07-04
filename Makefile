SRC := $(wildcard src/*.cpp)

CROSS :=arm-hisiv500-linux-
#CROSS := arm-linux-androideabi-
#TARGET := libcircqueue_v100nptl.so

LIBRARY_PATH:=./lib/$(CROSS)

ifeq ($(Varago),Y)
export PATH:=$(PATH):/work/TI8127_COMPILE_V3.5/bin
CROSS := arm-arago-linux-gnueabi-
TARGET := libcircqueue_varago.so
endif

TARGET := librtspclient.so

DEFAULT_INCLUDES = -I./include -I ./include/live555/BasicUsageEnvironment -I ./include/live555/groupsock -I ./include/live555/liveMedia -I ./include/live555/UsageEnvironment -I./lib
LINK_FLAGS = -Os -Wall -L$(LIBRARY_PATH) -lliveMedia -lBasicUsageEnvironment -lgroupsock -lUsageEnvironment
CFLAGS = -Wall -Os -c -fPIC $(DEFAULT_INCLUDES)
CC = gcc
STRIP = strip
CROSS_COMPILE = $(CROSS)$(CC)
CROSS_STRIP = $(CROSS)$(STRIP)

OBJ := $(SRC:src/%.cpp=lib/%.o)
LINK := $(CROSS_COMPILE) $(LINK_FLAGS) -shared -o lib/$(CROSS)/$(TARGET) $(OBJ)
DO_STRIP := $(CROSS_STRIP) lib/$(CROSS)/$(TARGET)

AR:=$(CROSS)ar
ARFLAGS:=crv
LINK_AR := $(AR) $(ARFLAGS) lib/$(CROSS)/$(TARGET:.so=.a) $(OBJ)

all : lib/$(CROSS)/$(TARGET)

lib/%.o : src/%.cpp
	$(CROSS_COMPILE) $(CFLAGS) -c -o $@ $< $(HISI_INCLUDE) $(TD_INCLUDE)
	@echo
	
lib/$(CROSS)/$(TARGET) : $(OBJ)
	$(LINK)
	$(DO_STRIP)
	#$(LINK_AR)
	
clean :
	$(RM) $(OBJ) lib/*.o
