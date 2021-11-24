# Project Name
TARGET ?= Wreath

DEBUG = 1
OPT = -O0

# Sources
CPP_SOURCES = ../kxmx_bluemchen/src/kxmx_bluemchen.cpp ./bluemchen/Wreath.cpp

USE_FATFS = 1

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libdaisy/
DAISYSP_DIR = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
