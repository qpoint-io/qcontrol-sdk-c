# content-filter plugin Makefile
#
# Build:   make build  (shared library for dynamic loading)
# Dist:    make dist   (object file for bundling)
# Clean:   make clean

NAME := content-filter
ARCH := $(shell uname -m | sed 's/^arm64$$/aarch64/')-$(shell uname -s | tr A-Z a-z | sed 's/^darwin$$/macos/')
DIST := dist
SHLIB_EXT := $(if $(filter Darwin,$(shell uname -s)),dylib,so)

CC ?= gcc
CFLAGS_SHARED := -shared -fPIC -Wall -Wextra
CFLAGS_OBJECT := -c -fPIC -Wall -Wextra

# SDK dependency
SDK_REPO ?= https://github.com/qpoint-io/qcontrol-sdk-c
SDK_REF ?= main
SDK_DIR := sdk
LOCAL_SDK := ../../include

INCLUDES := -I$(SDK_DIR)/include
LIBS := -lpthread

SRC := src/plugin.c

.PHONY: all build dist clean sdk

all: build

# Use local SDK if available, otherwise fetch from GitHub
sdk:
	@if [ -f "$(LOCAL_SDK)/qcontrol.h" ]; then \
		rm -rf $(SDK_DIR); \
		mkdir -p $(SDK_DIR)/include; \
		cp -r $(LOCAL_SDK)/* $(SDK_DIR)/include/; \
		echo "Using local SDK"; \
	elif [ ! -d "$(SDK_DIR)" ]; then \
		git clone --depth 1 --branch $(SDK_REF) $(SDK_REPO) $(SDK_DIR); \
	fi

build: sdk
	@mkdir -p $(DIST)
	$(CC) $(CFLAGS_SHARED) -o $(DIST)/$(NAME)-$(ARCH).$(SHLIB_EXT) $(SRC) $(INCLUDES) $(LIBS)

dist: sdk
	@mkdir -p $(DIST)
	$(CC) $(CFLAGS_OBJECT) -o $(DIST)/$(NAME)-$(ARCH).o $(SRC) $(INCLUDES)

clean:
	rm -rf $(DIST)

clean-all: clean
	rm -rf $(SDK_DIR)
