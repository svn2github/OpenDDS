# -*- Makefile -*-

#----------------------------------------------------------------------------
#
#       $Id$
#
#       Common Makefile rules for all of DDS
#
#----------------------------------------------------------------------------

ifndef TAO_ROOT
  TAO_ROOT = $(ACE_ROOT)/TAO
endif

ifndef DDS_ROOT
  DDS_ROOT = $(TAO_ROOT)/DDS
endif

ifeq (,$(findstring -L$(DDS_ROOT)/ciao,$(LDFLAGS)))
  LDFLAGS += -L$(DDS_ROOT)/dds
endif
ifeq (,$(findstring -I$(DDS_ROOT),$(INCLDIRS)))
  INCLDIRS += -I$(DDS_ROOT)
endif

# Set defaults for gnumake macros corresponding to MPC features used by DDS
built_in_topics ?= 1

# Turn on symbol versioning. The scheme that we follow is to allow
# applications dependent on libraries, with same version numbers (major,
# minor and beta) to run, but applications with dependencies on libraries
# with different minor or major or beta versions to fail.
#
ifeq (cmd,$(findstring cmd,$(SHELL)))
DDS_MAJOR_VERSION := $(shell awk "/DDS_MAJOR_VERSION/ { print $$3}" ${DDS_ROOT}/dds/Version.h)
DDS_MINOR_VERSION := $(shell awk "/DDS_MINOR_VERSION/ { print $$3}" ${DDS_ROOT}/dds/Version.h)
else
DDS_MAJOR_VERSION := $(shell awk '/DDS_MAJOR_VERSION/ { print $$3}' ${DDS_ROOT}/dds/Version.h)
DDS_MINOR_VERSION := $(shell awk '/DDS_MINOR_VERSION/ { print $$3}' ${DDS_ROOT}/dds/Version.h)
endif

# Version number of the libraries
#
ifeq ($(versioned_so),1)
  # Only set SOVERSION for DDS if versioning is turned on and SOVERION is
  # blank, which gives users to ability to override it.
  ifeq ($(SOVERSION),)
    SOVERSION = .$(DDS_MAJOR_VERSION).$(DDS_MINOR_VERSION)
  endif # SOVERSION

  # Name that will be written into the dynamic library
  #
  ifneq ($(SONAME),)
    SONAME = $(SHLIB).$(DDS_MAJOR_VERSION).$(DDS_MINOR_VERSION)
  endif # SONAME
endif # versioned_so
