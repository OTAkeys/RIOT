include ../Makefile.tests_common

BOARD_INSUFFICIENT_MEMORY := nucleo32-f031

USEMODULE += ztimer ztimer_periph ztimer_usec

include $(RIOTBASE)/Makefile.include

test:
	tests/01-run.py
