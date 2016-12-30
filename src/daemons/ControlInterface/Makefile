# ------------------------------------------------------------------
# Topo program makefile
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------

LDFLAGS += -lrt -lpthread

TARGET = iot_ci

INCLUDES = -I../../IotCommon -I../../IotCommon/mqtt
OBJECTS = ci_main.o \
	topo.o \
	topoGen.o \
	dbget.o \
	dbEdit.o \
	ctrl.o \
	sensor.o \
	cmd.o \
	lmp.o \
	grp.o \
	proc.o \
	../../IotCommon/atoi.o \
	../../IotCommon/colorConv.o \
	../../IotCommon/RgbSpaceMatrices.o \
	../../IotCommon/blackbody.o \
	../../IotCommon/iotError.o \
	../../IotCommon/iotSleep.o \
	../../IotCommon/gateway.o \
	../../IotCommon/strtoupper.o \
	../../IotCommon/newDb.o \
	../../IotCommon/plugUsage.o \
	../../IotCommon/systemtable.o \
	../../IotCommon/parsing.o \
	../../IotCommon/json.o \
	../../IotCommon/jsonCreate.o \
	../../IotCommon/fileCreate.o \
	../../IotCommon/iotSemaphore.o \
	../../IotCommon/queue.o \
	../../IotCommon/socket.o \
	../../IotCommon/dump.o \
	../../IotCommon/iotTimer.o \
	../../IotCommon/newLog.o \
	../../IotCommon/mqtt/Clients.o \
	../../IotCommon/mqtt/Heap.o \
	../../IotCommon/mqtt/LinkedList.o \
	../../IotCommon/mqtt/Log.o \
	../../IotCommon/mqtt/Messages.o \
	../../IotCommon/mqtt/MQTTClient.o \
	../../IotCommon/mqtt/MQTTPacket.o \
	../../IotCommon/mqtt/MQTTPacketOut.o \
	../../IotCommon/mqtt/MQTTPersistence.o \
	../../IotCommon/mqtt/MQTTPersistenceDefault.o \
	../../IotCommon/mqtt/MQTTProtocolClient.o \
	../../IotCommon/mqtt/MQTTProtocolOut.o \
	../../IotCommon/mqtt/Socket.o \
	../../IotCommon/mqtt/SocketBuffer.o \
	../../IotCommon/mqtt/SSLSocket.o \
	../../IotCommon/mqtt/StackTrace.o \
	../../IotCommon/mqtt/Thread.o \
	../../IotCommon/mqtt/Tree.o \
	../../IotCommon/mqtt/utf-8.o \
	../../IotCommon/mqtt/mqtt_client.o


%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -Wall -g -c $< -o $@

all: clean build

build: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET) -lc
	cp $(TARGET) ../../swupdate/images/usr/bin/

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
	-rm -f ../../swupdate/images/usr/bin/$(TARGET)

