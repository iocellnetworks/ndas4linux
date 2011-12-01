/*****************************************************************************
 * device.h: DVD device access
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: device.c,v 1.16 2003/07/08 18:00:54 gbazin Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "dvdcss.h"
#include "common.h"
#include "css.h"
#include "libdvdcss.h"
#include "ioctl.h"
#include "device.h"

/*****************************************************************************
 * Device reading prototypes
 *****************************************************************************/
static int libc_open  ( dvdcss_t, char const * );
static int libc_seek  ( dvdcss_t, int );
static int libc_read  ( dvdcss_t, void *, int );
static int libc_readv ( dvdcss_t, struct iovec *, int );


int _dvdcss_use_ioctls( dvdcss_t dvdcss )
{
    struct stat fileinfo;
    int ret;

    ret = fstat( dvdcss->i_fd, &fileinfo );
    if( ret < 0 )
    {
        return 1;  /* What to do?  Be conservative and try to use the ioctls */
    }

    /* Complete this list and check that we test for the right things
     * (I've assumed for all OSs that 'r', (raw) device, are char devices
     *  and those that don't contain/use an 'r' in the name are block devices)
     *
     * Linux    needs a block device
     * Solaris  needs a char device
     * Darwin   needs a char device
     * OpenBSD  needs a char device
     * NetBSD   needs a char device
     * FreeBSD  can use either the block or the char device
     * BSD/OS   can use either the block or the char device
     */

    /* Check if this is a block/char device */
    if( S_ISBLK( fileinfo.st_mode ) ||
        S_ISCHR( fileinfo.st_mode ) )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int _dvdcss_open ( dvdcss_t dvdcss )
{
    char psz_debug[200];
    char const *psz_device = dvdcss->psz_device;

    snprintf( psz_debug, 199, "opening target `%s'", psz_device );
    psz_debug[199] = '\0';
    _dvdcss_debug( dvdcss, psz_debug );

    _dvdcss_debug( dvdcss, "using libc for access" );
    dvdcss->pf_seek  = libc_seek;
    dvdcss->pf_read  = libc_read;
    dvdcss->pf_readv = libc_readv;
    return libc_open( dvdcss, psz_device );
}

int _dvdcss_raw_open ( dvdcss_t dvdcss, char const *psz_device )
{
    dvdcss->i_raw_fd = open( psz_device, 0 );

    if( dvdcss->i_raw_fd == -1 )
    {
        _dvdcss_error( dvdcss, "failed opening raw device, continuing" );
        return -1;
    }
    else
    {
        dvdcss->i_read_fd = dvdcss->i_raw_fd;
    }

    return 0;
}

int _dvdcss_close ( dvdcss_t dvdcss )
{
    close( dvdcss->i_fd );

    if( dvdcss->i_raw_fd >= 0 )
    {
        close( dvdcss->i_raw_fd );
        dvdcss->i_raw_fd = -1;
    }

    return 0;
}

/* Following functions are local */

/*****************************************************************************
 * Open commands.
 *****************************************************************************/
static int libc_open ( dvdcss_t dvdcss, char const *psz_device )
{
    dvdcss->i_fd = dvdcss->i_read_fd = open( psz_device, 0 );

    if( dvdcss->i_fd == -1 )
    {
        _dvdcss_error( dvdcss, "failed opening device" );
        return -1;
    }

    dvdcss->i_pos = 0;

    return 0;
}


/*****************************************************************************
 * Seek commands.
 *****************************************************************************/
static int libc_seek( dvdcss_t dvdcss, int i_blocks )
{
    off_t   i_seek;

    if( dvdcss->i_pos == i_blocks )
    {
        /* We are already in position */
        return i_blocks;
    }

    i_seek = lseek( dvdcss->i_read_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

    if( i_seek < 0 )
    {
        _dvdcss_error( dvdcss, "seek error" );
        dvdcss->i_pos = -1;
        return i_seek;
    }

    dvdcss->i_pos = i_seek / DVDCSS_BLOCK_SIZE;

    return dvdcss->i_pos;
}

/*****************************************************************************
 * Read commands.
 *****************************************************************************/
static int libc_read ( dvdcss_t dvdcss, void *p_buffer, int i_blocks )
{
    off_t i_ret;

    i_ret = read( dvdcss->i_read_fd, p_buffer,
                  (off_t)i_blocks * DVDCSS_BLOCK_SIZE );

    if( i_ret < 0 )
    {
        _dvdcss_error( dvdcss, "read error" );
        dvdcss->i_pos = -1;
        return i_ret;
    }

    /* Handle partial reads */
    if( i_ret != (off_t)i_blocks * DVDCSS_BLOCK_SIZE )
    {
        int i_seek;

        dvdcss->i_pos = -1;
        i_seek = libc_seek( dvdcss, i_ret / DVDCSS_BLOCK_SIZE );
        if( i_seek < 0 )
        {
            return i_seek;
        }

        /* We have to return now so that i_pos isn't clobbered */
        return i_ret / DVDCSS_BLOCK_SIZE;
    }

    dvdcss->i_pos += i_ret / DVDCSS_BLOCK_SIZE;
    return i_ret / DVDCSS_BLOCK_SIZE;
}



/*****************************************************************************
 * Readv commands.
 *****************************************************************************/
static int libc_readv ( dvdcss_t dvdcss, struct iovec *p_iovec, int i_blocks )
{
    int i_read = readv( dvdcss->i_read_fd, p_iovec, i_blocks );

    if( i_read < 0 )
    {
        dvdcss->i_pos = -1;
        return i_read;
    }

    dvdcss->i_pos += i_read / DVDCSS_BLOCK_SIZE;
    return i_read / DVDCSS_BLOCK_SIZE;
}


