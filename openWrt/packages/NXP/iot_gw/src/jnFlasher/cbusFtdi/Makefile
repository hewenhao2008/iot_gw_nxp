# ------------------------------------------------------------------
# FTDI CBUS Control makefile
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2015. All rights reserved
# ------------------------------------------------------------------

ifeq ($(TARGET_OS),RASPBIAN)
LDFLAGS += -lusb-1.0
LIBS = ../libftdi1-1.2/build/src/libftdi1.a
INCLUDES = -I../libftdi1-1.2/src
else
#LDFLAGS += -L/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/
# LDFLAGS += -lusb-1.0 -lftdi
# LDFLAGS += -l/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/libusb-1.0
# LDFLAGS += -l/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/libftdi
#LIBS = -l/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/libusb-1.0
#LIBS += -l/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/libftdi
LIBS = -lftdi -lusb -lusb-1.0
#LIBS = /home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/lib/libftdi.a
#LIBS = -lusb
#LIBS = -lusb-1.0

INCLUDES = -I$(STAGING_DIR)/usr/include
#INCLUDES = -I/home/ruud/openwrt/staging_dir/target-arm_arm1176jzf-s+vfp_uClibc-0.9.33.2_eabi/usr/include
endif

TARGET = iot_jf

OBJECTS = jf_main.o


%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -Wall -g -c $< -o $@


all: clean build

build: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET) -lc $(LIBS)
	cp $(TARGET) ../../swupdate/images/usr/bin/

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
	-rm -f ../../swupdate/images/usr/bin/$(TARGET)

