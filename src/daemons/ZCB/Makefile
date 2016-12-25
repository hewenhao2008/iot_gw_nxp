# ------------------------------------------------------------------
# ZCB makefile
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------
#-floor/ceil/pow是数学库里提供的函数，默认gcc不会自动链接math库，要手动提供,加上-lm选项就好了
# LDFLAGS += -ldaemon -lpthread
LDFLAGS += -lpthread -lm

TARGET = iot_zb

INCLUDES = -I../../IotCommon -I../../IotCommon/cJSON

OBJECTS = zb_main.o \
	cmd.o \
	lmp.o \
	grp.o \
	lmpgrp.o \
	plg.o \
	ctrl.o \
	topo.o \
	tunnel.o \
	Serial.o \
	SerialLink.o \
	Utils.o \
	zcb.o \
	ZigbeeDevices.o \
	../../IotCommon/atoi.o \
	../../IotCommon/newDb.o \
	../../IotCommon/systemtable.o \
	../../IotCommon/iotError.o \
	../../IotCommon/iotSleep.o \
	../../IotCommon/fileCreate.o \
	../../IotCommon/iotSemaphore.o \
	../../IotCommon/socket.o \
	../../IotCommon/queue.o \
	../../IotCommon/parsing.o \
	../../IotCommon/colorConv.o \
	../../IotCommon/RgbSpaceMatrices.o \
	../../IotCommon/blackbody.o \
	../../IotCommon/jsonCreate.o \
	../../IotCommon/plugUsage.o \
	../../IotCommon/json.o \
	../../IotCommon/nibbles.o \
	../../IotCommon/dump.o \
	../../IotCommon/newLog.o \
	../../IotCommon/cJSON/cJSON.o

#-下面的实现编译
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -Wall -std=gnu99 -g -c $< -o $@


all: clean build
#-下面的实现链接
build: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET) -lc
	cp $(TARGET) ../../swupdate/images/usr/bin/

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
	-rm -f ../../swupdate/images/usr/bin/$(TARGET) 

