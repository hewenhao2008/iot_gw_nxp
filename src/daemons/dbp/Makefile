# ------------------------------------------------------------------
# DBP makefile
# ------------------------------------------------------------------
# Author:    nxp80037
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------

LDFLAGS += -pthread

TARGET = iot_dbp

INCLUDES = -I../../IotCommon -I../../IotCommon/mqtt
OBJECTS = dbp_main.o \
	dbp.o \
	dbp_search.o \
	dbp_loc_receiver.o \
	../../IotCommon/atoi.o \
	../../IotCommon/iotError.o \
	../../IotCommon/iotSemaphore.o \
	../../IotCommon/newDb.o \
	../../IotCommon/parsing.o \
	../../IotCommon/json.o \
	../../IotCommon/fileCreate.o \
	../../IotCommon/queue.o \
	../../IotCommon/dump.o \
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


