#include <stdio.h>

#include "ztimer.h"
#include "ztimer/rtt.h"

/* for RTT_FREQUENCY */
#include "periph_conf.h"

static void callback(void *arg);

ztimer_rtt_t ztimer_rtt;
ztimer_t t = { .callback=callback, .arg="foobar!" };

static void callback(void *arg)
{
    puts(arg);
    ztimer_set(&ztimer_rtt, &t, RTT_FREQUENCY * 5);
}

int main(void)
{
    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);

    ztimer_rtt_init(&ztimer_rtt);

    ztimer_set(&ztimer_rtt, &t, RTT_FREQUENCY * 5);

    while(1) {
        puts("sleeping...");
	    ztimer_sleep(&ztimer_rtt, RTT_FREQUENCY * 1);
    }

    return 0;
}
