#
# Make options
#
#	CROSS_COMPILE	: define cross compiler (default arm-linux-gnueabihf-gcc)
#	DEBUG=y		: use debug build
#	DESTDIR=<path>	: set target dest path
#	LIBDIR=<path>	: set library path
#

CROSS_COMPILE := arm-linux-gnueabihf-

DESTDIR     := ../result
LIBDIR      := ../lib
INCDIR      := ../lib/include
EXT_CFLAGS  := -W -O2
EXT_LDFLAGS :=

ifeq ($(CC),cc)
CC  := $(CROSS_COMPILE)gcc
endif
STRIP  ?= $(CROSS_COMPILE)strip

CFLAGS  := $(EXT_CFLAGS)
LDFLAGS := $(EXT_LDFLAGS)

CFLAGS  += -I $(INCDIR)
LDFLAGS += -static

COBJS	:= i2c_test.o
OBJS	:= $(COBJS)

TARGET = i2c_test 

all : $(TARGET)

$(TARGET) : depend $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	mkdir -p $(DESTDIR)
	cp $@ $(DESTDIR)
ifneq ($(DEBUG),y)
	$(STRIP) $@
endif

.PHONY: clean

clean :
	rm -rf $(OBJS) $(TARGET) core .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

depend dep:
	$(CC)  -M $(CFLAGS) $(COBJS:.o=.c) > .depend

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
