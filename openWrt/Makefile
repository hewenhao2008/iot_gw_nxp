# 
# Copyright (C) 2015-2016 NXP Semiconductors
#
#  Licensed under the Apache License, Version 2.0 (the "License")
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

COMMIT_OPENWRT = 121e04446c31fc93c467ae42ffbb4a2643afd5dd
BRANCH_OPENWRT = 15.05

OPENWRT_BUILDROOT = openWrt-$(BRANCH_OPENWRT)

BUILD_STEPS =\
	download_openWRT \
	download_feeds \
	install_packages \
	install_NXP_packages \
	apply_patches \
	copy_config \
	apply_config \
	build_image

.PHONY: 

default: all

download_openWRT:
	@echo "Getting openWRT "$(BRANCH_OPENWRT)" "$(COMMIT_OPENWRT)
	@git clone \
		http://git.openwrt.org/$(BRANCH_OPENWRT)/openwrt.git \
		$(OPENWRT_BUILDROOT)
	@cd $(OPENWRT_BUILDROOT) && git checkout $(COMMIT_OPENWRT)
	@touch download_openWRT

download_feeds: download_openWRT
	@echo "Getting feeds"
	@cp config/feeds.conf $(OPENWRT_BUILDROOT)
	@cd $(OPENWRT_BUILDROOT) && ./scripts/feeds update -a
	@touch download_feeds

install_packages: download_openWRT \
					download_feeds \
					download_feeds
	@echo "Installing openWrt legacy packages"
	@cd $(OPENWRT_BUILDROOT) && ./scripts/feeds install
	@cd $(OPENWRT_BUILDROOT) && ./scripts/feeds install diffutils libftdi apache nano bash
	@touch install_packages

install_NXP_packages: download_openWRT \
					download_feeds \
					download_feeds \
					install_packages
	@echo "Installing NXP package"
	@ln -f -s ../../packages/NXP ./$(OPENWRT_BUILDROOT)/package/NXP
	@touch install_NXP_packages

apply_patches: download_openWRT \
				download_feeds \
				download_feeds \
				install_packages \
				install_NXP_packages
	@echo "Applying patches"
	@cp -R ./files ./$(OPENWRT_BUILDROOT)/files
	@rm -f ./$(OPENWRT_BUILDROOT)/package/kernel/mac80211/patches/910-00-rt2x00-enable-rt2800soc-for-mt7620.patch
	@rm -f ./$(OPENWRT_BUILDROOT)/package/kernel/mac80211/patches/910-01-add-support-for-mt7620.patch
	@rm -f ./$(OPENWRT_BUILDROOT)/package/kernel/mac80211/patches/925-rt2x00-backport_rf5362.patch
	@cp -R ./patches/* ./$(OPENWRT_BUILDROOT)
	@touch apply_patches

copy_config: download_openWRT \
				download_feeds \
				download_feeds \
				install_packages \
				install_NXP_packages \
				apply_patches
ifeq ($(strip $(TARGET)),RPi-B+)
	@echo "Setting configuration for target $(TARGET)"
	@cp config/config.$(TARGET) $(OPENWRT_BUILDROOT)/.config
else
ifeq ($(strip $(TARGET)),RPi2-B)
	@echo "Setting default configuration (RPi2-B)"
	@cp config/config.$(TARGET) $(OPENWRT_BUILDROOT)/.config
else
	$(error "ERROR : unknown target. Was TARGET correctly set ? TARGET is set to $(TARGET)")
endif
endif
	@cp -R files $(OPENWRT_BUILDROOT)
	@touch copy_config

apply_config: download_openWRT \
				download_feeds \
				download_feeds \
				install_packages \
				install_NXP_packages \
				apply_patches \
				copy_config
	@cd ./$(OPENWRT_BUILDROOT) ; make oldconfig
	# optimize build time (build only mandatory stuff)
	@cd ./$(OPENWRT_BUILDROOT) ; ../tools/config_feeds_disable_packages.pl > config.new
	@cd ./$(OPENWRT_BUILDROOT) ; mv config.new  .config
	@cd ./$(OPENWRT_BUILDROOT) ; ../tools/config_install_feeds.pl
	@touch apply_config
	@echo "Buildroot installation completed"

# This target is made each time
build_image: download_openWRT \
				download_feeds \
				download_feeds \
				install_packages \
				install_NXP_packages \
				apply_patches \
				copy_config \
				apply_config
ifneq ($(strip $(DL)),)
	ln -f -s $(DL) ./$(OPENWRT_BUILDROOT)/dl
endif
	@echo "Build openWrt image"
	cd ./$(OPENWRT_BUILDROOT) ; make

all: $(BUILD_STEPS)
	@echo "Image completed"
	@ls -al $(OPENWRT_BUILDROOT)/bin/brcm2708/*sdcard*

clean_all:
	@echo "Removing Buildroot"
	@rm -Rf $(BUILD_STEPS) $(OPENWRT_BUILDROOT)

