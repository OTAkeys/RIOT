APPLICATION = ztimer_periodic_wakeup
RIOTBASE ?= $(CURDIR)/../..
BOARD ?= native
QUIET ?= 0

DEVELHELP = 1
TUI = 1
DEBUG = 1
ENABLE_DEBUG = 1
CFLAGS += -DDEBUG_ASSERT_VERBOSE 
CFLAGS += -O0
CFLAGS_OPT += -O0
USEMODULE += ztimer
USEMODULE += periph_rtt
#FEATURES_REQUIRED += periph_timer
#FEATURES_REQUIRED += ztimer ztimer_extend


include $(RIOTBASE)/Makefile.include
