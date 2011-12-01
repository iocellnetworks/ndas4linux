#include <linux/version.h>
#include <linux/module.h> 

#include <linux/netdevice.h>
#include <linux/random.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <ndas_lpx.h>

#ifdef NDAS_HIX

/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1996, 1997, 1998, 1999 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#include "uuid.h"

struct uuid {
    __u32    time_low;
    __u16    time_mid;
    __u16    time_hi_and_version;
    __u16    clock_seq;
    __u8    node[6];
};

int sal_netdev_get_address(struct net_device *dev, unsigned char* mac)
{ 
    if (dev) {
        if (unlikely(dev->addr_len != IFHWADDRLEN )) {
            //dbgl_salnet(1, "the size of mac address is %d bytes:", dev->addr_len);
            dev_put(dev);
            return -1;
        } else {
            memcpy(mac, dev->dev_addr, dev->addr_len);
            dev_put(dev);
            return 0;
        }
    } else {
        return -1;
    }
}

/*
 * Get the ethernet hardware address, if we can find it...
 */
static int get_node_id(unsigned char *node_id)
{
    struct net_device *nd = lpxitf_find_first();
    if ( !nd ) return 0;
    if ( sal_netdev_get_address(nd,node_id) != 0 ) {
        return 0;
    }
    return 1;
}

/* Assume that the gettimeofday() has microsecond granularity */
#define MAX_ADJUSTMENT 10

static int get_clock(__u32 *clock_high, __u32 *clock_low, __u16 *ret_clock_seq)
{
    static int            adjustment = 0;
    static unsigned long        last = 0;
    static __u16            clock_seq;
    unsigned long             mtime;
    __u64        clock_reg;
    
try_again:
    mtime = jiffies_to_msecs(jiffies);
    if (last == 0) {
        get_random_bytes(&clock_seq, sizeof(clock_seq));
        clock_seq &= 0x3FFF;
        last = mtime;
        last -= 1000L;
    }
    if (mtime < last) {
        clock_seq = (clock_seq+1) & 0x3FFF;
        adjustment = 0;
        last = mtime;
    } else if (mtime == last) {
        if (adjustment >= MAX_ADJUSTMENT)
            goto try_again;
        adjustment++;
    } else {
        adjustment = 0;
        last = mtime;
    }
        
    clock_reg = mtime*1000*10 + adjustment;
    clock_reg += (((__u64) 0x01B21DD2) << 32) + 0x13814000;

    *clock_high = clock_reg >> 32;
    *clock_low = clock_reg;
    *ret_clock_seq = clock_seq;
    return 0;
}


static void uuid_pack(const struct uuid *uu, uuid_t ptr)
{
    __u32    tmp;
    unsigned char    *out = ptr;

    tmp = uu->time_low;
    out[3] = (unsigned char) tmp;
    tmp >>= 8;
    out[2] = (unsigned char) tmp;
    tmp >>= 8;
    out[1] = (unsigned char) tmp;
    tmp >>= 8;
    out[0] = (unsigned char) tmp;
    
    tmp = uu->time_mid;
    out[5] = (unsigned char) tmp;
    tmp >>= 8;
    out[4] = (unsigned char) tmp;

    tmp = uu->time_hi_and_version;
    out[7] = (unsigned char) tmp;
    tmp >>= 8;
    out[6] = (unsigned char) tmp;

    tmp = uu->clock_seq;
    out[9] = (unsigned char) tmp;
    tmp >>= 8;
    out[8] = (unsigned char) tmp;

    memcpy(out+10, uu->node, 6);
}

void uuid_generate(uuid_t out)
{
    static unsigned char node_id[6];
    static int has_init = 0;
    struct uuid uu;
    __u32    clock_mid;

    if (!has_init) {
        if (get_node_id(node_id) <= 0) {            
            get_random_bytes(node_id, 6);            
            /*             
             * Set multicast bit, to prevent conflicts             
             * with IEEE 802 addresses obtained from             
             * network cards             
             */            
            node_id[0] |= 0x01;        
        }
        has_init = 1;
    }
    get_clock(&clock_mid, &uu.time_low, &uu.clock_seq);
    uu.clock_seq |= 0x8000;
    uu.time_mid = (__u16) clock_mid;
    uu.time_hi_and_version = ((clock_mid >> 16) & 0x0FFF) | 0x1000;
    memcpy(uu.node, node_id, 6);
    uuid_pack(&uu, out);
}
#endif // NDAS_HIX
