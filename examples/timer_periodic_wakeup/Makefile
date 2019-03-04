APPLICATION = timer_periodic_wakeup
RIOTBASE ?= $(CURDIR)/../..
BOARD ?= native
USEMODULE += xtimer

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 0

#TUI ?= 1
#DEBUG ?= 1
ENABLE_DEBUG ?= 1
CFLAGS += -DDEBUG_ASSERT_VERBOSE 
CFLAGS += -O0
CFLAGS_OPT += -O0
CFLAGS += -DUSE_LPTIM
CFLAGS += '-DPM_BLOCKER_INITIAL={ .val_u8[0]=1 }'

FEATURES_REQUIRED += periph_rtt

include $(RIOTBASE)/Makefile.include
