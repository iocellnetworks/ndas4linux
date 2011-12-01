#ifdef TCPIP_PORT
#include "_stdio.h"
#include "_global.h"
#else
#include <stdio.h>
#include "global.h"
#endif


#include "mbuf.h"
#include "iface.h"
#include "arp.h"
#include "ip.h"
#include "enet.h"
UCHAR Ether_bdcst[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Convert Ethernet header in host form to network mbuf */
void htonether(struct ether *ether, struct mbuf **bpp)
{
    UCHAR *cp;

    if (bpp == NULL)
        return;
    pushdown(bpp, NULL, ETHERLEN);
    cp = (*bpp)->data;
    memcpy(cp, ether->dest, EADDR_LEN);
    cp += EADDR_LEN;
    memcpy(cp, ether->source, EADDR_LEN);
    cp += EADDR_LEN;
    put16(cp, ether->type);
}

/* Extract Ethernet header */
INT32 ntohether(struct ether * ether, struct mbuf ** bpp)
{
    pullup(bpp, ether->dest, EADDR_LEN);
    pullup(bpp, ether->source, EADDR_LEN);
    ether->type = pull16(bpp);
    return ETHERLEN;
}

/* Format an Ethernet address into a printable ascii string */
char *pether(char *out, UCHAR * addr)
{
    port_sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1],
                 addr[2], addr[3], addr[4], addr[5]);
    return out;
}

/* Convert an Ethernet address from Hex/ASCII to binary */
INT32 gether(UCHAR * out, char *cp)
{
    INT32 i;

    for (i = 6; i != 0; i--)
    {
        *out++ = htoi(cp);
        if ((cp = strchr(cp, ':')) == NULL)    /* Find delimiter */
            break;
        cp++;                    /* and skip over it */
    }
    return i;
}

/* Send an IP datagram on Ethernet */
/* bpp        - Buffer to send */
/* iface    - Pointer to interface control block */
/* gateway    - IP address of next hop */
/* tos        - Type of service, not used */
INT32 enet_send(struct mbuf ** bpp, struct iface * iface, INT32 gateway,
                UCHAR tos)
{
    UCHAR *egate;

    if (gateway == 0xffffffff)
        egate = Ether_bdcst; 
    else if (IS_MULTICAST(gateway))
    {
        static UCHAR ether_mcast[EADDR_LEN];

        GET_MCAST_MAPPING(gateway, ether_mcast);
        egate = ether_mcast;
    }
    else
        egate = res_arp(iface, ARP_ETHER, gateway, bpp);

/*    debug_outs("enet_send: gateway 0x%08x\r\n", gateway); */

    if (egate != NULL)
        return (*iface->output) (iface, egate, iface->hwaddr, IP_TYPE, bpp);
    return 0;
}

/* Send a packet with Ethernet header */
/* iface    - Pointer to interface control block */
/* dest        - Destination Ethernet address */
/* source    - Source Ethernet address */
/* type        - Type field */
/* bpp        - Data field */
INT32 enet_output(struct iface * iface, UCHAR * dest, UCHAR * source,
                  UINT32 type, struct mbuf ** bpp)
{
    struct ether ep;

    memcpy(ep.dest, dest, EADDR_LEN);
    memcpy(ep.source, source, EADDR_LEN);
    ep.type = type;
    
/*             
    debug_outs("enet_output: dest %02x%02x%02x%02x%02x%02x"
                " source %02x%02x%02x%02x%02x%02x\r\n", 
                dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], 
                source[0], source[1], source[2], source[3], source[4], source[5] );
*/
    htonether(&ep, bpp);
    return (*iface->raw) (iface, bpp);
}

#ifdef INCLUDE_NDAS
#define LPX_TYPE    0x88ad    /* Type field for XIMETA LPX */        
extern INT32 lpx_input(struct iface *i_iface, struct mbuf **bpp); /* In NDAS:sal/net.c */
#endif

/* Process incoming Ethernet packets. Shared by all ethernet drivers. */
void eproc(struct iface *iface, struct mbuf **bpp)
{
    struct ether hdr;

    /* Remove Ethernet header and kick packet upstairs */
    ntohether(&hdr, bpp);

    switch (hdr.type)
    {
        case REVARP_TYPE:
        case ARP_TYPE:
            arp_input(iface, bpp);
            break;
        case IP_TYPE:
            ip_route(iface, bpp, hdr.dest[0] & 1);
            break;
#ifdef INCLUDE_NDAS
        case LPX_TYPE: 
/*            debug_outs("lpx packet rxed: %d\r\n", len_p(*bpp)); */
            pushdown(bpp, NULL, ETHERLEN); /* Attach header again */
            lpx_input(iface, bpp);
            break;
#endif
        default:
            free_p(bpp);
            break;
    }
}

INT32 ether_forus(struct iface *iface, struct mbuf *bp)
{
    /* Just look at the multicast bit */

    if (bp->data[0] & 1)
        return 0;
    else
        return 1;
}
