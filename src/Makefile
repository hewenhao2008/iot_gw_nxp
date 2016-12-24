# ------------------------------------------------------------------
# Top-level IoT Makefile
# Build (or clean) all IoT software in one go
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------
#-整个应用程序组织的顶层makefile

#-下面的表示子目录,用于多目录下makefile文件的编写
SUBDIRS = startup daemons www Testing Zcb		#-说明这个makefile下有这样几个子目录,指定当前目录下，要递归编译的子目录。
#-判断两个变量是否相等,然后觉得条件编译
ifeq ($(TARGET_MACHINE),RASPBERRYPI)
SUBDIRS += jnFlasher
endif

ifeq ($(TARGET_MACHINE),LINUX_PC)
export CFLAGS += -DTARGET_RASPBERRYPI -DTARGET_LINUX_PC
endif
#-下面这句话能保证递归到子目录中。
.PHONY: subdirs $(SUBDIRS)
#-下面是环境变量,这个变量中会存放你所指定的终极目标的列表
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

