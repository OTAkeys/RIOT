/*
 * Copyright (C) 2018 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdint.h>

#include "irq.h"
#include "ztimer.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#define SECURITY_VALUE  20

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _update_head_offset(ztimer_dev_t *ztimer, uint16_t elapsed_counts);
static void _ztimer_relaunch_if_needed(ztimer_dev_t *ztimer);
static void _ztimer_print(ztimer_dev_t *ztimer);

void ztimer_remove(ztimer_dev_t *ztimer, ztimer_t *entry)
{
    DEBUG("enter:ztimer_remove()\n");
    unsigned state = irq_disable();
    uint16_t elapsed_counts = ztimer->ops->now(ztimer) - ztimer->list.offset;
    _update_head_offset(ztimer, elapsed_counts);
    _del_entry_from_list(ztimer, &entry->base);

    _ztimer_relaunch_if_needed(ztimer);

    irq_restore(state);
    DEBUG("exit:ztimer_remove()\n");
}

void ztimer_set(ztimer_dev_t *ztimer, ztimer_t *entry, uint32_t val)
{
    DEBUG("enter:ztimer_set()\n");
    uint32_t now = ztimer->ops->now(ztimer);
    DEBUG("ztimer_set(): now:0x%08"PRIx32" list.offset 0x%08"PRIx32
            "target 0x%08"PRIx32"\n", now, ztimer->list.offset, val);

    unsigned state = irq_disable();
    _update_head_offset(ztimer, now - ztimer->list.offset);
    _del_entry_from_list(ztimer, &entry->base);
    //update target alarm to current N counts value
    entry->base.offset =  val;
    DEBUG("possibleoffset to trigger %"PRIx32"\n", entry->base.offset);

    _add_entry_to_list(ztimer, &entry->base);
    if (ztimer->list.next == &entry->base) {
        //if next target should fire before next overflow check it!
        if((entry->base.offset + now + SECURITY_VALUE) < (uint32_t)0xFFFF){
            DEBUG("offset+now smaller than FFFF\n");
            DEBUG("no ovf alarm:target=0x%08"PRIx32"--cnt=0x%08"PRIx32"\n",
                    entry->base.offset, now);
            /* The added entry became the new list head */
            ztimer->ops->set(ztimer, entry->base.offset);
            //cancel ovf alarm cause it won't happen before next overflow
            ztimer->ops->cancel_ovf(ztimer);
        } else{
            DEBUG("offset+now bigger than FFFF\n");
            DEBUG("set ovf callback\n");
            //set overflow callback
            ztimer->ops->set_ovf_alarm(ztimer);
            //cancel possible next alarm
            ztimer->ops->cancel(ztimer);
        }
        //only if we just trigger the first alarm, we update the reference counts
        ztimer->list.offset = now;
    }
    irq_restore(state);
    DEBUG("exit:ztimer_set()\n");
}

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    uint32_t delta_sum = 0;
    DEBUG("enter:_add_entry_to_list()\n");
    ztimer_base_t *list = &ztimer->list;

    /* Jump past all entries which are set to an earlier target than the new entry */
    while (list->next) {
        ztimer_base_t *list_entry = list->next;
        if ((list_entry->offset + delta_sum) > entry->offset) {
            break;
        }
        delta_sum += list_entry->offset;
        list = list->next;
    }

    DEBUG("delta_sum=0x%08lx\n", delta_sum);
    /* Insert into list */
    entry->next = list->next;
    entry->offset -= delta_sum;
    if (entry->next) {
        entry->next->offset -= entry->offset;
    }
    list->next = entry;
    DEBUG("_add_entry_to_list() %p offset %"PRIx32"\n", (void *)entry, entry->offset);
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    DEBUG("exit:_add_entry_to_list()\n");
}


//update all timer's offsets in list with the elapsed counts.
static void _update_head_offset(ztimer_dev_t *ztimer, uint16_t elapsed_counts)
{
    DEBUG("enter:update_head_offset()\n");
    ztimer_base_t *entry = ztimer->list.next;
    DEBUG("ztimer:update_head_offset() ELAPSED COUNTS=0x%08"PRIx32"\n", (uint32_t)elapsed_counts);
    if (entry) {
    DEBUG("sthing to update: elapsed time %04x with "
                "entry offset %" PRIx32" \n", elapsed_counts, entry->offset);
        do {
            if (elapsed_counts <= entry->offset) {
                entry->offset -= elapsed_counts;
                break;
            } else{
                elapsed_counts -= entry->offset;
                if (elapsed_counts > 0) {
                    /* skip timers with offset < diff */
                    entry->offset = 0;
                    entry = entry->next;
                }
            }
        } while ((elapsed_counts > 0) && entry);
    }
//    ztimer->list.offset += elapsed_counts;
    if (entry) {
        DEBUG("ztimer_offset=%"PRIx32" new head %p with entry offset %"PRIx32"\n",
            ztimer->list.offset, (void *)entry, entry->offset);
    }
    DEBUG("exit:update_head_offset()\n");
}

static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    DEBUG("enter:_del_entry_from_list()\n");
    ztimer_base_t *list = &ztimer->list;
    while (list->next) {
        ztimer_base_t *list_entry = list->next;
        if (list_entry == entry) {
            DEBUG("del_entry_from_list(), removing %p\n", (void *)list->next);
            list->next = entry->next;
            if (list->next) {
                list_entry = list->next;
                list_entry->offset += entry->offset;
            }
            break;
        }
        list = list->next;
    }
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    //if there's no more alarms to fire... stop everything!
    if(!list->next){
        DEBUG("NO MORE ENTRIES, STOPPING CB'S\n");
        ztimer->ops->cancel(ztimer);
        ztimer->ops->cancel_ovf(ztimer);
        ztimer->list.offset = 0;
    }
    DEBUG("exit:_del_entry_from_list()\n");
}


//only return timer that should be fired
static ztimer_t *_now_next(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = ztimer->list.next;

    if (entry && (entry->offset == 0)) {
        ztimer->list.next = entry->next;
        return (ztimer_t*)entry;
    }
    else {
        return NULL;
    }
}

static void _ztimer_relaunch_if_needed(ztimer_dev_t *ztimer)
{
    DEBUG("enter:_ztimer_relaunch_if_needed()\n");
    uint16_t now = ztimer->ops->now(ztimer);
    DEBUG("RELAUNCH IF NEEDED: now 0x%04x\n", now);
    DEBUG("global off=0x%08lx\n",ztimer->list.offset);
    if (ztimer->list.next) {
        //if next target should fire before next overflow set it!
        if((ztimer->list.next->offset + now) < 0xFFFF){
            DEBUG("offset+now smaller than FFFF\n");
            /* The added entry became the new list head */
            ztimer->ops->set(ztimer, ztimer->list.next->offset);
            //cancel ovf alarm cause it won't happen before next alarm
            ztimer->ops->cancel_ovf(ztimer);

        } else{
            DEBUG("offset+now bigger than FFFF\n");
            //set overflow callback
            ztimer->ops->set_ovf_alarm(ztimer);
            //cancel possible next alarm
            ztimer->ops->cancel(ztimer);
        }
        //only if we trigger one alarm, we update the reference counts
        ztimer->list.offset = now;
    }
    else {
        DEBUG("relaunch_NOT_needed-global off=0x%08lx \n",ztimer->list.offset);
        ztimer->ops->cancel(ztimer);
        ztimer->ops->cancel_ovf(ztimer);
        ztimer->list.offset = 0;
    }
    DEBUG("exit:_ztimer_relaunch_if_needed()\n");
}

void _ztimer_overflow_callback(ztimer_dev_t *ztimer){
    (void) ztimer;
    DEBUG("_ztimer_overflow_callback()-global ref counts=0x%08lx \n",ztimer->list.offset);
    uint16_t elapsed_counts = 0xFFFF - ztimer->list.offset;
    DEBUG("Updating with=0x%08x \n", elapsed_counts);

//  update list alarm values with elapsed time
    _update_head_offset(ztimer, elapsed_counts);

    _ztimer_relaunch_if_needed(ztimer);
}

void ztimer_handler(ztimer_dev_t *ztimer)
{
    DEBUG("enter:ztimer_handler()\n");
    DEBUG("ztimer_handler(): glob_off before update=0x%08"PRIx32" now=0x%08"PRIx32"\n",
            ztimer->list.offset, ztimer->ops->now(ztimer));
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    //first timer triggered the cb, add elapsed time to global offset
//    uint32_t elapsed_count = ztimer->list.next->offset;
//    ztimer->list.offset += elapsed_count;
    //en teoria esto de abajo no habria que ponerlo xq
    //las alarmas se disparan solo si son menor a FFFF
//    ztimer->list.offset &= 0xFFFF;
    //update the alarm that just triggered
    ztimer->list.next->offset = 0;
    ztimer_t *entry = _now_next(ztimer);
    while (entry) {
        DEBUG("ztimer_handler(): trigger %p->%p at %"PRIx32"\n",
                (void *)entry, (void *)entry->base.next, ztimer->ops->now(ztimer));
        entry->callback(entry->arg);
        _del_entry_from_list(ztimer, &entry->base);
        entry = _now_next(ztimer);
//        if (!entry) {
//            /* See if any more alarms expired during callback processing */
//            /* This reduces the number of implicit calls to ztimer->ops->now() */
//            _update_head_offset(ztimer, elapsed_count);
//            entry = _now_next(ztimer);
//        }
    }

    _ztimer_relaunch_if_needed(ztimer);

    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    DEBUG("ztimer_handler(): %p done.\n", (void *)ztimer);
    if (!irq_is_in()) {
        thread_yield_higher();
    }
    DEBUG("exit:ztimer_handler()\n");
}

static void _ztimer_print(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = &ztimer->list;
    do {
        printf("0x%08x:%"PRIx32"->", (unsigned)entry, entry->offset);
    } while ((entry = entry->next));
    puts("");
}
