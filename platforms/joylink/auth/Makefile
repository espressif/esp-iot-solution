
MODULE = obj-$(shell pwd | sed "s/.*\///" ).o
all: $(MODULE)

#include $(TOPDIR)/rules.mak
include ../../rules.mak

#
# Set CFLAGS or EXTRA_CFLAGS
#
EXTRA_CFLAGS = -I$(shell cd .. ; pwd)/include 
#-Werror


#
# Enumerate files to compile
#

obj-y += crc.o joylinkAES.o md5.o uECC.o

ALL_OBJS += $(obj-y)

#
# Leave rule to make
#
$(MODULE): $(ALL_OBJS)
	$(XLD) -r $(ENDIAN) -o $(MODULE) $(ALL_OBJS)
clean:
	find . -name '*.[od]' -type f -print | xargs rm -f
