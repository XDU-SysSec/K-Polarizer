# Setup for testing environment

CC      = gcc
CXX     = g++

AS      = $(CC)
LINK    = $(CXX)

AR      = ar

CFLAGS          = -O2 -std=gnu99
CXXFLAGS        = -O2
ASFLAGS         = -fPIC

WITH_PIE        = -fPIE -pie
NO_PIE          = -fpie -no-pie

DYNAMIC_LINKING =
STATIC_LINKING  = -static

# Platform determination

ROOT        = .
BUILDDIR    = $(ROOT)/$(PLATFORM)/build/

P_ARCH := $(shell uname -m)
ifeq (armv7l,$(P_ARCH))
    P_ARCH = arm
endif
DISTRO = $(word 3,$(shell lsb_release -i))
export P_ARCH
$(if $(VERBOSE),$(info "Building for $(P_ARCH)"))

ifeq ($(PLATFORM),)
PLATFORM=$(P_ARCH)-local
$(info Defaulting to PLATFORM=$(PLATFORM))
endif

ifeq (aarch64,$(P_ARCH))
    CFLAGS += -DARCH_AARCH64
else ifeq (x86_64,$(P_ARCH))
    CFLAGS += -DARCH_X86_64
else ifeq (arm,$(P_ARCH))
    CFLAGS += -DARCH_ARM
else
$(error Unsupported platform, we only handle arm, aarch64, and x86_64)
endif

ifeq ($(VERBOSE),)
	SHORT_AS    = @echo "AS   $<"; $(AS)
	SHORT_CC    = @echo "CC   $<"; $(CC)
	SHORT_CXX   = @echo "CXX  $<"; $(CXX)
	SHORT_LINK  = @echo "LINK $@"; $(LINK)
	SHORT_AR    = @echo "AR   $@"; $(AR)
else
	SHORT_AS    = $(AS)
	SHORT_CC    = $(CC)
	SHORT_CXX   = $(CXX)
	SHORT_LINK  = $(LINK)
	SHORT_AR    = $(AR)
endif
