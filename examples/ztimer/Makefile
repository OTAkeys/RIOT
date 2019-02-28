APPLICATION = ztimer_example
RIOTBASE ?= $(CURDIR)/../..
BOARD ?= native
USEMODULE += xtimer ztimer ztimer_periph ztimer_usec
QUIET ?= 0

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP = 1

DEBUG = 1
enable_debug = 1
CFLAGS += -DDEBUG_ASSERT_VERBOSE

FEATURES_REQUIRED += periph_timer
FEATURES_REQUIRED += periph_rtt

include $(RIOTBASE)/Makefile.include
