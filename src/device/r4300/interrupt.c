/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - interrupt.c                                              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define M64P_CORE_PROTOTYPES 1

#include "interrupt.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/ai/ai_controller.h"
#include "device/pi/pi_controller.h"
#include "device/pifbootrom/pifbootrom.h"
#include "device/r4300/cached_interp.h"
#include "device/r4300/exception.h"
#include "device/r4300/mi_controller.h"
#include "device/r4300/new_dynarec/new_dynarec.h"
#include "device/r4300/r4300_core.h"
#include "device/r4300/recomp.h"
#include "device/rdp/rdp_core.h"
#include "device/rsp/rsp_core.h"
#include "device/si/si_controller.h"
#include "device/vi/vi_controller.h"
#include "main/main.h"
#include "main/savestates.h"


/***************************************************************************
 * Pool of Single Linked List Nodes
 **************************************************************************/

static struct node* alloc_node(struct pool* p);
static void free_node(struct pool* p, struct node* node);
static void clear_pool(struct pool* p);


/* node allocation/deallocation on a given pool */
static struct node* alloc_node(struct pool* p)
{
    /* return NULL if pool is too small */
    if (p->index >= INTERRUPT_NODES_POOL_CAPACITY) {
        return NULL;
    }

    return p->stack[p->index++];
}

static void free_node(struct pool* p, struct node* node)
{
    if (p->index == 0 || node == NULL) {
        return;
    }

    p->stack[--p->index] = node;
}

/* release all nodes */
static void clear_pool(struct pool* p)
{
    size_t i;

    for (i = 0; i < INTERRUPT_NODES_POOL_CAPACITY; ++i) {
        p->stack[i] = &p->nodes[i];
    }

    p->index = 0;
}

/***************************************************************************
 * Interrupt Queue
 **************************************************************************/

static void clear_queue(struct interrupt_queue* q)
{
    q->first = NULL;
    clear_pool(&q->pool);
}

static int before_event(const struct cp0* cp0, unsigned int evt1, unsigned int evt2, int type2)
{
    const uint32_t* cp0_regs = r4300_cp0_regs();
    uint32_t count = cp0_regs[CP0_COUNT_REG];

    if (evt1 - count < UINT32_C(0x80000000))
    {
        if (evt2 - count < UINT32_C(0x80000000))
        {
            if ((evt1 - count) < (evt2 - count)) return 1;
            else return 0;
        }
        else
        {
            if ((count - evt2) < UINT32_C(0x10000000))
            {
                switch(type2)
                {
                    case SPECIAL_INT:
                        if (cp0->special_done) return 1;
                        else return 0;
                        break;
                    default:
                        return 0;
                }
            }
            else return 1;
        }
    }
    else return 0;
}

void add_interrupt_event(struct cp0* cp0, int type, unsigned int delay)
{
    const uint32_t* cp0_regs = r4300_cp0_regs();
    add_interrupt_event_count(cp0, type, cp0_regs[CP0_COUNT_REG] + delay);
}

void add_interrupt_event_count(struct cp0* cp0, int type, unsigned int count)
{
    struct node* event;
    struct node* e;
    int special;
    const uint32_t* cp0_regs = r4300_cp0_regs();
    unsigned int* cp0_next_interrupt = r4300_cp0_next_interrupt();

    special = (type == SPECIAL_INT);

    if (cp0_regs[CP0_COUNT_REG] > UINT32_C(0x80000000)) {
        cp0->special_done = 0;
    }

    if (get_event(&cp0->q, type)) {
        DebugMessage(M64MSG_WARNING, "two events of type 0x%x in interrupt queue", type);
        /* FIXME: hack-fix for freezing in Perfect Dark
         * http://code.google.com/p/mupen64plus/issues/detail?id=553
         * https://github.com/mupen64plus-ae/mupen64plus-ae/commit/802d8f81d46705d64694d7a34010dc5f35787c7d
         */
        return;
    }

    event = alloc_node(&cp0->q.pool);
    if (event == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Failed to allocate node for new interrupt event");
        return;
    }

    event->data.count = count;
    event->data.type = type;

    if (cp0->q.first == NULL)
    {
        cp0->q.first = event;
        event->next = NULL;
        *cp0_next_interrupt = cp0->q.first->data.count;
    }
    else if (before_event(cp0, count, cp0->q.first->data.count, cp0->q.first->data.type) && !special)
    {
        event->next = cp0->q.first;
        cp0->q.first = event;
        *cp0_next_interrupt = cp0->q.first->data.count;
    }
    else
    {
        for (e = cp0->q.first;
            e->next != NULL &&
            (!before_event(cp0, count, e->next->data.count, e->next->data.type) || special);
            e = e->next);

        if (e->next == NULL)
        {
            e->next = event;
            event->next = NULL;
        }
        else
        {
            if (!special)
                for(; e->next != NULL && e->next->data.count == count; e = e->next);

            event->next = e->next;
            e->next = event;
        }
    }
}

static void remove_interrupt_event(struct cp0* cp0)
{
    struct node* e;
    const uint32_t* cp0_regs = r4300_cp0_regs();
    uint32_t count = cp0_regs[CP0_COUNT_REG];
    unsigned int* cp0_next_interrupt = r4300_cp0_next_interrupt();

    e = cp0->q.first;
    cp0->q.first = e->next;
    free_node(&cp0->q.pool, e);

    *cp0_next_interrupt = (cp0->q.first != NULL
         && (cp0->q.first->data.count > count
         || (count - cp0->q.first->data.count) < UINT32_C(0x80000000)))
        ? cp0->q.first->data.count
        : 0;
}

unsigned int get_event(const struct interrupt_queue* q, int type)
{
    const struct node* e = q->first;

    if (e == NULL) {
        return 0;
    }

    if (e->data.type == type) {
        return e->data.count;
    }

    for (; e->next != NULL && e->next->data.type != type; e = e->next);

    return (e->next != NULL)
        ? e->next->data.count
        : 0;
}

int get_next_event_type(const struct interrupt_queue* q)
{
    return (q->first == NULL)
        ? 0
        : q->first->data.type;
}

void remove_event(struct interrupt_queue* q, int type)
{
    struct node* to_del;
    struct node* e = q->first;

    if (e == NULL) {
        return;
    }

    if (e->data.type == type)
    {
        q->first = e->next;
        free_node(&q->pool, e);
    }
    else
    {
        for (; e->next != NULL && e->next->data.type != type; e = e->next);

        if (e->next != NULL)
        {
            to_del = e->next;
            e->next = to_del->next;
            free_node(&q->pool, to_del);
        }
    }
}

void translate_event_queue(struct cp0* cp0, unsigned int base)
{
    struct node* e;
    const uint32_t* cp0_regs = r4300_cp0_regs();

    remove_event(&cp0->q, COMPARE_INT);
    remove_event(&cp0->q, SPECIAL_INT);

    for (e = cp0->q.first; e != NULL; e = e->next)
    {
        e->data.count = (e->data.count - cp0_regs[CP0_COUNT_REG]) + base;
    }
    add_interrupt_event_count(cp0, COMPARE_INT, cp0_regs[CP0_COMPARE_REG]);
    add_interrupt_event_count(cp0, SPECIAL_INT, 0);
}

int save_eventqueue_infos(struct cp0* cp0, char *buf)
{
    int len;
    struct node* e;

    len = 0;

    for (e = cp0->q.first; e != NULL; e = e->next)
    {
        memcpy(buf + len    , &e->data.type , 4);
        memcpy(buf + len + 4, &e->data.count, 4);
        len += 8;
    }

    *((unsigned int*)&buf[len]) = 0xFFFFFFFF;
    return len+4;
}

void load_eventqueue_infos(struct cp0* cp0, const char *buf)
{
    int len = 0;

    clear_queue(&cp0->q);

    while (*((const unsigned int*)&buf[len]) != 0xFFFFFFFF)
    {
        int type = *((const unsigned int*)&buf[len]);
        unsigned int count = *((const unsigned int*)&buf[len+4]);
        add_interrupt_event_count(cp0, type, count);
        len += 8;
    }
}

void init_interrupt(struct cp0* cp0)
{
    /* XXX: VI doesn't really belongs here */
    struct vi_controller* vi = &g_dev.vi;

    cp0->special_done = 1;

    vi->delay = vi->next_vi = 5000;

    clear_queue(&cp0->q);
    add_interrupt_event_count(cp0, VI_INT, vi->next_vi);
    add_interrupt_event_count(cp0, SPECIAL_INT, 0);
}

void check_interrupt(struct r4300_core* r4300)
{
    struct node* event;
    uint32_t* cp0_regs = r4300_cp0_regs();
    unsigned int* cp0_next_interrupt = r4300_cp0_next_interrupt();

    if (r4300->mi.regs[MI_INTR_REG] & r4300->mi.regs[MI_INTR_MASK_REG]) {
        cp0_regs[CP0_CAUSE_REG] = (cp0_regs[CP0_CAUSE_REG] | CP0_CAUSE_IP2) & ~CP0_CAUSE_EXCCODE_MASK;
    }
    else {
        cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_IP2;
    }
    if ((cp0_regs[CP0_STATUS_REG] & (CP0_STATUS_IE | CP0_STATUS_EXL | CP0_STATUS_ERL)) != CP0_STATUS_IE) {
        return;
    }
    if (cp0_regs[CP0_STATUS_REG] & cp0_regs[CP0_CAUSE_REG] & UINT32_C(0xFF00))
    {
        event = alloc_node(&r4300->cp0.q.pool);

        if (event == NULL)
        {
            DebugMessage(M64MSG_ERROR, "Failed to allocate node for new interrupt event");
            return;
        }

        event->data.count = *cp0_next_interrupt = cp0_regs[CP0_COUNT_REG];
        event->data.type = CHECK_INT;

        if (r4300->cp0.q.first == NULL)
        {
            r4300->cp0.q.first = event;
            event->next = NULL;
        }
        else
        {
            event->next = r4300->cp0.q.first;
            r4300->cp0.q.first = event;

        }
    }
}

static void wrapped_exception_general(struct r4300_core* r4300)
{
#ifdef NEW_DYNAREC
    uint32_t* cp0_regs = r4300_cp0_regs();
    if (r4300->emumode == EMUMODE_DYNAREC) {
        cp0_regs[CP0_EPC_REG] = (pcaddr&~3)-(pcaddr&1)*4;
        pcaddr = 0x80000180;
        cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_EXL;
        if (pcaddr & 1) {
          cp0_regs[CP0_CAUSE_REG] |= CP0_CAUSE_BD;
        }
        else {
          cp0_regs[CP0_CAUSE_REG] &= ~CP0_CAUSE_BD;
        }
        pending_exception=1;
    } else {
        exception_general(r4300);
    }
#else
    exception_general(r4300);
#endif
}

void raise_maskable_interrupt(struct r4300_core* r4300, uint32_t cause)
{
    uint32_t* cp0_regs = r4300_cp0_regs();
    cp0_regs[CP0_CAUSE_REG] = (cp0_regs[CP0_CAUSE_REG] | cause) & ~CP0_CAUSE_EXCCODE_MASK;

    if (!(cp0_regs[CP0_STATUS_REG] & cp0_regs[CP0_CAUSE_REG] & UINT32_C(0xff00))) {
        return;
    }

    if ((cp0_regs[CP0_STATUS_REG] & (CP0_STATUS_IE | CP0_STATUS_EXL | CP0_STATUS_ERL)) != CP0_STATUS_IE) {
        return;
    }

    wrapped_exception_general(r4300);
}

static void special_int_handler(struct cp0* cp0)
{
    const uint32_t* cp0_regs = r4300_cp0_regs();

    if (cp0_regs[CP0_COUNT_REG] > UINT32_C(0x10000000)) {
        return;
    }


    cp0->special_done = 1;
    remove_interrupt_event(cp0);
    add_interrupt_event_count(cp0, SPECIAL_INT, 0);
}

static void compare_int_handler(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs();

    remove_interrupt_event(&r4300->cp0);

    cp0_regs[CP0_COUNT_REG] += r4300->cp0.count_per_op;
    add_interrupt_event_count(&r4300->cp0, COMPARE_INT, cp0_regs[CP0_COMPARE_REG]);
    cp0_regs[CP0_COUNT_REG] -= r4300->cp0.count_per_op;

    raise_maskable_interrupt(r4300, CP0_CAUSE_IP7);
}

static void hw2_int_handler(struct r4300_core* r4300)
{
    uint32_t* cp0_regs = r4300_cp0_regs();
    // Hardware Interrupt 2 -- remove interrupt event from queue
    remove_interrupt_event(&r4300->cp0);

    cp0_regs[CP0_STATUS_REG] = (cp0_regs[CP0_STATUS_REG] & ~(CP0_STATUS_SR | CP0_STATUS_TS | UINT32_C(0x00080000))) | CP0_STATUS_IM4;
    cp0_regs[CP0_CAUSE_REG] = (cp0_regs[CP0_CAUSE_REG] | CP0_CAUSE_IP4) & ~CP0_CAUSE_EXCCODE_MASK;

    wrapped_exception_general(r4300);
}

/* XXX: this should only require r4300 struct not device ? */
static void nmi_int_handler(struct device* dev)
{
    struct r4300_core* r4300 = &dev->r4300;
    uint32_t* cp0_regs = r4300_cp0_regs();
    // Non Maskable Interrupt -- remove interrupt event from queue
    remove_interrupt_event(&r4300->cp0);
    // setup r4300 Status flags: reset TS and SR, set BEV, ERL, and SR
    cp0_regs[CP0_STATUS_REG] = (cp0_regs[CP0_STATUS_REG] & ~(CP0_STATUS_SR | CP0_STATUS_TS | UINT32_C(0x00080000))) | (CP0_STATUS_ERL | CP0_STATUS_BEV | CP0_STATUS_SR);
    cp0_regs[CP0_CAUSE_REG]  = 0x00000000;
    // simulate the soft reset code which would run from the PIF ROM
    pifbootrom_hle_execute(dev);
    // clear all interrupts, reset interrupt counters back to 0
    cp0_regs[CP0_COUNT_REG] = 0;
    g_gs_vi_counter = 0;
    init_interrupt(&r4300->cp0);
    // clear the audio status register so that subsequent write_ai() calls will work properly
    dev->ai.regs[AI_STATUS_REG] = 0;
    // set ErrorEPC with the last instruction address
    cp0_regs[CP0_ERROREPC_REG] = *r4300_pc();
    // reset the r4300 internal state
    if (r4300->emumode != EMUMODE_PURE_INTERPRETER)
    {
        // clear all the compiled instruction blocks and re-initialize
        free_blocks(r4300);
        init_blocks(r4300);
    }
    // adjust ErrorEPC if we were in a delay slot, and clear the r4300->delay_slot and r4300->dyna_interp flags
    if(r4300->delay_slot==1 || r4300->delay_slot==3)
    {
        cp0_regs[CP0_ERROREPC_REG]-=4;
    }
    r4300->delay_slot = 0;
    r4300->dyna_interp = 0;
    // set next instruction address to reset vector
    r4300->cp0.last_addr = UINT32_C(0xa4000040);
    generic_jump_to(r4300, UINT32_C(0xa4000040));

#ifdef NEW_DYNAREC
    if (r4300->emumode == EMUMODE_DYNAREC)
    {
        uint32_t* cp0_next_regs = r4300_cp0_regs();
        cp0_next_regs[CP0_ERROREPC_REG]=(pcaddr&~3)-(pcaddr&1)*4;
        pcaddr = 0xa4000040;
        pending_exception = 1;
        invalidate_all_pages();
    }
#endif
}


static void reset_hard(struct device* dev)
{
    struct r4300_core* r4300 = &dev->r4300;

    poweron_device(dev);

    pifbootrom_hle_execute(dev);
    r4300->cp0.last_addr = UINT32_C(0xa4000040);
    *r4300_cp0_next_interrupt() = 624999;
    init_interrupt(&r4300->cp0);
    if (r4300->emumode != EMUMODE_PURE_INTERPRETER)
    {
        free_blocks(r4300);
        init_blocks(r4300);
    }
    generic_jump_to(r4300, r4300->cp0.last_addr);
}


void gen_interrupt(void)
{
    struct r4300_core* r4300 = &g_dev.r4300;
    uint32_t* cp0_regs = r4300_cp0_regs();
    unsigned int* cp0_next_interrupt = r4300_cp0_next_interrupt();

    if (*r4300_stop() == 1)
    {
        g_gs_vi_counter = 0; // debug
        dyna_stop();
    }

    if (!r4300->cp0.interrupt_unsafe_state)
    {
        if (savestates_get_job() == savestates_job_load)
        {
            savestates_load();
            return;
        }

        if (r4300->reset_hard_job)
        {
            reset_hard(&g_dev);
            return;
        }
    }

    if (r4300->skip_jump)
    {
        uint32_t dest = r4300->skip_jump;
        r4300->skip_jump = 0;

        *cp0_next_interrupt = (r4300->cp0.q.first->data.count > cp0_regs[CP0_COUNT_REG]
                || (cp0_regs[CP0_COUNT_REG] - r4300->cp0.q.first->data.count) < UINT32_C(0x80000000))
            ? r4300->cp0.q.first->data.count
            : 0;

        r4300->cp0.last_addr = dest;
        generic_jump_to(r4300, dest);
        return;
    }

    switch (r4300->cp0.q.first->data.type)
    {
        case SPECIAL_INT:
            special_int_handler(&r4300->cp0);
            break;

        case VI_INT:
            remove_interrupt_event(&r4300->cp0);
            vi_vertical_interrupt_event(&g_dev.vi);
            break;

        case COMPARE_INT:
            compare_int_handler(r4300);
            break;

        case CHECK_INT:
            remove_interrupt_event(&r4300->cp0);
            wrapped_exception_general(r4300);
            break;

        case SI_INT:
            remove_interrupt_event(&r4300->cp0);
            si_end_of_dma_event(&g_dev.si);
            break;

        case PI_INT:
            remove_interrupt_event(&r4300->cp0);
            pi_end_of_dma_event(&g_dev.pi);
            break;

        case AI_INT:
            remove_interrupt_event(&r4300->cp0);
            ai_end_of_dma_event(&g_dev.ai);
            break;

        case SP_INT:
            remove_interrupt_event(&r4300->cp0);
            rsp_interrupt_event(&g_dev.sp);
            break;

        case DP_INT:
            remove_interrupt_event(&r4300->cp0);
            rdp_interrupt_event(&g_dev.dp);
            break;

        case HW2_INT:
            hw2_int_handler(r4300);
            break;

        case NMI_INT:
            nmi_int_handler(&g_dev);
            break;

        default:
            DebugMessage(M64MSG_ERROR, "Unknown interrupt queue event type %.8X.", r4300->cp0.q.first->data.type);
            remove_interrupt_event(&r4300->cp0);
            wrapped_exception_general(r4300);
            break;
    }

    if (!r4300->cp0.interrupt_unsafe_state)
    {
        if (savestates_get_job() == savestates_job_save)
        {
            savestates_save();
            return;
        }
    }
}

