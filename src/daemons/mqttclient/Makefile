# ------------------------------------------------------------------
# MQTT Client makefile
# ------------------------------------------------------------------
# Author:    taida
# Copyright: NXP B.V. 2016. All rights reserved
# ------------------------------------------------------------------
#-这个是最后一个makefile了

LDFLAGS += -lpthread

TARGET = iot_mc
#-包含头文件的地方
INCLUDES = -I../../IotCommon -I../../IotCommon/mqtt
OBJECTS = mc_main.o \
	mqtt_publish.o \
	mqtt_subscribe.o \
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

