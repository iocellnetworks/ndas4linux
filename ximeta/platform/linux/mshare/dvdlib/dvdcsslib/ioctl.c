/*****************************************************************************
 * ioctl.c: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ioctl.c,v 1.23 2003/01/16 22:58:29 sam Exp $
 *
 * Authors: Markus Kuespert <ltlBeBoy@beosmail.com>
 *          Samuel Hocevar <sam@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          David Siebörger <drs-videolan@rucus.ru.ac.za>
 *          Alex Strelnikov <lelik@os2.ru>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <stdio.h>
#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include "common.h"
#include "ioctl.h"


/*****************************************************************************
 * ioctl_ReadCopyright: check whether the disc is encrypted or not
 *****************************************************************************/
int ioctl_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
    int i_ret;

    dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = i_layer;

    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );

    *pi_copyright = dvd.copyright.cpst;


    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadDiscKey: get the disc key
 *****************************************************************************/
int ioctl_ReadDiscKey( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

    dvd_struct dvd;

    memset( &dvd, 0, sizeof( dvd ) );
    dvd.type = DVD_STRUCT_DISCKEY;
    dvd.disckey.agid = *pi_agid;
    memset( dvd.disckey.value, 0, DVD_DISCKEY_SIZE );
    i_ret = ioctl( i_fd, DVD_READ_STRUCT, &dvd );
    if( i_ret < 0 )
    {
        return i_ret;
    }
    memcpy( p_key, dvd.disckey.value, DVD_DISCKEY_SIZE );
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadTitleKey: get the title key
 *****************************************************************************/
int ioctl_ReadTitleKey( int i_fd, int *pi_agid, int i_pos, uint8_t *p_key )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_TITLE_KEY;
    auth_info.lstk.agid = *pi_agid;
    auth_info.lstk.lba = i_pos;
    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );
    memcpy( p_key, auth_info.lstk.title_key, DVD_KEY_SIZE );
    return i_ret;
}


/*****************************************************************************
 * ioctl_ReportAgid: get AGID from the drive
 *****************************************************************************/
int ioctl_ReportAgid( int i_fd, int *pi_agid )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_agid = auth_info.lsa.agid;
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportChallenge: get challenge from the drive
 *****************************************************************************/
int ioctl_ReportChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_CHALLENGE;
    auth_info.lsc.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_challenge, auth_info.lsc.chal, DVD_CHALLENGE_SIZE );

    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportASF: get ASF from the drive
 *****************************************************************************/
int ioctl_ReportASF( int i_fd, int *pi_remove_me, int *pi_asf )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_ASF;
    auth_info.lsasf.asf = *pi_asf;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *pi_asf = auth_info.lsasf.asf;
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportKey1: get the first key from the drive
 *****************************************************************************/
int ioctl_ReportKey1( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_KEY1;
    auth_info.lsk.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    memcpy( p_key, auth_info.lsk.key, DVD_KEY_SIZE );
    return i_ret;
}

/*****************************************************************************
 * ioctl_InvalidateAgid: invalidate the current AGID
 *****************************************************************************/
int ioctl_InvalidateAgid( int i_fd, int *pi_agid )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_INVALIDATE_AGID;
    auth_info.lsa.agid = *pi_agid;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    return i_ret;
}

/*****************************************************************************
 * ioctl_SendChallenge: send challenge to the drive
 *****************************************************************************/
int ioctl_SendChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_CHALLENGE;
    auth_info.hsc.agid = *pi_agid;

    memcpy( auth_info.hsc.chal, p_challenge, DVD_CHALLENGE_SIZE );

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    return i_ret;
}

/*****************************************************************************
 * ioctl_SendKey2: send the second key to the drive
 *****************************************************************************/
int ioctl_SendKey2( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_KEY2;
    auth_info.hsk.agid = *pi_agid;

    memcpy( auth_info.hsk.key, p_key, DVD_KEY_SIZE );

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportRPC: get RPC status for the drive
 *****************************************************************************/
int ioctl_ReportRPC( int i_fd, int *p_type, int *p_mask, int *p_scheme )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_RPC_STATE;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    *p_type = auth_info.lrpcs.type;
    *p_mask = auth_info.lrpcs.region_mask;
    *p_scheme = auth_info.lrpcs.rpc_scheme;

    return i_ret;
}

/*****************************************************************************
 * ioctl_SendRPC: set RPC status for the drive
 *****************************************************************************/
int ioctl_SendRPC( int i_fd, int i_pdrc )
{
    int i_ret;

    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_HOST_SEND_RPC_STATE;
    auth_info.hrpcs.pdrc = i_pdrc;

    i_ret = ioctl( i_fd, DVD_AUTH, &auth_info );

    return i_ret;
}
