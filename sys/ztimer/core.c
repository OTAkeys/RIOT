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

#define ENABLE_DEBUG (1)
#include "debug.h"

#define SPIN_VALUE  300

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry);
static void _update_head_offset(ztimer_dev_t *ztimer, uint16_t elapsed_counts);
static void _ztimer_relaunch_if_needed(ztimer_dev_t *ztimer);
static void _ztimer_print(ztimer_dev_t *ztimer);
static ztimer_t *_now_next(ztimer_dev_t *ztimer);

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
    // TODO: as for now we don't manage 'val' in seconds, only in number of counts
    uint32_t now = ztimer->ops->now(ztimer);
    DEBUG("enter:ztimer_set:now:%"PRIu32" ref cnt %"PRIu32
            " target %"PRIu32"\n", now, ztimer->list.offset, val);

    unsigned state = irq_disable();
    //delete from list if timer is already present
    _del_entry_from_list(ztimer, &entry->base);
    //update target alarm to current N counts value
    entry->base.offset =  val;
//    //only if the new timer has to be the first
//    //update counts
//    if(val < ztimer->list.next->offset){
//        DEBUG("timer first on list\n");
//        uint32_t ref_counts = ztimer->list.offset;
//        //check if an OVF didn't happened.
//        uint16_t diff = now >= ref_counts ? now - ref_counts :
//                (0xFFFF - ref_counts + now);
//        _update_head_offset(ztimer, diff);
//    }
    DEBUG("insert to queue offset %"PRIu32"\n", entry->base.offset);
    _add_entry_to_list(ztimer, &entry->base);
    //only if the timer we just set is the first that should
    //trigger we update the offset and relaunch if needed
    if(ztimer->list.next == &entry->base) {
        _ztimer_relaunch_if_needed(ztimer);
    }
    irq_restore(state);
    DEBUG("exit:ztimer_set()\n");
}

static void _add_entry_to_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    uint32_t delta_sum = 0;
    DEBUG("enter:_add_entry_to_list\n");
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

    /* Insert into list */
    entry->next = list->next;
    entry->offset -= delta_sum;
    if (entry->next) {
        entry->next->offset -= entry->offset;
    }
    list->next = entry;
    DEBUG("_add_entry_to_list() %p offset %"PRIu32"\n", (void *)entry, entry->offset);
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }
    DEBUG("exit:_add_entry_to_list\n");
}


//update all timer's offsets in list with the elapsed counts.
static void _update_head_offset(ztimer_dev_t *ztimer, uint16_t elapsed_counts)
{
    DEBUG("enter:update_head_offset:ELAPSED COUNTS=%"PRIu32"\n", (uint32_t)elapsed_counts);
    ztimer_base_t *next_timer = ztimer->list.next;
    if (next_timer) {
        do {
            if (elapsed_counts <= next_timer->offset) {
                next_timer->offset -= elapsed_counts;
                break;
            } else{
                elapsed_counts -= next_timer->offset;
                DEBUG("WARNING!!! elapsed time %d with "
                "entry offset %"PRIu32"\n", elapsed_counts, next_timer->offset);
                next_timer->offset = 0;
                if (elapsed_counts) {
                    /* skip timers with offset == 0 */
                    do {
                        next_timer = next_timer->next;
                    } while (next_timer && (next_timer->offset == 0));
                }
            }
        } while (elapsed_counts && next_timer);
    }

    //check triggered timers on time update
    ztimer_t *entry = _now_next(ztimer);
    while(entry) {
        DEBUG("trig timer while cb=%p\n", entry);
        _del_entry_from_list(ztimer, &entry->base);
        entry->callback(entry->arg);
        //TODO count time in cb and update timers
        entry = _now_next(ztimer);
    }

//    ztimer->list.offset = ztimer->ops->now(ztimer);
    ztimer->list.offset += elapsed_counts;
    DEBUG("update ref cnt=%ld\n",ztimer->list.offset);
    if (ENABLE_DEBUG) {
        _ztimer_print(ztimer);
    }

    DEBUG("exit:update_head_offset()\n");
}

static void _del_entry_from_list(ztimer_dev_t *ztimer, ztimer_base_t *entry)
{
    DEBUG("enter:_del_entry_from_list\n");
    ztimer_base_t *list = &ztimer->list;
    while (list->next) {
        ztimer_base_t *list_entry = list->next;
        if (list_entry == entry) {
            DEBUG("del_entry_from_list, removing %p\n", (void *)list->next);
            list->next = entry->next;
            if (list->next) {
                list_entry = list->next;
                list_entry->offset += entry->offset;
            }
            break;
        }
        list = list->next;
    }
//    if (ENABLE_DEBUG) {
//        _ztimer_print(ztimer);
//    }
    DEBUG("exit:_del_entry_from_list()\n");
}


//only return timer that should be fired
static ztimer_t *_now_next(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = ztimer->list.next;

    if (entry && (entry->offset <= SPIN_VALUE)) {
        ztimer->list.next = entry->next;
        return (ztimer_t*)entry;
    }
    else {
        return NULL;
    }
}

static void _ztimer_relaunch_if_needed(ztimer_dev_t *ztimer)
{
    DEBUG("enter:ztimer_relaunch_if_needed\n");
    uint16_t now = ztimer->ops->now(ztimer);
    DEBUG("now %d ref cnt=%ld\n", now, ztimer->list.offset);

//    if(ztimer->list.next->offset < SPIN_VALUE){
//        uint16_t delta = ztimer->list.next->offset;
//        DEBUG("only spin=%d\n", delta);
//        ztimer_t *entry = (ztimer_t*)ztimer->list.next;
//        _del_entry_from_list(ztimer, ztimer->list.next);
//        //as the value is really low, spin till trigger
//        while((now+delta) < ztimer->ops->now(ztimer));
//        entry->callback(entry->arg);
//        //remove timer
//
//    }

    if (ztimer->list.next) {
        //if next target should fire before next overflow set it!
        if((ztimer->list.next->offset + now) < 0xFFFF){
            DEBUG("offset+now smaller than FFFF\n");
            /* The added entry became the new list head */
            ztimer->ops->set(ztimer, ztimer->list.next->offset + now);
            ztimer->ops->cancel_ovf(ztimer);
        } else{
            //otherwise set ovf alarm
            DEBUG("offset+now bigger than FFFF\n");
            ztimer->ops->set_ovf_alarm(ztimer);
            //cancel possible next alarm
            ztimer->ops->cancel(ztimer);
        }
    }
    else {
        ztimer->ops->cancel(ztimer);
        ztimer->ops->cancel_ovf(ztimer);
    }
    //update ref counts
    ztimer->list.offset = now;
    DEBUG("ref cnt=%ld updated\n", ztimer->list.offset);
    DEBUG("exit:ztimer_relaunch_if_needed\n");
}

void _ztimer_overflow_callback(ztimer_dev_t *ztimer){
    (void) ztimer;
    uint16_t now_before_cb = 0, now = 0;
    //update the alarm that just triggered and reference counts
     ztimer->list.next->offset -= (0xFFFF - ztimer->list.offset);

     ztimer_t *entry = _now_next(ztimer);
     now_before_cb = ztimer->ops->now(ztimer);
     while (entry) {
         if (ENABLE_DEBUG) {
             _ztimer_print(ztimer);
         }
         _del_entry_from_list(ztimer, &entry->base);

         //callback might take a lot of counts but it should not take more than
         //two complete timer rounds
         entry->callback(entry->arg);
         entry = _now_next(ztimer);
         if (!entry) {
             //check that no overflow came in between
             now = ztimer->ops->now(ztimer);
             /* See if any more alarms expired during callback processing */
             /* This reduces the number of implicit calls to ztimer->ops->now() */
             _update_head_offset(ztimer, now_before_cb < now ? now - now_before_cb :
                     (0xFFFF - now_before_cb + now));
             now_before_cb = now;
             entry = _now_next(ztimer);
         }
     }

     _ztimer_relaunch_if_needed(ztimer);

     if (!irq_is_in()) {
         thread_yield_higher();
     }
    DEBUG("exit ztimer_overflow_callback\n");
}

void ztimer_handler(ztimer_dev_t *ztimer)
{
    DEBUG("enter:ztimer_handler()\n");
    DEBUG("ztimer_handler(): ref cnt=%"PRIu32" now=%"PRIu32"\n",
            ztimer->list.offset, ztimer->ops->now(ztimer));
    uint16_t now_before_cb = 0, now = 0;

    //disable callbacks
    ztimer->ops->cancel(ztimer);
    ztimer->ops->cancel_ovf(ztimer);

    //update the alarm that just triggered and reference counts
    ztimer->list.next->offset = 0;

    ztimer_t *entry = _now_next(ztimer);
    now_before_cb = ztimer->ops->now(ztimer);
    while (entry) {
        if (ENABLE_DEBUG) {
            _ztimer_print(ztimer);
        }
        _del_entry_from_list(ztimer, &entry->base);

        //callback might take a lot of counts
        entry->callback(entry->arg);
        entry = _now_next(ztimer);
        if (!entry) {
            //check that no overflow came in between
            now = ztimer->ops->now(ztimer);
            /* See if any more alarms expired during callback processing */
            /* This reduces the number of implicit calls to ztimer->ops->now() */
            _update_head_offset(ztimer, now_before_cb < now ? now - now_before_cb :
                    (0xFFFF - now_before_cb + now));
            now_before_cb = now;
            entry = _now_next(ztimer);
        }
    }

    _ztimer_relaunch_if_needed(ztimer);

    if (!irq_is_in()) {
        thread_yield_higher();
    }
    DEBUG("exit:ztimer_handler()\n");
}

static void _ztimer_print(ztimer_dev_t *ztimer)
{
    ztimer_base_t *entry = &ztimer->list;
    do {
        printf("0x%"PRIx16":%lu->", (unsigned)entry, entry->offset);
    } while ((entry = entry->next));
    puts("");
}
