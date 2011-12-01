/***************************************************************************
 *   Copyright (C) 2008 by root   *
 *   root@fedora8   *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#define _GNU_SOURCE

#include <stdio.h> // printf
#include <string.h> // strncmp strlen
#include <stdlib.h> // malloc, free
#include <fcntl.h> // open
#include <unistd.h> // close
#include <ctype.h> // isalnum
#include <sys/ioctl.h> // ioctl
#include <sys/types.h> // open
#include <sys/stat.h> // open
#include <errno.h> // EINVAL
#include <getopt.h> // getopt_long
#include <linux/limits.h> // PATH_MAX for linux
#include <sys/utsname.h> // uname

#ifdef NDAS_MSHARE
#include <time.h>
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

int main(int argc, char *argv[])
{
	char buffer[512*64];
	int fd;
	int i;

	printf("Hello, world! 1228800\n");
	
	fd = open("/var/test", O_RDWR);
	
	if (fd) {

		printf("open success /var/test\n");

	} else {

		printf("open fail /var/test\n");
	}

	for (i=0; i<1228800/64; i++) {

		write(fd, buffer, 512*64);
	}

	close(fd);

	return EXIT_SUCCESS;
}
