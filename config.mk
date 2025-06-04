OUT_NAME = dwm

include version.mk

CC         ?= gcc
CXX        ?= g++
AR         ?= ar
PKG_CONFIG ?= pkg-config

SRC_DIR   = src
BUILD_DIR = build

# Turn (some) warnings into errors
WARNINGS_AS_ERRORS=1
# Use address, leak and UB sanisizers
USE_SANITIZERS=1
# allow c and c++ extension
USE_EXTENSIONS=

# Generate a `compile_commands.json` file in the root of the project
COMPILE_COMMANDS ?=

# Possible values are DEBUG and RELEASE (case sensitive)
MODE ?=
# This value will be used if MODE is not specified
DEFAULT_MODE = DEBUG

include libs.mk

# Install directories. Defaults to out/ in the root of the project
# To install system wide by default, set DESTDIR= PREFIX=/usr
DESTDIR ?= ./
PREFIX  ?= out
MANPREFIX  = $(PREFIX)/share/man
ICONPREFIX ?= $(DESTDIR)$(PREFIX)/share/pixmap
ICONDIR ?= $(ICONPREFIX)
