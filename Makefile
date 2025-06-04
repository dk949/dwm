include vendor.mk
include config.mk

############################## Collecting sources ##############################

# NOTE: making real build directory BUILD_DIR/SRC_DIR allows for multiple
#       makefiles/subprojects with different source directories but a single top
#       level build directory without accidentally overwriting other subproject
#       object files.
REAL_BUILD_DIR=$(BUILD_DIR)/$(SRC_DIR)
CXX_SRC = $(wildcard $(SRC_DIR)/*.cpp)
CXX_OBJ = $(CXX_SRC:$(SRC_DIR)/%.cpp=$(REAL_BUILD_DIR)/%.o)

C_SRC = $(wildcard $(SRC_DIR)/*.c)

ifndef ASOUNDFLAGS
C_SRC = $(filter-out $(SRC_DIR)/volc.c,$(C_SRC))
endif

C_OBJ = $(C_SRC:$(SRC_DIR)/%.c=$(REAL_BUILD_DIR)/%.o)

PREREQ = $(CXX_OBJ:%.o=%.d) $(C_OBJ:%.o=%.d)

##################################### Misc #####################################

COMMA := ,
LP := )

################################## Check mode ##################################
# NOTE: this has to be very early in the make file, since further configuration requires MODE to be set
ifdef MODE
ifneq ($(shell case $(MODE) in DEBUG|RELEASE$(LP) echo 1; esac),1)
$(error MODE must be one of DEBUG or RELEASE)
endif
else
ifeq ($(shell [ -f "$(REAL_BUILD_DIR)/MODE" ] && echo 1),1)
MODE = $(shell cat "$(REAL_BUILD_DIR)/MODE")
OLDMODE=1
else
MODE=$(DEFAULT_MODE)
endif
endif

############################ Detailed configuration ############################
# Preprocessor flags

CPPFLAGS = -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_XOPEN_SOURCE=700L -D$(OUT_NAME)_version=\"$(VERSION)\" -DICONDIR=\"$(ICONPREFIX)\"

# Compiler flags

warn_no_error = -W$1 -Wno-error=$1

## Clang: ignore uknown warnings
GCC_CWARN = -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wunused-const-variable=1
GCC_CXXWARN = -Wnoexcept -Wuseless-cast

CLANG_CWARN = -Wused-but-marked-unused -Wunused-comparison

CLANG_CXXWARN = $(call warn_no_error,unused-private-field) $(call warn_no_error,unused-exception-parameter) \
				$(call warn_no_error,unused-lambda-capture) $(call warn_no_error,unused-member-function)    \
				$(call warn_no_error,unused-template)

## Raise error
CWARN = -Wall -Wextra -Wshadow -Wcast-align -Wunused -Wunused-result                                 \
		-Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2              \
		-Wswitch-default -Wswitch-enum -Wimplicit-fallthrough -Wmisleading-indentation               \
		$(call warn_no_error,unused-but-set-parameter) $(call warn_no_error,unused-but-set-variable) \
		$(call warn_no_error,unused-const-variable) $(call warn_no_error,unused-local-typedefs)      \
		$(call warn_no_error,unused-function) $(call warn_no_error,unused-parameter)                 \
		$(call warn_no_error,unused-label) $(call warn_no_error,unused-macros)                       \
		$(call warn_no_error,unused-variable)                                                        \
		$(if $(findstring clang,$(CC)),$(CLANG_CWARN),)                                              \
		$(if $(findstring clang,$(CC)),,$(GCC_CWARN))                                                \
		$(if $(WARNINGS_AS_ERRORS),-Werror,)                                                         \
		$(if $(USE_EXTENSIONS),,-Wpedantic -pedantic-errors)

## C++ specific extra errors
CXXWARN = -Wnon-virtual-dtor -Wno-old-style-cast -Woverloaded-virtual \
		  -Wctad-maybe-unsupported -Weffc++ -Wsuggest-override     \
		  $(if $(findstring clang,$(CXX)),$(CLANG_CXXWARN),)       \
		  $(if $(findstring clang,$(CXX)),,$(GCC_CXXWARN))         \

## Flags to be used in DEBUG mode
DEBUG_FLAGS    = -Og -g $(if $(USE_SANITIZERS),-fsanitize=address$(COMMA)undefined$(COMMA)leak,)
## Flags to be used in RELEASE mode
RELEASE_FLAGS  = -O3 -DNDEBUG -flto

## Flags shared by C and C++
COMMON_FLAGS = $(CPPFLAGS) $($(MODE)_FLAGS) $(CWARN) $(INC) $(if $(PKG_CONFIG_LIBS),$(shell $(PKG_CONFIG) --cflags $(PKG_CONFIG_LIBS)),)

## C specific flags
CFLAGS = $(COMMON_FLAGS) -std=c11
## C++ specific flags
CXXFLAGS = $(COMMON_FLAGS) $(CXXWARN) -std=c++23

# Linker flags

## Flags to be used in DEBUG mode
DEBUG_LDFLAGS    = -lg $(if $(USE_SANITIZERS),-fsanitize=address$(COMMA)undefined$(COMMA)leak,)
## Flags to be used in RELEASE mode
RELEASE_LDFLAGS  = -flto

LDFLAGS = $($(MODE)_LDFLAGS) $(LIBS) $(if $(PKG_CONFIG_LIBS),$(shell $(PKG_CONFIG) --libs $(PKG_CONFIG_LIBS)),)

ifdef COMPILE_COMMANDS
ifeq ($(shell command -v jq &> /dev/null && echo 1),1)
COMPILE_COMMANDS_FILE=compile_commands.json
else
$(warning jq is not installed, install it or disable COMPILE_COMMANDS to silence this warning)
endif
endif

########################## Version management details ##########################

VER_MAJ = `echo $(VERSION) | cut -d '.' -f1`
VER_MIN = `echo $(VERSION) | cut -d '.' -f2`
VER_PATCH =`echo $(VERSION) | cut -d '.' -f3`

#################################### Rules #####################################

OUT_DIR=bin
REAL_OUT_NAME= $(REAL_BUILD_DIR)/$(OUT_DIR)/$(OUT_NAME)
INSTALL_NAME = "$(REAL_OUT_NAME)"

all: $(REAL_OUT_NAME) $(COMPILE_COMMANDS_FILE)
$(CXX_OBJ): $(REAL_BUILD_DIR)/MODE $(NB_OUT)
$(C_OBJ): $(REAL_BUILD_DIR)/MODE $(NB_OUT)

install: all
	@mkdir -p "$(DESTDIR)$(PREFIX)/$(OUT_DIR)"
	cp -d $(INSTALL_NAME) "$(DESTDIR)$(PREFIX)/$(OUT_DIR)"
	@mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < $(OUT_NAME).1 > $(DESTDIR)$(MANPREFIX)/man1/$(OUT_NAME).1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/$(OUT_NAME).1
	@mkdir -p $(ICONPREFIX)
	cp ./icons/dwm*.svg $(ICONPREFIX)

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/$(OUT_DIR)/$(OUT_NAME)" \
		$(DESTDIR)$(MANPREFIX)/man1/$(OUT_NAME).1


$(REAL_OUT_NAME): $(CXX_OBJ) $(C_OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

$(REAL_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p "$(REAL_BUILD_DIR)/$(OUT_DIR)"
	$(CXX) -MD $(CXXFLAGS) -c $< -o "$@"

$(REAL_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(REAL_BUILD_DIR)/$(OUT_DIR)"
	$(CC) -MD $(CFLAGS) -c $< -o "$@"

clean:
	rm -f $(REAL_BUILD_DIR)/*.o $(REAL_BUILD_DIR)/*.d $(REAL_BUILD_DIR)/MODE $(COMPILE_COMMANDS_FILE)

ifdef COMPILE_COMMANDS_FILE
file_entry =                                                    \
	jq -nj                                                      \
	--arg directory "`realpath $(REAL_BUILD_DIR)`"              \
	--arg file "`realpath $1`"                                  \
	--arg command '$2'                                          \
	'{directory:$$directory, file:$$file, command:$$command}';  \

file_entries = $(foreach f,$1,$(call file_entry,$f,$2))

$(COMPILE_COMMANDS_FILE): FORCE
	@{                                                                          \
		$(call file_entries,$(C_SRC),$(shell which $(CC)) $(CFLAGS) -c)        \
		$(call file_entries,$(CXX_SRC),$(shell which $(CXX)) $(CXXFLAGS) -c)   \
	} | jq --slurp > $@
endif

.PHONY: all clean install FORCE
ifndef OLDMODE
$(REAL_BUILD_DIR)/MODE: FORCE
	@mkdir -p $(REAL_BUILD_DIR)
	@[ "`cat $@ 2>&1`" = '$(MODE)' ] || printf "%s" "$(MODE)" >"$@"
endif

-include $(PREREQ)

# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <https://unlicense.org>
