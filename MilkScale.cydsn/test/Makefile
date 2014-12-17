#---------
#
# CppUTest Examples Makefile
#
#----------

#Set this to @ to keep the makefile quiet
ifndef SILENCE
	SILENCE = @
endif

CPPUTEST_CXXFLAGS += -Wno-old-style-cast

#--- Inputs ----#
COMPONENT_NAME = RingBufferTests
CPPUTEST_HOME = ./cpputest

CPPUTEST_USE_EXTENSIONS = Y
CPP_PLATFORM = Gcc

# This line is overriding the default new macros.  This is helpful
# when using std library includes like <list> and other containers
# so that memory leak detection does not conflict with stl.
#CPPUTEST_MEMLEAK_DETECTOR_NEW_MACRO_FILE = -include ApplicationLib/ExamplesNewOverrides.h
SRC_DIRS = \

SRC_FILES = \
	    ../ringbuf.c

TEST_SRC_DIRS = \
	tests

INCLUDE_DIRS =\
  ..\
  $(CPPUTEST_HOME)/include\

include $(CPPUTEST_HOME)/build/MakefileWorker.mk


