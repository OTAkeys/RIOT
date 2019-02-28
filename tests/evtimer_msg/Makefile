include ../Makefile.tests_common

BOARD_INSUFFICIENT_MEMORY := arduino-duemilanove arduino-uno nucleo-f031k6 \
                             nucleo-f042k6

USEMODULE += evtimer

#CFLAGS += -DDEBUG_ASSERT_VERBOSE
#CFLAGS += -O0

#USEMODULE += xtimer
#FEATURES_REQUIRED += periph_timer
#
#USEMODULE += ztimer ztimer_rtt ztimer_extend
#FEATURES_REQUIRED += periph_rtt

TEST_ON_CI_WHITELIST += all

include $(RIOTBASE)/Makefile.include
