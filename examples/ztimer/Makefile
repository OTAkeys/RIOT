APPLICATION = ztimer_example
RIOTBASE ?= $(CURDIR)/../..
BOARD ?= native
USEMODULE += ztimer ztimer_periph ztimer_usec
QUIET ?= 0

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP = 1

DEBUG = 1

FEATURES_REQUIRED += periph_timer ztimer
FEATURES_REQUIRED += periph_rtt

include $(RIOTBASE)/Makefile.include
