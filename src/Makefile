# ------------------------------------------------------------------
# Top-level IoT Makefile
# Build (or clean) all IoT software in one go
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------
#-����Ӧ�ó�����֯�Ķ���makefile

#-����ı�ʾ��Ŀ¼,���ڶ�Ŀ¼��makefile�ļ��ı�д
SUBDIRS = startup daemons www Testing Zcb		#-˵�����makefile��������������Ŀ¼,ָ����ǰĿ¼�£�Ҫ�ݹ�������Ŀ¼��
#-�ж����������Ƿ����,Ȼ�������������
ifeq ($(TARGET_MACHINE),RASPBERRYPI)
SUBDIRS += jnFlasher
endif

ifeq ($(TARGET_MACHINE),LINUX_PC)
export CFLAGS += -DTARGET_RASPBERRYPI -DTARGET_LINUX_PC
endif
#-������仰�ܱ�֤�ݹ鵽��Ŀ¼�С�
.PHONY: subdirs $(SUBDIRS)
#-�����ǻ�������,��������л�������ָ�����ռ�Ŀ����б�
DOIT = $(MAKECMDGOALS)

all: $(SUBDIRS)

build: $(SUBDIRS)
ifeq ($(TARGET_MACHINE),LINUX_PC)
	rm -Rf /tmp/iot-test
	mkdir -p /tmp/iot-test
	ln -s -t /tmp/iot-test $(CURDIR)/swupdate/images/*
endif

clean: $(SUBDIRS)
ifeq ($(TARGET_MACHINE),LINUX_PC)
	rm -Rf /tmp/iot-test
	rm -Rf swupdate/images/www
	rm -Rf swupdate/images/tmp
	rm -Rf swupdate/images/usr
	rm -Rf swupdate/images/etc
endif

$(SUBDIRS):
	$(MAKE) $(DOIT) -C $@
