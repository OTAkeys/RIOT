ifneq (,$(filter newlib_nano,$(USEMODULE)))
  USE_NEWLIB_NANO = 1
  # Test if nano.specs is available
  ifeq ($(shell $(LINK) -Werror -specs=nano.specs -E - 2>/dev/null >/dev/null </dev/null ; echo $$?),0)
    USE_NANO_SPECS_FILE = 1
    ifeq ($(shell echo "int main(){} void _exit(int n) {(void)n;while(1);}" | LC_ALL=C $(CC) -xc - -o /dev/null -lc -specs=nano.specs -Wall -Wextra -pedantic 2>&1 | grep -q "use of wchar_t values across objects may fail" ; echo $$?),0)
        CFLAGS += -fshort-wchar
        LINKFLAGS += -Wl,--no-wchar-size-warning
    endif
  endif
endif

ifneq (,$(filter newlib_gnu_source,$(USEMODULE)))
  CFLAGS += -D_GNU_SOURCE=1
endif

export LINKFLAGS += -lc

ifeq (1,$(USE_NANO_SPECS_FILE))
  export LINKFLAGS += -specs=nano.specs
  export CFLAGS += -specs=nano.specs
else
  # Try to search for newlib in the standard search path of the compiler for includes
  ifeq (,$(NEWLIB_INCLUDE_DIR))
    COMPILER_INCLUDE_PATHS := $(shell $(PREFIX)gcc -v -x c -E /dev/null 2>&1 | grep '^\s' | tr -d '\n')
    NEWLIB_INCLUDE_DIR := $(firstword $(dir $(wildcard $(addsuffix /newlib.h, $(COMPILER_INCLUDE_PATHS)))))
  endif

  ifeq (,$(NEWLIB_INCLUDE_DIR))
    $(warning newlib.h not found in compiler standard search path, looking in predefined directories)
    # Ubuntu gcc-arm-embedded toolchain (https://launchpad.net/gcc-arm-embedded)
    # places newlib headers in several places, but the primary source seem to be
    # /etc/alternatives/gcc-arm-none-eabi-include
    # Gentoo Linux crossdev place the newlib headers in /usr/arm-none-eabi/include
    # Arch Linux also place the newlib headers in /usr/arm-none-eabi/include
    # Ubuntu seem to put a copy of the newlib headers in the same place as
    # Gentoo crossdev, but we prefer to look at /etc/alternatives first.
    # On OSX, newlib includes are possibly located in
    # /usr/local/opt/arm-none-eabi*/arm-none-eabi/include or /usr/local/opt/gcc-arm/arm-none-eabi/include
    NEWLIB_INCLUDE_PATHS ?= \
      /etc/alternatives/gcc-$(TARGET_ARCH)-include \
      /usr/$(TARGET_ARCH)/include \
      /usr/local/opt/$(TARGET_ARCH)*/$(TARGET_ARCH)/include \
      /usr/local/opt/gcc-*/$(TARGET_ARCH)/include \
      #

    NEWLIB_INCLUDE_DIR ?= $(firstword $(dir $(wildcard $(addsuffix /newlib.h, $(NEWLIB_INCLUDE_PATHS)))))
  endif

  # If nothing was found we will try to fall back to searching for a cross-gcc in
  # the current PATH and use a relative path for the includes
  ifeq (,$(NEWLIB_INCLUDE_DIR))
    $(warning newlib.h not found, guessing newlib include path from gcc path)
    NEWLIB_INCLUDE_DIR := $(abspath $(wildcard $(dir $(shell command -v $(PREFIX)gcc 2>/dev/null))/../$(TARGET_ARCH)/include))
  endif

  ifeq (,$(NEWLIB_INCLUDE_DIR))
    $(error Could not find newlib include directory)
  endif

  # We use the -isystem gcc/clang argument to add the include
  # directories as system include directories, which means they will not be
  # searched until after all the project specific include directories (-I/path)
  NEWLIB_INCLUDES := -isystem $(NEWLIB_INCLUDE_DIR)
  NEWLIB_INCLUDES += $(addprefix -isystem ,$(abspath $(wildcard $(dir $(NEWLIB_INCLUDE_DIR))/usr/include)))
  ifeq (1,$(USE_NEWLIB_NANO))
    # newlib-nano include directory is called either newlib-nano or nano. Use the one we find first.
    NEWLIB_NANO_INCLUDE_DIR ?= $(firstword $(wildcard $(addprefix $(NEWLIB_INCLUDE_DIR)/, newlib-nano newlib/nano nano)))
    ifneq (,$(NEWLIB_NANO_INCLUDE_DIR))
      # newlib-nano overrides newlib.h and its include dir
      # should therefore go before the other newlib includes.
      NEWLIB_INCLUDES := -isystem $(NEWLIB_NANO_INCLUDE_DIR) $(NEWLIB_INCLUDES)
    else
      $(warning Could not find newlib nano include dir, using normal newlib)
    endif
  endif
endif

# Newlib includes should go before GCC includes. This is especially important
# when using Clang, because Clang will yield compilation errors on some GCC-
# bundled headers. Clang compatible versions of those headers are already
# provided by Newlib, so placing this directory first will eliminate those problems.
# The above problem was observed with LLVM 3.9.1 when building against GCC 6.3.0 headers.
export INCLUDES := $(NEWLIB_INCLUDES) $(INCLUDES)
