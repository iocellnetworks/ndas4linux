/*
 * Copyright (C) 2002  Olaf Beck <olaf_sc@yahoo.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* dvdbackup version 0.1 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "inc/dvd_reader.h"
#include "inc/ifo_read.h"
#include "inc/ifo_print.h"
#include "inc/dvdcopy.h"

#define MAXNAME 256
#ifndef PATH_MAX
#define PATH_MAX MAXNAME
#endif
/*Flag for verbose mode */
int verbose;
int aspect;


/* Structs to keep title set information in */

typedef struct {
  int		size_ifo;
  int		size_menu;
  int		size_bup;
  int		number_of_vob_files;
  int		size_vob[10];
} title_set_t;

typedef struct {
  int		number_of_title_sets;
  title_set_t  *title_set;
} title_set_info_t;


typedef struct {
  int		title;
  int		title_set;
  int		vts_title;
  int		chapters;
  int		aspect_ratio;
  int		angles;
  int		audio_tracks;
  int		audio_channels;
  int		sub_pictures;
} titles_t;

typedef struct {
  int		main_title_set;
  int		number_of_titles;
  titles_t	*titles;
} titles_info_t;

typedef struct {
  int		size_ifo;
  unsigned int	ifo_startlb;
  int		size_menu;
  unsigned int 	menu_startlb;
  int		size_bup;
  unsigned int  bup_startlb;
  int		number_of_vob_files;
  int		size_vob[10];
  unsigned int 	vob_startlb[10];
} DVDRip_title_set_t;

typedef struct {
  int		number_of_title_sets;
  DVDRip_title_set_t  *title_set;
} DVDRip_title_set_info_t;

struct Partition {
    int valid;
    char VolumeDesc[128];
    uint16_t Flags;
    uint16_t Number;
    char Contents[32];
    uint32_t AccessType;
    uint32_t Start;
    uint32_t Length;
};





void bsort_max_to_min(int sector[], int title[], int size);

/* Usage:
	To gather info about the dvd:
		dvdbackup -i /dev/dvd -I

	Gerneral backup information:
		If your backup directory is /my/dvd/backup/dir/
		specified with the "-o" flag. Then dvdbackup
		will create a DVD-Video structure under
		/my/dvd/backup/dir/TITLE_NAME/VIDEO_TS.

		Since the title is "uniq" you can use the same dir
		for all your DVD backups. If it happens to have a
		generic title dvdbackup will exit with a return value
		of 2. And you will need to specify a title name with
		the -n switch.

		dvdbackup will always mimic the original DVD-Video
		structure. Hence if you e.g. use the -M (mirror) you
		will get an exact duplicate of the original. This
		means that every file will be have the same size as
		the original one. Like wise goes also for the -F
		and the -T switch.

		However the -t and (-t -s/-e) switch is a bit
		different the titles sectors will be written to the
		original file but not at the same offset as the
		original one since they may be gaps in the cell
		structure that we don't fill.


	To backup the whole DVD

		dvdbackup -M -i/dev/dvd -o/my/dvd/backup/dir/

		This action creates a valid DVD-Video structure
		that can be burned to a DVD-/+R(W) with help of
		mkisofs version 1.11a27 or later


	To backup the main feature of the DVD:

		dvdbackup -F -i/dev/dvd -o/my/dvd/backup/dir/

		This action creates a valid DVD-Video structure
		of the feature title set

		dvdbackup defaults to get the 16:9 version of the
		main feature if a 4:3 is also present on the DVD.
		To get the 4:3 version use -a 0.

		dvdbackup makes it best to make a inteligent guess
		what is the main feature of the DVD - in case it fails
		please send a bug report.


	To backup a title set

		dvdbackup -T 2 -i/dev/dvd -o/my/dvd/backup/dir/

		where "-T 2" specifies that you want to backup
		title set 2 i.e. all VTS_02_X.XXX files.

		This action creates a valid DVD-Video structure
		of the specified title set


	To backup a title:

		dvdbackup -t 1 -i/dev/dvd -o/my/dvd/backup/dir

		This action backups all cells that forms the
		specified title. Note that there can be sector
		gaps in between one cell and an other. dvdbackup
		will backup all sectors that belongs to the title
		but will skip sectors that aren't a part of the title.


	To backup a specific chapter or chapters from a title:

		dvdbackup -t 1 -s 20 -e 25 -i/dev/dvd -o/my/dvd/backup/dir

		This action will backup chapter 20 to 25 in title 1, as with
		the backup of a title there can be sector gaps between one chapter
		(cell) and on other.dvdbackup will backup all sectors that belongs
		to the title 1 chapter 20 to 25 but will skip sectors that aren't
		a part of the title 1 chapter 20 to 25.

		To backup a single chapter e.g. chapter 20 do -s 20 -e 20
		To backup from chapter 20 to the end chapter use only -s 20
		To backup to chapter 20 from the first chapter use only -e 20

		You can skip the -t switch and let the program guess the title although
		it's not recomened.

		If you specify a chapter that his higher than the last chapter of the title
		dvdbackup will turncate to the highest chapter of the title.

	Return values:
		0 on success
		1 on usage error
		2 on title name error
		-1 on failur


	Todo - i.e. what's on the agenda.
		Make the main feature guessing algoritm better. Not that it doesn't do
		it's job, but it's implementation it's that great. I would also like
		to preserve more information about the main feature since that would
		let me preform better implementations in other functions that depends
		on the titles_info_t and title_set_info_t strcutures.

		Make it possible to extract cells in a title not just chapters (very
		easy so it will definitly be in the next version).

		Make a split mirror (-S) option that divides a DVD-9 to two valid DVD-5
		video structures. This is not a trivial hack and it's my main goal
		the next month or so. It involves writing ifoedit and vobedit
		libraries in order to be able to manipulate both the IFO structures
		and the VOB files. Out of this will most probably also come tscreate
		and vtscreate which will enable you to make a very simple DVD-Video
		from MPEG-1/2 source.

*/


void usage(){
	fprintf(stderr,"\nUsage: dvdbackup [options]\n");
	fprintf(stderr,"\t-i device\twhere device is your dvd device\n");
	fprintf(stderr,"\t-v X\t\twhere X is the amount of verbosity\n");
	fprintf(stderr,"\t-I\t\tfor information about the DVD\n");
	fprintf(stderr,"\t-o directory\twhere directory is your backup target\n");
	fprintf(stderr,"\t-M\t\tbackup the whole DVD\n");
	fprintf(stderr,"\t-G\t\tbackup the whole DVD to ISO file\n");
	fprintf(stderr,"\t-F\t\tbackup the main feature of the DVD\n");
	fprintf(stderr,"\t-T X\t\tbackup title set X\n");
	fprintf(stderr,"\t-t X\t\tbackup title X\n");
	fprintf(stderr,"\t-s X\t\tbackup from chapter X\n");
	fprintf(stderr,"\t-e X\t\tbackup to chapter X\n");
	fprintf(stderr,"\t-a 0\t\tto get aspect ratio 4:3 instead of 16:9 if both are present\n");
	fprintf(stderr,"\t-h\t\tprint a brief usage message\n");
	fprintf(stderr,"\t-?\t\tprint a brief usage message\n\n");
	fprintf(stderr,"\t-i is manditory\n");
	fprintf(stderr,"\t-o is manditory except if you use -I\n");
	fprintf(stderr,"\t-a is option to the -F switch and has no effect on other options\n");
	fprintf(stderr,"\t-s and -e should prefereibly be used together with -t \n\n");
	exit(1);
}

int CheckSizeArray(const int size_array[], int reference, int target) {
	if ( (size_array[reference]/size_array[target] == 1) &&
	     ((size_array[reference] * 2 - size_array[target])/ size_array[target] == 1) &&
	     ((size_array[reference]%size_array[target] * 3) < size_array[reference]) ) {
		/* We have a dual DVD with two feature films - now lets see if they have the same amount of chapters*/
		return(1);
	} else {
		return(0);
	}
}


int CheckAudioSubChannels(int audio_audio_array[], int title_set_audio_array[],
			  int subpicture_sub_array[], int title_set_sub_array[],
			  int channels_channel_array[],int title_set_channel_array[],
			  int reference, int candidate, int title_sets) {

	int temp, i, found_audio, found_sub, found_channels;

	found_audio=0;
	temp = audio_audio_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( audio_audio_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_audio_array[i] ) {
			found_audio=1;
			break;
		}

	}

	found_sub=0;
	temp = subpicture_sub_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( subpicture_sub_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_sub_array[i] ) {
			found_sub=1;
			break;
		}

	}


	found_channels=0;
	temp = channels_channel_array[reference];
	for (i=0 ; i < title_sets ; i++ ) {
		if ( channels_channel_array[i] < temp ) {
			break;
		}
		if ( candidate == title_set_channel_array[i] ) {
			found_channels=1;
			break;
		}

	}


	return(found_audio + found_sub + found_channels);
}




int DVDWriteCells(dvd_reader_t * dvd, int cell_start_sector[], int cell_end_sector[],
   int length, int titles, title_set_info_t * title_set_info, titles_info_t * titles_info, char * targetdir,char * title_name){


	/* Loop variables */
	int i, f;


	/* Vob control */
	int vob = 0;

	/* Temp filename,dirname */
	char targetname[PATH_MAX];

	/* Write buffer */

	unsigned char * buffer=NULL;

	/* File Handler */
	int streamout;

	int size = 0;
	int left;
	int leftover = 0;

	/* Buffer size in DVD sectors */
	/* Currently set to 1MB */
	int buff = 512;
	int tsize;


	/* Offsets */
	int soffset;
	int offset;


	/* DVD handler */
	dvd_file_t   *	dvd_file=NULL;

	int title_set;
	int number_of_vob_files;

#ifdef DEBUG
	fprintf(stderr,"DVDWriteCells: length is %d\n", length);

#endif


	title_set = titles_info->titles[titles - 1].title_set;
	number_of_vob_files = title_set_info->title_set[title_set].number_of_vob_files;
#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: title set is %d\n", title_set);
		fprintf(stderr,"DVDWriteCells: vob files are %d\n", number_of_vob_files);

#endif



	/* Remove all old files silently if they exists */

	for ( i = 0 ; i < 10 ; i++ ) {
		sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",targetdir, title_name, title_set, i + 1);
#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: file is %s\n", targetname);

#endif

		unlink( targetname);

	}




	/* Loop through all sectors and find the right vob */
	for (f = 0; f < length ; f++) {

		soffset=0;
		offset=0;


		/* Now figure which vob we will use and write to that vob and if there is a left over write to the next vob*/

		for ( i = 0; i < number_of_vob_files ; i++ ) {


			tsize = title_set_info->title_set[title_set].size_vob[i];

			if (tsize%2048 != 0) {
				fprintf(stderr, "The Title VOB number %d of title set %d doesn't have a valid DVD size\n", i + 1, title_set);
				return(1);
			} else {
				soffset = offset;
				offset = offset + tsize/2048;
			}
#ifdef DEBUG
			fprintf(stderr,"DVDWriteCells: soffset is %d\n", soffset);
			fprintf(stderr,"DVDWriteCells: offset is %d\n", offset);
#endif
			/* Find out if this is the right vob */
			if( soffset <= cell_start_sector[f]  && offset > cell_start_sector[f] ) {
#ifdef DEBUG
				fprintf(stderr,"DVDWriteCells: got it \n");
#endif
				leftover=0;
				soffset = cell_start_sector[f];
				if ( cell_end_sector[f] > offset ) {
					leftover = cell_end_sector[f] - offset + 1;
				} else {
					size = cell_end_sector[f] - cell_start_sector[f] + 1;
				}
#ifdef DEBUG
				fprintf(stderr,"DVDWriteCells: size is %d\n", size);
#endif
				vob = i + 1;
				break;
			}

		}
#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: Writing soffset is %d and leftover is %d\n", soffset, leftover);
#endif




		/* Create VTS_XX_X.VOB */
		if (title_set == 0) {
			fprintf(stderr,"Don't try to copy chapters from the VMG domain there aren't any\n");
			return(1);
		} else {
			sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",targetdir, title_name, title_set, vob);
		}

#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: 1\n");
#endif


		if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(unsigned char))) == NULL) {
			fprintf(stderr, "Out of memory coping %s\n", targetname);
			return(1);
		}

#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: 2\n");
#endif


		if ((streamout = open(targetname, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
			fprintf(stderr, "Error creating %s\n", targetname);
			perror("");
			return(1);
		}

#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: 3\n");
#endif


		if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
			fprintf(stderr, "Faild opending TITLE VOB\n");
			free(buffer);
			close(streamout);
			return(1);
		}

		left = size;

#ifdef DEBUG
		fprintf(stderr,"DVDWriteCells: left is %d\n", left);
#endif

		while( left > 0 ) {

			if (buff > left) {
				buff = left;
			}
			if ( DVDReadBlocks(dvd_file,soffset,buff, buffer) != buff) {
				fprintf(stderr, "Error reading MENU VOB\n");
				free(buffer);
				DVDCloseFile(dvd_file);
				close(streamout);
				return(1);
			}


			if (write(streamout,buffer,buff *  2048) != buff * 2048) {
				fprintf(stderr, "Error writing TITLE VOB\n");
				free(buffer);
				close(streamout);
				return(1);
			}

			soffset = soffset + buff;
			left = left - buff;

		}

		DVDCloseFile(dvd_file);
		free(buffer);
		close(streamout);


		if ( leftover != 0 ) {

			vob = vob + 1;

			if (title_set == 0) {
				fprintf(stderr,"Don't try to copy chapters from the VMG domain there aren't any\n");
				return(1);
			} else {
				sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",targetdir, title_name, title_set, vob);
			}


			if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(unsigned char))) == NULL) {
				fprintf(stderr, "Out of memory coping %s\n", targetname);
				close(streamout);
				return(1);
			}


			if ((streamout = open(targetname, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1) {
				fprintf(stderr, "Error creating %s\n", targetname);
				perror("");
				return(1);
			}


			if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
				fprintf(stderr, "Faild opending TITLE VOB\n");
				free(buffer);
				close(streamout);
				return(1);
			}

			left = leftover;

			while( left > 0 ) {

				if (buff > left) {
					buff = left;
				}
				if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
					fprintf(stderr, "Error reading MENU VOB\n");
					free(buffer);
					DVDCloseFile(dvd_file);
					close(streamout);
					return(1);
				}


				if (write(streamout,buffer,buff *  2048) != buff * 2048) {
					fprintf(stderr, "Error writing TITLE VOB\n");
					free(buffer);
					close(streamout);
					return(1);
				}

				offset = offset + buff;
				left = left - buff;

			}

			DVDCloseFile(dvd_file);
			free(buffer);
			close(streamout);
		}

	}
	return(0);
}



void FreeSortArrays( int chapter_chapter_array[], int title_set_chapter_array[],
		int angle_angle_array[], int title_set_angle_array[],
		int subpicture_sub_array[], int title_set_sub_array[],
		int audio_audio_array[], int title_set_audio_array[],
		int size_size_array[], int title_set_size_array[],
		int channels_channel_array[], int title_set_channel_array[]) {


	free(chapter_chapter_array);
	free(title_set_chapter_array);

	free(angle_angle_array);
	free(title_set_angle_array);

	free(subpicture_sub_array);
	free(title_set_sub_array);

	free(audio_audio_array);
	free(title_set_audio_array);

	free(size_size_array);
	free(title_set_size_array);

	free(channels_channel_array);
	free(title_set_channel_array);
}


titles_info_t * DVDGetInfo(dvd_reader_t * _dvd) {

	/* title interation */
	int counter, i, f;

	/* Our guess */
	int candidate = 0;
	int multi = 0;
	int dual = 0;


	int titles;
	int title_sets;

	/* Arrays for chapter, angle, subpicture, audio, size, aspect, channels -  file_set relationship */

	/* Size == number_of_titles */
	int * chapter_chapter_array;
	int * title_set_chapter_array;

	int * angle_angle_array;
	int * title_set_angle_array;

	/* Size == number_of_title_sets */

	int * subpicture_sub_array;
	int * title_set_sub_array;

	int * audio_audio_array;
	int * title_set_audio_array;

	int * size_size_array;
	int * title_set_size_array;

	int * channels_channel_array;
	int * title_set_channel_array;

	/* Temp helpers */
	int channels;
	int temp;
	int found;
	int chapters_1;
	int chapters_2;
	int found_chapter;
	int number_of_multi;


	/*DVD handlers*/
	ifo_handle_t * vmg_ifo=NULL;
	dvd_file_t   *  vts_title_file=NULL;

	titles_info_t * titles_info=NULL;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}

	titles = vmg_ifo->tt_srpt->nr_of_srpts;
	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	if ((vmg_ifo->tt_srpt == 0) || (vmg_ifo->vts_atrt == 0)) {
		ifoClose(vmg_ifo);
		return(0);
	}


	/* Todo fix malloc check */
	titles_info = ( titles_info_t *)malloc(sizeof(titles_info_t));
	titles_info->titles = (titles_t *)malloc((titles)* sizeof(titles_t));

	titles_info->number_of_titles = titles;


	chapter_chapter_array = malloc(titles * sizeof(int));
	title_set_chapter_array = malloc(titles * sizeof(int));

	/*currently not used in the guessing */
	angle_angle_array = malloc(titles * sizeof(int));
	title_set_angle_array = malloc(titles * sizeof(int));


	subpicture_sub_array = malloc(title_sets * sizeof(int));
	title_set_sub_array = malloc(title_sets * sizeof(int));

	audio_audio_array = malloc(title_sets * sizeof(int));
	title_set_audio_array = malloc(title_sets * sizeof(int));

	size_size_array = malloc(title_sets * sizeof(int));
	title_set_size_array = malloc(title_sets * sizeof(int));

	channels_channel_array = malloc(title_sets * sizeof(int));
	title_set_channel_array = malloc(title_sets * sizeof(int));


	/* Interate over the titles nr_of_srpts */


	for (counter=0; counter < titles; counter++ )  {
		/* For titles_info */
		titles_info->titles[counter].title = counter + 1;
		titles_info->titles[counter].title_set = vmg_ifo->tt_srpt->title[counter].title_set_nr;
		titles_info->titles[counter].vts_title = vmg_ifo->tt_srpt->title[counter].vts_ttn;
		titles_info->titles[counter].chapters = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
		titles_info->titles[counter].angles = vmg_ifo->tt_srpt->title[counter].nr_of_angles;

		/* For main title*/
		chapter_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_ptts;
		title_set_chapter_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
		angle_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].nr_of_angles;
		title_set_angle_array[counter] = vmg_ifo->tt_srpt->title[counter].title_set_nr;
	}

	/* Interate over vmg_nr_of_title_sets */

	for (counter=0; counter < title_sets ; counter++ )  {

		/* Picture*/
		subpicture_sub_array[counter] = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
		title_set_sub_array[counter] = counter + 1;


		/* Audio */
		audio_audio_array[counter] = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
		title_set_audio_array[counter] = counter + 1;

		channels=0;
		for  (i=0; i < audio_audio_array[counter]; i++) {
			if ( channels < vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1) {
				channels = vmg_ifo->vts_atrt->vts[counter].vtstt_audio_attr[i].channels + 1;
			}

		}
		channels_channel_array[counter] = channels;
		title_set_channel_array[counter] = counter + 1;

		/* For tiles_info */
		for (f=0; f < titles_info->number_of_titles ; f++ ) {
			if ( titles_info->titles[f].title_set == counter + 1 ) {
				titles_info->titles[f].aspect_ratio = vmg_ifo->vts_atrt->vts[counter].vtstt_vobs_video_attr.display_aspect_ratio;
				titles_info->titles[f].sub_pictures = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_subp_streams;
				titles_info->titles[f].audio_tracks = vmg_ifo->vts_atrt->vts[counter].nr_of_vtstt_audio_streams;
				titles_info->titles[f].audio_channels = channels;
			}
		}

	}




	for (counter=0; counter < title_sets; counter++ ) {

		vts_title_file = DVDOpenFile(_dvd, counter + 1, DVD_READ_TITLE_VOBS);

		if (vts_title_file  != 0) {
			size_size_array[counter] = DVDFileSize(vts_title_file);
			DVDCloseFile(vts_title_file);
		} else {
			size_size_array[counter] = 0;
		}

		title_set_size_array[counter] = counter + 1;


	}


	/* Sort all arrays max to min */

	bsort_max_to_min(chapter_chapter_array, title_set_chapter_array, titles);
	bsort_max_to_min(angle_angle_array, title_set_angle_array, titles);
	bsort_max_to_min(subpicture_sub_array, title_set_sub_array, title_sets);
	bsort_max_to_min(audio_audio_array, title_set_audio_array, title_sets);
	bsort_max_to_min(size_size_array, title_set_size_array, title_sets);
	bsort_max_to_min(channels_channel_array, title_set_channel_array, title_sets);


	/* Check if the second biggest one actually can be a feature title */
	/* Here we will take do biggest/second and if that is bigger than one it's not a feauture title */
	/* Now this is simply not enough since we have to check that the diff between the two of them is small enough
	 to consider the second one a feature title we are doing two checks (biggest  + biggest - second) /second == 1
	 and biggest%second * 3 < biggest */

	if ( CheckSizeArray(size_size_array, 0, 1)  == 1 ) {
		/* We have a dual DVD with two feature films - now lets see if they have the same amount of chapters*/

		chapters_1 = 0;
		for (i=0 ; i < titles ; i++ ) {
			if (titles_info->titles[i].title_set == title_set_size_array[0] ) {
				if ( chapters_1 < titles_info->titles[i].chapters){
					chapters_1 = titles_info->titles[i].chapters;
				}
			}
		}

		chapters_2 = 0;
		for (i=0 ; i < titles ; i++ ) {
			if (titles_info->titles[i].title_set == title_set_size_array[1] ) {
				if ( chapters_2 < titles_info->titles[i].chapters){
					chapters_2 = titles_info->titles[i].chapters;
				}
			}
		}

		if (  vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio ==
			vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio) {
			/* In this case it's most likely so that we have a dual film but with different context
			They are with in the same size range and have the same aspect ratio
			I would guess that such a case is e.g. a DVD containing several episodes of a TV serie*/
			candidate = title_set_size_array[0];
			multi = 1;
		} else if ( chapters_1 == chapters_2  && vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio !=
			vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio){
			/* In this case we have (guess only) the same context - they have the same number of chapters but different aspect ratio and are in the same size range*/
			if ( vmg_ifo->vts_atrt->vts[title_set_size_array[0] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
				candidate = title_set_size_array[0];
			} else if ( vmg_ifo->vts_atrt->vts[title_set_size_array[1] - 1].vtstt_vobs_video_attr.display_aspect_ratio == aspect) {
				candidate = title_set_size_array[1];
			} else {
				/* Okay we didn't have the prefered aspect ratio - just make the biggest one a candidate */
				/* please send  report if this happens*/
				fprintf(stderr, "You have encountered a very special DVD, please send a bug report along with all IFO files from this title\n");
				candidate = title_set_size_array[0];
			}
			dual = 1;
		}
	} else {
		candidate = title_set_size_array[0];
	}


	/* Lets start checking audio,sub pictures and channels my guess is namly that a special suburb will put titles with a lot of
	 chapters just to make our backup hard */


	found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
				      subpicture_sub_array, title_set_sub_array,
				      channels_channel_array, title_set_channel_array,
				      0 , candidate, title_sets);


	/* Now lets see if we can find our candidate among the top most chapters */
	found_chapter=6;
	temp = chapter_chapter_array[0];
	for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
		if ( candidate == title_set_chapter_array[i] ) {
			found_chapter=i+1;
			break;
		}
	}

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);


	if (((found == 3) && (found_chapter == 1) && (dual == 0) && (multi == 0)) || ((found == 3) && (found_chapter < 3 ) && (dual == 1))) {

		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);

	}

	if (multi == 1) {
		for (i=0 ; i < title_sets ; ++i) {
			if (CheckSizeArray(size_size_array, 0, i + 1)  == 0) {
					break;
			}
		}
		number_of_multi = i;
		for (i = 0; i < number_of_multi; i++ ) {
			if (title_set_chapter_array[0] == i + 1) {
				candidate = title_set_chapter_array[0];
			}
		}

		found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
			      subpicture_sub_array, title_set_sub_array,
			      channels_channel_array, title_set_channel_array,
			      0 , candidate, title_sets);

		if (found == 3) {
			FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
					angle_angle_array, title_set_angle_array,
					subpicture_sub_array, title_set_sub_array,
					audio_audio_array, title_set_audio_array,
					size_size_array, title_set_size_array,
					channels_channel_array, title_set_channel_array);
			titles_info->main_title_set = candidate;
			return(titles_info);
		}
	}

	/* We have now come to that state that we more or less have given up :( giving you a good guess of the main feature film*/
	/*No matter what we will more or less only return the biggest VOB*/
	/* Lets see if we can find our biggest one - then we return that one */
	candidate = title_set_size_array[0];

	found = CheckAudioSubChannels(audio_audio_array, title_set_audio_array,
				      subpicture_sub_array, title_set_sub_array,
				      channels_channel_array, title_set_channel_array,
				      0 , candidate, title_sets);

	/* Now lets see if we can find our candidate among the top most chapters */

	found_chapter=5;
	temp = chapter_chapter_array[0];
	for (i=0 ; (i < titles) && (i < 4) ; i++ ) {
		if ( candidate == title_set_chapter_array[i] ) {
			found_chapter=i+1;
			break;
		}

	}

	/* Here we take chapters in to consideration*/
	if (found == 3) {
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);
	}

	/* Here we do but we lower the treshold for audio, sub and channels */

	if ((found > 1 ) && (found_chapter <= 4)) {
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);

		/* return it */
	} else {
		/* Here we give up and just return the biggest one :(*/
		/* Just return the biggest badest one*/
		FreeSortArrays( chapter_chapter_array, title_set_chapter_array,
				angle_angle_array, title_set_angle_array,
				subpicture_sub_array, title_set_sub_array,
				audio_audio_array, title_set_audio_array,
				size_size_array, title_set_size_array,
				channels_channel_array, title_set_channel_array);
		titles_info->main_title_set = candidate;
		return(titles_info);
	}


	/* Some radom thoughts about DVD guessing */
	/* We will now gather as much data about the DVD-Video as we can and
	then make a educated guess which one is the main feature film of it*/


	/* Make a tripple array with chapters, angles and title sets
	 - sort out dual title sets with a low number of chapters. Tradtionaly
	 the title set with most chapters is the main film. Number of angles is
	 keept as a reference point of low value*/

	/* Make a dual array with number of audio streams, sub picture streams
	 and title sets. Tradtionaly the main film has many audio streams
	 since it's supposed be synconised e.g. a English film syncronised/dubbed
	 in German. We are also keeping track of sub titles since it's also indication
	 of the main film*/

	/* Which title set is the biggest one - dual array with title sets and size
	 The biggest one is usally the main film*/

	/* Which title set is belonging to title 1 and how many chapters has it. Once
	 again tradtionaly title one is belonging to the main film*/

	/* Yes a lot of rant - but it helps me think - some sketch on paper or in the mind
	 I sketch in the comments - beside it will help you understand the code*/

	/* Okay lets see if the biggest one has most chapters, it also has more subtitles
	 and audio tracks than the second one and it's title one.
	 Done it must be the main film

	 Hmm the biggest one doesn't have the most chapters?

	 See if the second one has the same amount of chapters and is the biggest one
	 If so we probably have a 4:3 and 16:9 versions of film on the same disk

	 Now we fetch the 16:9 by default unless the forced to do 4:3
	 First check which one is which.
	 If the 16:9 is the biggest one and has the same or more subtile, audio streams
	 then we are happy unless we are in force 4:3 mode :(
	 The same goes in reverse if we are in force 4:3 mode


	 Hmm, in force 4:3 mode - now we check how much smaller than the biggest one it is
	 (or the reverse if we are in 16:9 mode)

	 Generally a reverse division should render in 1 and with a small modulo - like wise
	 a normal modulo should give us a high modulo

	 If we get more than one it's of cource a fake however if we get just one we still need to check
	 if we subtract the smaller one from the bigger one we should end up with a small number - hence we
	 need to multiply it more than 4 times to get it bigger than the biggest one. Now we know that the
	 two biggest once are really big and possibly carry the same film in differnet formats.

	 We will now return the prefered one either 16:9 or 4:3 but we will first check that the one
	 we return at lest has two or more audio tracks. We don't want it if the other one has a lot
	 more sound (we may end up with a film that only has 2ch Dolby Digital so we want to check for
	 6ch DTS or Dolby Digital. If the prefered one doesn't have those features but the other once has
	 we will return the other one.
	 */

}




int DVDCopyTileVobX(dvd_reader_t * dvd, title_set_info_t * title_set_info, int title_set, int vob, char * targetdir,char * title_name) {

	/* Loop variable */
	int i;

	/* Temp filename,dirname */
	char targetname[PATH_MAX];
	struct stat fileinfo;

	/* Write buffer */

	unsigned char * buffer=NULL;
	unsigned char buffy; /* :-) */

	/* File Handler */
	int streamout;

	int size;
	int left;

	/* Buffer size in DVD sectors */
	/* Currently set to 1MB */
	int buff = 512;
	int offset = 0;
	int tsize;

	/* DVD handler */
	dvd_file_t   *	dvd_file=NULL;

	if (title_set_info->number_of_title_sets + 1 < title_set) {
		fprintf(stderr,"Faild num title test\n");
		return(1);
	}

	if (title_set_info->title_set[title_set].number_of_vob_files < vob ) {
		fprintf(stderr,"Faild vob test\n");
		return(1);
	}

	if (title_set_info->title_set[title_set].size_vob[0] == 0 ) {
		fprintf(stderr,"Faild vob 1 size test\n");
		return(0);
	} else if (title_set_info->title_set[title_set].size_vob[vob - 1] == 0 ) {
		fprintf(stderr,"Faild vob %d test\n", vob);
		return(0);
	} else {
		size = title_set_info->title_set[title_set].size_vob[vob - 1]/2048;
		if (title_set_info->title_set[title_set].size_vob[vob - 1]%2048 != 0) {
			fprintf(stderr, "The Title VOB number %d of title set %d doesn't have a valid DVD size\n", vob, title_set);
			return(1);
		}
	}
#ifdef DEBUG
	fprintf(stderr,"After we check the vob it self %d\n", vob);
#endif

	/* Create VTS_XX_X.VOB */
	if (title_set == 0) {
		fprintf(stderr,"Don't try to copy a Title VOB from the VMG domain there aren't any\n");
		return(1);
	} else {
		sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_%i.VOB",targetdir, title_name, title_set, vob);
	}



	/* Now figure out the offset we will start at also check that the previus files are of valid DVD size */
	for ( i = 0; i < vob - 1; i++ ) {
		tsize = title_set_info->title_set[title_set].size_vob[i];
		if (tsize%2048 != 0) {
			fprintf(stderr, "The Title VOB number %d of title set %d doesn't have a valid DVD size\n", i + 1, title_set);
			return(1);
		} else {
			offset = offset + tsize/2048;
		}
	}
#ifdef DEBUG
	fprintf(stderr,"The offset for vob %d is %d\n", vob, offset);
#endif


	if (stat(targetname, &fileinfo) == 0) {
		fprintf(stderr, "The Title file %s exists will try to over write it.\n", targetname);
		if (! S_ISREG(fileinfo.st_mode)) {
			fprintf(stderr,"The Title %s file is not valid, it may be a directory\n", targetname);
			return(1);
		} else {
			if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
				fprintf(stderr, "Error opening %s\n", targetname);
				perror("");
				return(1);
			}
		}
	} else {
		if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
			fprintf(stderr, "Error creating %s\n", targetname);
			perror("");
			return(1);
		}
	}

	left = size;

	if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(buffy))) == NULL) {
		fprintf(stderr, "Out of memory coping %s\n", targetname);
		close(streamout);
		return(1);
	}


	if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_TITLE_VOBS))== 0) {
		fprintf(stderr, "Faild opending TITLE VOB\n");
		free(buffer);
		close(streamout);
		return(1);
	}

	while( left > 0 ) {

		if (buff > left) {
			buff = left;
		}
		if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
			fprintf(stderr, "Error reading MENU VOB\n");
			free(buffer);
			DVDCloseFile(dvd_file);
			close(streamout);
			return(1);
		}


		if (write(streamout,buffer,buff *  2048) != buff * 2048) {
			fprintf(stderr, "Error writing TITLE VOB\n");
			free(buffer);
			close(streamout);
			return(1);
		}

		offset = offset + buff;
		left = left - buff;

	}

	DVDCloseFile(dvd_file);
	free(buffer);
	close(streamout);
	return(0);

}




int DVDCopyMenu(dvd_reader_t * dvd, title_set_info_t * title_set_info, int title_set, char * targetdir,char * title_name) {

	/* Temp filename,dirname */
	char targetname[PATH_MAX];
	struct stat fileinfo;

	/* Write buffer */

	unsigned char * buffer=NULL;
	unsigned char buffy; /* :-) */

	/* File Handler */
	int streamout;

	int size;
	int left;

	/* Buffer size in DVD sectors */
	/* Currently set to 1MB */
	int buff = 512;
	int offset = 0;


	/* DVD handler */
	dvd_file_t   *	dvd_file=NULL;

	if (title_set_info->number_of_title_sets + 1 < title_set) {
		return(1);
	}

	if (title_set_info->title_set[title_set].size_menu == 0 ) {
		return(0);
	} else {
		size = title_set_info->title_set[title_set].size_menu/2048;
		if (title_set_info->title_set[title_set].size_menu%2048 != 0) {
			fprintf(stderr, "The Menu VOB of title set %d doesn't have a valid DVD size\n", title_set);
			return(1);
		}
	}

	/* Create VIDEO_TS.VOB or VTS_XX_0.VOB */
	if (title_set == 0) {
		sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.VOB",targetdir, title_name);
	} else {
		sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.VOB",targetdir, title_name, title_set);
	}


	if (stat(targetname, &fileinfo) == 0) {
		fprintf(stderr, "The Menu file %s exists will try to over write it.\n", targetname);
		if (! S_ISREG(fileinfo.st_mode)) {
			fprintf(stderr,"The Menu %s file is not valid, it may be a directory\n", targetname);
			return(1);
		} else {
			if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
				fprintf(stderr, "Error opening %s\n", targetname);
				perror("");
				return(1);
			}
		}
	} else {
		if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
			fprintf(stderr, "Error creating %s\n", targetname);
			perror("");
			return(1);
		}
	}

	left = size;

	if ((buffer = (unsigned char *)malloc(buff * 2048 * sizeof(buffy))) == NULL) {
		fprintf(stderr, "Out of memory coping %s\n", targetname);
		close(streamout);
		return(1);
	}


	if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_MENU_VOBS))== 0) {
		fprintf(stderr, "Faild opending MENU VOB\n");
		free(buffer);
		close(streamout);
		return(1);
	}

	while( left > 0 ) {

		if (buff > left) {
			buff = left;
		}
		if ( DVDReadBlocks(dvd_file,offset,buff, buffer) != buff) {
			fprintf(stderr, "Error reading MENU VOB\n");
			free(buffer);
			DVDCloseFile(dvd_file);
			close(streamout);
			return(1);
		}


		if (write(streamout,buffer,buff *  2048) != buff * 2048) {
			fprintf(stderr, "Error writing MENU VOB\n");
			free(buffer);
			close(streamout);
			return(1);
		}

		offset = offset + buff;
		left = left - buff;

	}

	DVDCloseFile(dvd_file);
	free(buffer);
	close(streamout);
	return(0);

}


int DVDCopyIfoBup (dvd_reader_t * dvd, title_set_info_t * title_set_info, int title_set, char * targetdir,char * title_name) {

	/* Temp filename,dirname */
	char targetname[PATH_MAX];
	struct stat fileinfo;

	/* Write buffer */

	unsigned char * buffer=NULL;
	unsigned char buffy; /* :-) */

	/* File Handler */
	int streamout;

	int size;

	/* DVD handler */
	dvd_file_t   *	dvd_file=NULL;


	if (title_set_info->number_of_title_sets + 1 < title_set) {
		return(1);
	}

	if (title_set_info->title_set[title_set].size_ifo == 0 ) {
		return(0);
	} else {
		size = title_set_info->title_set[title_set].size_ifo;
		if (title_set_info->title_set[title_set].size_ifo%2048 != 0) {
			fprintf(stderr, "The IFO of title set %d doesn't have a valid DVD size\n", title_set);
			return(1);
		}
	}

	/* Create VIDEO_TS.IFO or VTS_XX_0.IFO */

	if (title_set == 0) {
		sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.IFO",targetdir, title_name);
	} else {
		sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.IFO",targetdir, title_name, title_set);
	}

	if (stat(targetname, &fileinfo) == 0) {
		fprintf(stderr, "The IFO file %s exists will try to over write it.\n", targetname);
		if (! S_ISREG(fileinfo.st_mode)) {
			fprintf(stderr,"The IFO %s file is not valid, it may be a directory\n", targetname);
			return(1);
		} else {
			if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
				fprintf(stderr, "Error opening %s\n", targetname);
				perror("");
				return(1);
			}
		}
	} else {
		if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
			fprintf(stderr, "Error creating %s\n", targetname);
			perror("");
			return(1);
		}
	}

	/* Copy VIDEO_TS.IFO, since it's a small file try to copy it in one shot */


	if ((buffer = (unsigned char *)malloc(size * sizeof(buffy))) == NULL) {
		fprintf(stderr, "Out of memory coping %s\n", targetname);
		close(streamout);
		return(1);
	}


	if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_INFO_FILE))== 0) {
		fprintf(stderr, "Faild opending IFO for tile set %d\n", title_set);
		free(buffer);
		close(streamout);
		return(1);
	}

	if ( DVDReadBytes(dvd_file,buffer,size) != size) {
		fprintf(stderr, "Error reading IFO for title set %d\n", title_set);
		free(buffer);
		DVDCloseFile(dvd_file);
		close(streamout);
		return(1);
	}

	DVDCloseFile(dvd_file);

	if (write(streamout,buffer,size) != size) {
		fprintf(stderr, "Error writing %s\n",targetname);
		free(buffer);
		close(streamout);
		return(1);
	}

	free(buffer);
	close(streamout);


	/* Create VIDEO_TS.BUP or VTS_XX_0.BUP */

	if (title_set == 0) {
		sprintf(targetname,"%s/%s/VIDEO_TS/VIDEO_TS.BUP",targetdir, title_name);
	} else {
		sprintf(targetname,"%s/%s/VIDEO_TS/VTS_%02i_0.BUP",targetdir, title_name, title_set);
	}


	if (title_set_info->title_set[title_set].size_bup == 0 ) {
		return(0);
	} else {
		size = title_set_info->title_set[title_set].size_bup;
		if (title_set_info->title_set[title_set].size_bup%2048 != 0) {
			fprintf(stderr, "The BUP of title set %d doesn't have a valid DVD size\n", title_set);
			return(1);
		}
	}


	if (stat(targetname, &fileinfo) == 0) {
		fprintf(stderr, "The BUP file %s exists will try to over write it.\n", targetname);
		if (! S_ISREG(fileinfo.st_mode)) {
			fprintf(stderr,"The BUP %s file is not valid, it may be a directory\n", targetname);
			return(1);
		} else {
			if ((streamout = open(targetname, O_WRONLY | O_TRUNC, 0644)) == -1) {
				fprintf(stderr, "Error opening %s\n", targetname);
				perror("");
				return(1);
			}
		}
	} else {
		if ((streamout = open(targetname, O_WRONLY | O_CREAT, 0644)) == -1) {
			fprintf(stderr, "Error creating %s\n", targetname);
			perror("");
			return(1);
		}
	}



	/* Copy VIDEO_TS.BUP or VTS_XX_0.BUP, since it's a small file try to copy it in one shot */

	if ((buffer = (unsigned char *)malloc(size * sizeof(buffy))) == NULL) {
		fprintf(stderr, "Out of memory coping %s\n", targetname);
		close(streamout);
		return(1);
	}


	if ((dvd_file = DVDOpenFile(dvd, title_set, DVD_READ_INFO_BACKUP_FILE))== 0) {
		fprintf(stderr, "Faild opending BUP for title set %d\n", title_set);
		free(buffer);
		close(streamout);
		return(1);
	}

	if ( DVDReadBytes(dvd_file,buffer,size) != size) {
		fprintf(stderr, "Error reading BUP for title set %d\n", title_set);
		free(buffer);
		DVDCloseFile(dvd_file);
		close(streamout);
		return(1);
	}

	DVDCloseFile(dvd_file);

	if (write(streamout,buffer,size) != size) {
		fprintf(stderr, "Error writing %s\n", targetname);
		free(buffer);
		close(streamout);
		return(1);
	}

	free(buffer);
	close(streamout);

	return(0);
}


int DVDMirrorVMG(dvd_reader_t * dvd, title_set_info_t *  title_set_info,char * targetdir,char * title_name){

	if ( DVDCopyIfoBup(dvd, title_set_info, 0, targetdir, title_name) != 0 ) {
		return(1);
	}

	if ( DVDCopyMenu(dvd, title_set_info, 0, targetdir, title_name) != 0 ) {
		return(1);
	}
	return(0);
}

int DVDMirrorTitleX(dvd_reader_t * dvd, title_set_info_t *  title_set_info, int title_set, char * targetdir,char * title_name) {

	/* Loop through the vobs */
	int i;



	if ( DVDCopyIfoBup(dvd, title_set_info, title_set, targetdir, title_name) != 0 ) {
		return(1);
	}

	if ( DVDCopyMenu(dvd, title_set_info, title_set, targetdir, title_name) != 0 ) {
		return(1);
	}

	for (i = 0; i < title_set_info->title_set[title_set].number_of_vob_files ; i++) {
#ifdef DEBUG
		fprintf(stderr,"In the VOB copy loop for %d\n", i);
#endif		
		if ( DVDCopyTileVobX(dvd, title_set_info, title_set, i + 1, targetdir, title_name) != 0 ) {
		return(1);
		}
	}


	return(0);
}

int DVDGetTitleName(const char *device, char *title)
{
	/* Variables for filehandel and title string interaction */

	int  filehandle, i, last;

	/* Open DVD device */

	if ( !(filehandle = open(device, O_RDONLY)) ) {
		fprintf(stderr, "Can't open secified device %s - check your DVD device\n", device);
		return(1);
	}

	/* Seek to title of first track, which is at (track_no * 32768) + 40 */

	if ( 32808 != lseek(filehandle, 32808, SEEK_SET) ) {
		close(filehandle);
		fprintf(stderr, "Can't seek DVD device %s - check your DVD device\n", device);
		return(1);
	}

	/* Read the DVD-Video title */

	if ( 32 != read(filehandle, title, 32)) {
		close(filehandle);
		fprintf(stderr, "Can't read title from DVD device %s\n", device);
		return(1);
	}

	/* Terminate the title string */

	title[32] = '\0';


	/* Remove trailing white space */

	last = 32;
	for ( i = 0; i < 32; i++ ) {
		if ( title[i] != ' ' ) { last = i; }
	}

	title[last + 1] = '\0';

	close(filehandle);
	return(0);
}



void bsort_min_to_max(int sector[], int title[], int size){

	int temp_title, temp_sector, i, j;

	for ( i=0; i < size ; i++ ) {
	  for ( j=0; j < size ; j++ ) {
		if (sector[i] < sector[j]) {
			temp_sector = sector[i];
			temp_title = title[i];
			sector[i] = sector[j];
			title[i] = title[j];
			sector[j] = temp_sector;
			title[j] = temp_title;
		}
	  }
	}
}

void bsort_max_to_min(int sector[], int title[], int size){

	int temp_title, temp_sector, i, j;

	for ( i=0; i < size ; i++ ) {
	  for ( j=0; j < size ; j++ ) {
		if (sector[i] > sector[j]) {
			temp_sector = sector[i];
			temp_title = title[i];
			sector[i] = sector[j];
			title[i] = title[j];
			sector[j] = temp_sector;
			title[j] = temp_title;
		}
	  }
	}
}



void uniq(int sector[], int title[], int title_sets_array[], int sector_sets_array[], int titles){
	int  i, j;


	for ( i=0, j=0; j < titles;) {
		if (sector[j] != sector[j+1]) {
			title_sets_array[i]  = title[j];
			sector_sets_array[i] = sector[j];
			i++ ;
			j++ ;
		} else {
			do {
			     if (j < titles) {
				j++ ;
			   }
			} while ( sector[j] == sector[j+1] );

		}
	}

}

void align_end_sector(int cell_start_sector[],int cell_end_sector[], int size) {

	int i;

	for (i = 0; i < size  - 1 ; i++) {
		if ( cell_end_sector[i] >= cell_start_sector[i + 1] ) {
			cell_end_sector[i] = cell_start_sector[i + 1] - 1;
		}
	}
}




void DVDFreeTitleSetInfo(title_set_info_t * title_set_info) {
	free(title_set_info->title_set);
	free(title_set_info);
}

void DVDFreeTitlesInfo(titles_info_t * titles_info) {
	free(titles_info->titles);
	free(titles_info);
}



title_set_info_t *DVDGetFileSet(dvd_reader_t * _dvd) {

	/* title interation */
	int title_sets, counter, i;


	/* DVD Video files */
	char	filename[MAXNAME];
	int	size;

	/*DVD ifo handler*/
	ifo_handle_t * 	vmg_ifo=NULL;

	/* The Title Set Info struct*/
	title_set_info_t * title_set_info;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}


	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);

	/* Todo fix malloc check */
	title_set_info = (title_set_info_t *)malloc(sizeof(title_set_info_t));
	title_set_info->title_set = (title_set_t *)malloc((title_sets + 1)* sizeof(title_set_t));

	title_set_info->number_of_title_sets = title_sets;


	/* Find VIDEO_TS.IFO is present - must be present since we did a ifo open 0*/

	sprintf(filename,"/VIDEO_TS/VIDEO_TS.IFO");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_ifo = size;
	} else {
		DVDFreeTitleSetInfo(title_set_info);
		return(0);
	}



	/* Find VIDEO_TS.VOB if present*/

	sprintf(filename,"/VIDEO_TS/VIDEO_TS.VOB");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_menu = size;
	} else {
		title_set_info->title_set[0].size_menu = 0 ;
	}

	/* Find VIDEO_TS.BUP if present */

	sprintf(filename,"/VIDEO_TS/VIDEO_TS.BUP");

	if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
		title_set_info->title_set[0].size_bup = size;
	} else {
		DVDFreeTitleSetInfo(title_set_info);
		return(0);
	}

	if (title_set_info->title_set[0].size_ifo != title_set_info->title_set[0].size_bup) {
		fprintf(stderr,"BUP and IFO size not the same be warened!\n");
	}


	/* Take care of the titles which we don't have in VMG */

	title_set_info->title_set[0].number_of_vob_files = 0;
	title_set_info->title_set[0].size_vob[0] = 0;


	if ( verbose > 0 ){
		fprintf(stderr,"\n\n\nFile sizes for Title set 0 VIDEO_TS.XXX\n");
		fprintf(stderr,"IFO = %d, MENU_VOB = %d, BUP = %d\n",title_set_info->title_set[0].size_ifo, title_set_info->title_set[0].size_menu, title_set_info->title_set[0].size_bup );

	}


	if ( title_sets >= 1 ) {
 		for (counter=0; counter < title_sets; counter++ ){

			if ( verbose > 1 ){
				fprintf(stderr,"At top of loop\n");
			}


			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.IFO",counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter + 1].size_ifo = size;
			} else {
				DVDFreeTitleSetInfo(title_set_info);
				return(0);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"After opening files\n");
			}


			/* Find VTS_XX_0.VOB if present*/

			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.VOB", counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter + 1].size_menu = size;
			} else {
				title_set_info->title_set[counter + 1].size_menu = 0 ;
			}


			if ( verbose > 1 ){
				fprintf(stderr,"After Menu VOB check\n");
			}


			/* Find all VTS_XX_[1 to 9].VOB files if they are present*/

			for( i = 0; i < 9; ++i ) {
				sprintf(filename,"/VIDEO_TS/VTS_%02i_%i.VOB", counter + 1, i + 1 );
				if(UDFFindFile(_dvd, filename, &size) == 0 ) {
					break;
				}
				title_set_info->title_set[counter + 1].size_vob[i] = size;
			}
			title_set_info->title_set[counter + 1].number_of_vob_files = i;

			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title VOB check\n");
			}


			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.BUP", counter + 1);

			if ( UDFFindFile(_dvd, filename, &size) != 0 ) {
				title_set_info->title_set[counter +1].size_bup = size;
			} else {
				DVDFreeTitleSetInfo(title_set_info);
				return(0);
			}

			if (title_set_info->title_set[counter +1].size_ifo != title_set_info->title_set[counter + 1].size_bup) {
				fprintf(stderr,"BUP and IFO size for fileset %d is not the same be warened!\n", counter + 1);
			}



			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title BUP check\n");
			}


			if ( verbose > 0 ) {
				fprintf(stderr,"\n\n\nFile sizes for Title set %d i.e.VTS_%02d_X.XXX\n", counter + 1, counter + 1);
				fprintf(stderr,"IFO: %d, MENU: %d\n", title_set_info->title_set[counter +1].size_ifo, title_set_info->title_set[counter +1].size_menu);
				for (i = 0; i < title_set_info->title_set[counter + 1].number_of_vob_files ; i++) {
					fprintf(stderr, "VOB %d is %d\n", i + 1, title_set_info->title_set[counter + 1].size_vob[i]);
				}
				fprintf(stderr,"BUP: %d\n",title_set_info->title_set[counter +1].size_bup);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"Bottom of loop \n");
			}
		}

        }

	/* Return the info */
	return(title_set_info);


}

int DVDMirror(dvd_reader_t * _dvd, char * targetdir,char * title_name) {

	int i;
	title_set_info_t * title_set_info=NULL;

	title_set_info = DVDGetFileSet(_dvd);
	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}

	if ( DVDMirrorVMG(_dvd, title_set_info, targetdir, title_name) != 0 ) {
		fprintf(stderr,"Mirror of VMG faild\n");
		DVDFreeTitleSetInfo(title_set_info);
		return(1);
	}

	for ( i=0; i < title_set_info->number_of_title_sets; i++) {
		if ( DVDMirrorTitleX(_dvd, title_set_info, i + 1, targetdir, title_name) != 0 ) {
			fprintf(stderr,"Mirror of Title set %d faild\n", i + 1);
			DVDFreeTitleSetInfo(title_set_info);
			return(1);
		}
	}
	return(0);
}



void DVDFreeTitleSetInfo_image(DVDRip_title_set_info_t * title_set_info) {
	free(title_set_info->title_set);
	free(title_set_info);
}




DVDRip_title_set_info_t *DVDGetFileSet_image(dvd_reader_t * _dvd) {

	/* title interation */
	int title_sets, counter, i;

	/* startlb */
	unsigned int file_startlb = 0;

	/* DVD Video files */
	char	filename[MAXNAME];
	int	size;

	/*DVD ifo handler*/
	ifo_handle_t * 	vmg_ifo=NULL;

	/* The Title Set Info struct*/
	DVDRip_title_set_info_t * title_set_info;

	/*  Open main info file */
	vmg_ifo = ifoOpen( _dvd, 0 );
	if( !vmg_ifo ) {
        	fprintf( stderr, "Can't open VMG info.\n" );
        	return (0);
    	}


	title_sets = vmg_ifo->vmgi_mat->vmg_nr_of_title_sets;

	/* Close the VMG ifo file we got all the info we need */
        ifoClose(vmg_ifo);

	/* Todo fix malloc check */
	title_set_info = (DVDRip_title_set_info_t *)malloc(sizeof(DVDRip_title_set_info_t));
	title_set_info->title_set = (DVDRip_title_set_t *)malloc((title_sets + 1)* sizeof(DVDRip_title_set_t));

	title_set_info->number_of_title_sets = title_sets;


	/* Find VIDEO_TS.IFO is present - must be present since we did a ifo open 0*/

	sprintf(filename,"/VIDEO_TS/VIDEO_TS.IFO");

	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_ifo = size;
		title_set_info->title_set[0].ifo_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.IFO startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		DVDFreeTitleSetInfo_image(title_set_info);
		return(0);
	}



	/* Find VIDEO_TS.VOB if present*/
	file_startlb = 0;
	sprintf(filename,"/VIDEO_TS/VIDEO_TS.VOB");

	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_menu = size;
		title_set_info->title_set[0].menu_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.VOB startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		title_set_info->title_set[0].size_menu = 0 ;	
		title_set_info->title_set[0].menu_startlb = file_startlb;
	}

	/* Find VIDEO_TS.BUP if present */

	sprintf(filename,"/VIDEO_TS/VIDEO_TS.BUP");
	file_startlb = 0;
	if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
		title_set_info->title_set[0].size_bup = size;
		title_set_info->title_set[0].bup_startlb = file_startlb;
		fprintf(stderr,"find VIDEO_TS/VIDEO_TS.BUP startlb (%d) size (%d)\n", file_startlb, size); 
	} else {
		DVDFreeTitleSetInfo_image(title_set_info);
		return(0);
	}

	if (title_set_info->title_set[0].size_ifo != title_set_info->title_set[0].size_bup) {
		fprintf(stderr,"BUP and IFO size not the same be warened!\n");
	}


	/* Take care of the titles which we don't have in VMG */

	title_set_info->title_set[0].number_of_vob_files = 0;
	title_set_info->title_set[0].size_vob[0] = 0;


	if ( verbose > 0 ){
		fprintf(stderr,"\n\n\nFile sizes for Title set 0 VIDEO_TS.XXX\n");
		fprintf(stderr,"IFO startlb = %d size= %d , MENU_VOB = startlb = %d size= %d, BUP = startlb = %d size= %d\n",
		title_set_info->title_set[0].ifo_startlb,
		title_set_info->title_set[0].size_ifo, 
		title_set_info->title_set[0].menu_startlb, 
		title_set_info->title_set[0].size_menu, 
		title_set_info->title_set[0].bup_startlb,
		title_set_info->title_set[0].size_bup);
	}

	fprintf(stderr,"TILTE SET = (%d)\n", title_sets);
	if ( title_sets >= 1 ) {
 		for (counter=0; counter < title_sets; counter++ ){

			if ( verbose > 1 ){
				fprintf(stderr,"At top of loop\n");
			}


			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.IFO",counter + 1);
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter + 1].size_ifo = size;
				title_set_info->title_set[counter + 1].ifo_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.IFO startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				DVDFreeTitleSetInfo_image(title_set_info);
				return(0);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"After opening files\n");
			}


			/* Find VTS_XX_0.VOB if present*/

			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.VOB", counter + 1);
			
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter + 1].size_menu = size;
				title_set_info->title_set[counter + 1].menu_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.VOB startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				title_set_info->title_set[counter + 1].size_menu = 0 ;
				title_set_info->title_set[counter + 1].menu_startlb = file_startlb;
			}


			if ( verbose > 1 ){
				fprintf(stderr,"After Menu VOB check\n");
			}


			/* Find all VTS_XX_[1 to 9].VOB files if they are present*/

			for( i = 0; i < 9; ++i ) {
				sprintf(filename,"/VIDEO_TS/VTS_%02i_%i.VOB", counter + 1, i + 1 );
				file_startlb  = 0;
				if((file_startlb = UDFFindFile(_dvd, filename, &size)) == 0 ) {
					break;
				}
				title_set_info->title_set[counter + 1].size_vob[i] = size;
				title_set_info->title_set[counter + 1].vob_startlb[i] = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_%i.IFO startlb (%d) size (%d)\n", counter + 1, i+1, file_startlb, size); 
			}
			title_set_info->title_set[counter + 1].number_of_vob_files = i;

			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title VOB check\n");
			}


			sprintf(filename,"/VIDEO_TS/VTS_%02i_0.BUP", counter + 1);
			
			file_startlb = 0;
			if ( (file_startlb = UDFFindFile(_dvd, filename, &size)) != 0 ) {
				title_set_info->title_set[counter +1].size_bup = size;
				title_set_info->title_set[counter +1].bup_startlb = file_startlb;
				fprintf(stderr,"find VIDEO_TS/VTS_%02i_0.BUP startlb (%d) size (%d)\n", counter + 1, file_startlb, size); 
			} else {
				DVDFreeTitleSetInfo_image(title_set_info);
				return(0);
			}

			if (title_set_info->title_set[counter +1].size_ifo != title_set_info->title_set[counter + 1].size_bup) {
				fprintf(stderr,"BUP and IFO size for fileset %d is not the same be warened!\n", counter + 1);
			}



			if ( verbose > 1 ){
				fprintf(stderr,"After Menu Title BUP check\n");
			}


			if ( verbose > 0 ) {
				fprintf(stderr,"\n\n\nFile sizes for Title set %d i.e.VTS_%02d_X.XXX\n", counter + 1, counter + 1);
				fprintf(stderr,"IFO: startlb =%d size=%d, MENU: startlb =%d size=%d\n", 
					title_set_info->title_set[counter +1].ifo_startlb, 
					title_set_info->title_set[counter +1].size_ifo,
					title_set_info->title_set[counter +1].menu_startlb, 
					title_set_info->title_set[counter +1].size_menu);

				for (i = 0; i < title_set_info->title_set[counter + 1].number_of_vob_files ; i++) {
					fprintf(stderr, "VOB %d is startlb=%d size=%d\n", i + 1, 
						title_set_info->title_set[counter + 1].vob_startlb[i],
						title_set_info->title_set[counter + 1].size_vob[i]);
				}
				fprintf(stderr,"BUP: startlb=%d size=%d\n",
					title_set_info->title_set[counter +1].bup_startlb,
					title_set_info->title_set[counter +1].size_bup);
			}

			if ( verbose > 1 ){
				fprintf(stderr,"Bottom of loop \n");
			}
		}

        }

	/* Return the info */
	return(title_set_info);


}

#define PLANE_AREA	0
#define TITLE_VOB	1
#define MENU_VOB	2
#define IFO		3
#define BUP		4

typedef struct _AddressEntry
{
	unsigned int startlb;
	unsigned int lbcount;
	unsigned int needKeySet;
	unsigned int type;
	unsigned short title;
	unsigned short index;
	struct _AddressEntry * next;
	struct _AddressEntry * prev;
} AddressEntry;

typedef struct _AddressMap
{
	unsigned int EntryCount;
	AddressEntry Head;
} AddressMap;

AddressEntry *
GenerateAddressEntry(
	unsigned int startlb, 
	unsigned int lbcount, 
	unsigned int needKey,
	unsigned int type,
	unsigned short title,
	unsigned short index
	)
{
	AddressEntry *entry;
 
	if((entry= (AddressEntry *)malloc( sizeof(AddressEntry) ) ) == NULL)
	{
		fprintf(stderr,"Out of Memory when making AddressEntry startlb=%d\n",startlb);
		return NULL;
	}
	
	entry->startlb = startlb;
	entry->lbcount = lbcount;
	entry->needKeySet = needKey;
	entry->type = type;
	entry->next = entry;
	entry->prev = entry;
	entry->title = title;
	entry->index = index;
	return entry;
}


int
InsertAddressEntry(AddressMap * Map, AddressEntry *entry)
{
	AddressEntry * p;
	AddressEntry * head = &(Map->Head);
	
	p = head->next;
	if(p == head){
		p->next = entry;
		p->prev = entry;
		entry->prev = p;
		entry->next = p;
	}else{
		while(p != head)
		{
			if(p->startlb >  entry->startlb)
			{
				p->prev->next = entry;
				entry->prev = p->prev;
				p->prev = entry;
				entry->next = p;
				break;
			}else if (p->startlb == entry->startlb){
				fprintf(stderr," Same Entry already exist! p->startlb=%d : entry->startlb=%d\n",p->startlb,entry->startlb);
				free(entry);
				return 0;
			} else{
				p = p->next;				
			}
		}
		
		if(p == head)
		{
			p->prev->next = entry;
			entry->prev = p->prev;
			p->prev = entry;
			entry->next = p;
		}			
	}
	
	return 1;
}

void PrintAddressMap(AddressMap * Map)
{
	AddressEntry * p, *q, *head;
	int count = 0;
	
	char * TypeStr[] = {"PLAN_AREA","TITLE_VOB","MENU_VOB","IFO","BUP"};
	
	head = &(Map->Head);
	p = head->next;

	fprintf(stderr,"Total entry %d\n",Map->EntryCount);

	while(p != head)
	{
		q = p;
		p = p->next;
		count ++;
		fprintf(stderr,"ADDRESS ENTRY (%d)\n",count);
		fprintf(stderr,"\t Slb=%d, blkC=%d, UseKey=%d,  Type= %s, Tittle= %d, Index=%d\n",
			q->startlb, q->lbcount,q->needKeySet, TypeStr[q->type],q->title, q->index); 
	}
}


void FreeAddressMap(AddressMap * Map)
{
	AddressEntry * p, *q, *head;
	head = &(Map->Head);
	p = head->next;
	while(p != head)
	{
		q = p;
		p = p->next;
		free(q);
	}

	free(Map);
}

int
AddUncoveredSpace(AddressMap * Map, unsigned int TotalDiscSize)
{
	AddressEntry * p, *q, *head, *entry;
	unsigned int startlb = 0;
	unsigned int blockcount = 0;	
	head = &(Map->Head);
	p = head->next;
	entry = NULL;

	while(p != head)
	{	
		q = p;
		p = p->next;
		if(q->startlb != startlb)
		{
			if(q->startlb >  startlb)
			{
				blockcount = q->startlb - startlb;
				if( (entry = GenerateAddressEntry(startlb,blockcount,0,PLANE_AREA,0,0)) == NULL)
				{
					return 0;
				}
				fprintf(stderr, " insert new address q->startlb (%d), startlb (%d)\n", q->startlb, startlb);	
				if(!InsertAddressEntry(Map,entry))
				{
					return 0;
				}
				Map->EntryCount ++;
				startlb = q->startlb + q->lbcount;
			}else if(q->startlb < startlb){
				fprintf(stderr, "Error Address Spaces are duplicated!!!\n");
				return 0;
			}
		}else {
			startlb = q->startlb + q->lbcount;
		}		
		
	}
	
	if(startlb != TotalDiscSize)
	{
		blockcount = TotalDiscSize - startlb;
		if( (entry = GenerateAddressEntry(startlb,blockcount,0,PLANE_AREA,0,0)) == NULL)
		{
			return 0;
		}
		
		fprintf(stderr, " insert new address totalDiscSize (%d), startlb (%d)\n", TotalDiscSize, startlb);	
		if(!InsertAddressEntry(Map,entry))
		{
			return 0;
		}
		Map->EntryCount ++;	
	}

	return 1;
}


AddressMap *
GenerateAddressMap(DVDRip_title_set_info_t * title_set_info, unsigned int TotalDiscSize)
{
	AddressMap 		*Map;
	AddressEntry 		*entry;
	DVDRip_title_set_t 	*title;
	unsigned int		blockcount; 
	unsigned int 		startlb;
	int CountOfVobFile = 0;	
	int loop = 0;
	int i,j;


	if( (Map = (AddressMap *)malloc( sizeof(AddressMap)) ) == NULL)
	{
		fprintf(stderr,"Out of Memory when making AddressMap n");
		return NULL;	
	}

	Map->EntryCount = 0;
	(Map->Head).prev = &(Map->Head);
	(Map->Head).next = &(Map->Head);	

	loop = title_set_info->number_of_title_sets + 1;
	for(i = 0; i < loop; i++)
	{	
		title = &(title_set_info->title_set[i]);
		/* Set IFO File */
		blockcount = ( (title->size_ifo/DVD_VIDEO_LB_LEN) + 
				((title->size_ifo % DVD_VIDEO_LB_LEN)?1:0) );
		fprintf(stderr,"IFO file title->ifo_startlb (%d) blockcount (%d)\n", title->ifo_startlb, blockcount);	
		if(blockcount !=0)
		{	
			if( (entry = GenerateAddressEntry(title->ifo_startlb,blockcount,0,IFO,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}

			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}	
		Map->EntryCount ++;
		/* Set MENU File */
		blockcount = ( (title->size_menu/DVD_VIDEO_LB_LEN) + 
				((title->size_menu % DVD_VIDEO_LB_LEN)?1:0) );			
		fprintf(stderr,"MENU file title->menu_startlb (%d) blockcount (%d)\n", title->menu_startlb, blockcount);	
		if(blockcount != 0)
		{	
			if( (entry = GenerateAddressEntry(title->menu_startlb,blockcount,1,MENU_VOB,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}
		

			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}	
		Map->EntryCount ++;	
		/* Set BUP  File */
		blockcount = ( (title->size_bup/DVD_VIDEO_LB_LEN) + 
				((title->size_bup % DVD_VIDEO_LB_LEN)?1:0) );			
		fprintf(stderr,"BUP file title->bup_startlb (%d) blockcount (%d)\n", title->bup_startlb, blockcount);
		if(blockcount != 0)
		{		
			if( (entry = GenerateAddressEntry(title->bup_startlb,blockcount,0,BUP,i,0)) == NULL)
			{
				FreeAddressMap(Map);
				return NULL;
			}
		
			if(!InsertAddressEntry(Map,entry))
			{
				FreeAddressMap(Map);
				return NULL;
			}
		}

		Map->EntryCount ++;
		
		
		/* Set TITLE File */
		CountOfVobFile = title->number_of_vob_files;
		if(CountOfVobFile > 0)
		{	
			blockcount = 0;
			startlb = title->vob_startlb[0];

			for(j = 0; j < CountOfVobFile; j++)
			{
				blockcount += ( (title->size_vob[j]/DVD_VIDEO_LB_LEN) + 
						((title->size_vob[j] % DVD_VIDEO_LB_LEN)?1:0) );
			}
			
			fprintf(stderr,"TITLE file startlb (%d) blockcount (%d)\n", startlb, blockcount);
			if(blockcount != 0)
			{		
				if( (entry = GenerateAddressEntry(startlb,blockcount,1,TITLE_VOB,i,1)) == NULL)
				{
					FreeAddressMap(Map);
					return NULL;
				}
		
				if(!InsertAddressEntry(Map,entry))
				{
					FreeAddressMap(Map);
					return NULL;
				}
			}
			Map->EntryCount ++;
		}
				
	}

	if(!AddUncoveredSpace(Map,TotalDiscSize))
	{
		FreeAddressMap(Map);
		return NULL;
	}
			
	return Map;	
}


typedef struct _PartInfo 
{
	unsigned short num;
	unsigned int start;
	unsigned int length; // size/2048
} PartInfo;

#define MAX_DATA_SIZE 	(1024*64)
#define MAX_BLK_SIZE	(MAX_DATA_SIZE/2048)
int DVDcopyUnscambledBlock(dvd_reader_t * dvd, int file, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	//int copyed = 0;

	char * buff = malloc(MAX_DATA_SIZE);
	struct timeval t,g;
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
	fprintf(stderr, "COPY Unscambled Address Area\n");
	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d)\n", 
		entry->startlb,	entry->lbcount, entry->type, entry->title);
	if(!buff){
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff,0,MAX_DATA_SIZE);

		//gettimeofday(&t,NULL);
		if(!UDFReadBlocksRaw( dvd, startlb, size, buff, 0))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);
			return 0;
		}
		//gettimeofday(&g,NULL);

		//fprintf(stderr,"READ TIME: t.tv_sec(%d), t.tv_usec(%d)\n",t.tv_sec, t.tv_usec);
		//fprintf(stderr,"READ TIME: g.tv_sec(%d), g.tv_usec(%d)\n",g.tv_sec, g.tv_usec);
		

		//gettimeofday(&t,NULL);
		if(write(file,buff,size*2048) != size*2048)
		{
			free(buff);
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			return 0;
		}
		//gettimeofday(&g,NULL);
			
		//fprintf(stderr,"WRITE TIME: t.tv_sec(%d), t.tv_usec(%d)\n",t.tv_sec, t.tv_usec);
		//fprintf(stderr,"WRITE TIME: g.tv_sec(%d), g.tv_usec(%d)\n",g.tv_sec, g.tv_usec);
		
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}

int DVDcopyScambledBlock(dvd_reader_t * dvd, int file, AddressEntry * entry)
{
	int startlb = 0;
	int blkcount = 0;
	int size = 0;
	//int copyed = 0;
	int csstitle = 0;
	struct timeval t,g;

	char * buff = malloc(MAX_DATA_SIZE);
	
	startlb = (int)entry->startlb;
	blkcount = (int)entry->lbcount;
	
	fprintf(stderr, "COPY Scambled Address Area\n");
	fprintf(stderr, "SLB(%d) CBLK(%d) TYPE(%d) TITLE(%d) INDEX(%d)\n", 
		entry->startlb,	entry->lbcount, entry->type, entry->title, entry->index);

	if(!buff) {
		fprintf(stderr, "Error Alloc Memory\n");
		return 0;
	}

	if(entry->type == MENU_VOB)
	{
		csstitle =( (((int)entry->title) << 1) | 1 );
	}else{
		csstitle = ((int)entry->title) << 1;
	}
	
	/* set decrypte key */		
	if(GetCssTitle(dvd) != csstitle)
	{
		SetCssTitle(dvd, csstitle);
		DoDvdTitle(dvd, (int)entry->startlb);	
	}

	while(blkcount)
	{
		if(blkcount < MAX_BLK_SIZE)
		{
			size = blkcount;
		}else{
			size = MAX_BLK_SIZE;
		}
		memset(buff, 0, MAX_DATA_SIZE);


		//gettimeofday(&t,NULL);
		if(!UDFReadBlocksRaw( dvd, startlb, size, buff, 1))
		{
			fprintf(stderr,"Can't read data from dev SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		//gettimeofday(&g,NULL);

		//fprintf(stderr,"READ TIME: t.tv_sec(%d), t.tv_usec(%d)\n",t.tv_sec, t.tv_usec);
		//fprintf(stderr,"READ TIME: g.tv_sec(%d), g.tv_usec(%d)\n",g.tv_sec, g.tv_usec);
		
		//gettimeofday(&t,NULL);
		if(write(file,buff,size*2048) != size*2048)
		{
			fprintf(stderr,"Can't write data to ISO  SLB(%d) CBLK(%d) ENCRYPT(%d)\n",startlb,size,0);
			free(buff);	
			return 0;
		}
		//gettimeofday(&g,NULL);
		
		//fprintf(stderr,"WRITE TIME: t.tv_sec(%d), t.tv_usec(%d)\n",t.tv_sec, t.tv_usec);
		//fprintf(stderr,"WRITE TIME: g.tv_sec(%d), g.tv_usec(%d)\n",g.tv_sec, g.tv_usec);
		blkcount -= size;
		startlb += size;
	}

	free(buff);	
	return 1;

}




int DVDcopyDiscToISO(dvd_reader_t * _dvd, int file, AddressMap * Map)
{
	int CountOfAddressMap = 0;
	//int result;
	int Count = 0;
	AddressEntry *head, *p;

	CountOfAddressMap = Map->EntryCount;
	head = &(Map->Head);
	p = head->next;

	while(p != head)
	{
		Count++;
		fprintf(stderr, "CopyAddress Entry(%d)\n",Count);

		switch(p->type)
		{
		case PLANE_AREA :
		case IFO :
		case BUP:
			if(!DVDcopyUnscambledBlock(_dvd, file, p))
			{
				return 0;
			}
			break;		
		case TITLE_VOB :
		case MENU_VOB :
			if(!DVDcopyScambledBlock(_dvd, file, p))
			{
				return 0;
			}
			break;		
		default:
			fprintf(stderr,"Error No Address type\n");
			return 0;
			break; 
		}
		p = p->next;
	}

	return 1;		
}

int DVDMirror_image(dvd_reader_t * _dvd, char * targetdir,char * title_name) {

	int i;
	DVDRip_title_set_info_t *title_set_info=NULL;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
	AddressMap		*Map = NULL;
	int 			file;
	char 			targetname[PATH_MAX];
	
	
	title_set_info = DVDGetFileSet_image(_dvd);
	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}
	
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;
	
	
	fprintf(stderr, "\n\nDVDMirror Image Address Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscSize %d\n",TotalDiscSize);

	if((Map = GenerateAddressMap(title_set_info, TotalDiscSize)) == NULL)
	{
		fprintf(stderr,"Cant Get AddressMap\n");
		DVDFreeTitleSetInfo_image(title_set_info);
		return(1);	
	} 		
	DVDFreeTitleSetInfo_image(title_set_info);	
	PrintAddressMap(Map);

	sprintf(targetname,"%s%s/%s.ISO",targetdir,title_name,title_name);
	

	if( (file = open(targetname, O_WRONLY|O_CREAT|O_LARGEFILE, 0644)) == -1)
	{
		fprintf(stderr, "Error openning %s\n",targetname);
		FreeAddressMap(Map);
		return(1);
	}	

	/* generate all keys */
	initAllCSSKeys(_dvd);
	fprintf(stderr, "dvd->css_state %d\n", SetCssState(_dvd, 2));

	
	/* read and copy */
	if(!DVDcopyDiscToISO(_dvd, file, Map))
	{
		fprintf(stderr, "DVDcopyDiscToISO Fail!\n");
	}
	
	FreeAddressMap(Map);
	close(file);	
	
	return(0);
}


int DVDMirrorTitleSet(dvd_reader_t * _dvd, char * targetdir,char * title_name, int title_set) {

	title_set_info_t * title_set_info=NULL;


#ifdef DEBUG
	fprintf(stderr,"In DVDMirrorTitleSet\n");
#endif

	title_set_info = DVDGetFileSet(_dvd);

	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}

	if ( title_set > title_set_info->number_of_title_sets ) {
		fprintf(stderr, "Can't copy title_set %d there is only %d title_sets present on this DVD\n", title_set, title_set_info->number_of_title_sets);
		DVDFreeTitleSetInfo(title_set_info);
		return(1);
	}

	if ( title_set == 0 ) {
		if ( DVDMirrorVMG(_dvd, title_set_info, targetdir, title_name) != 0 ) {
			fprintf(stderr,"Mirror of Title set 0 (VMG) faild\n");
			DVDFreeTitleSetInfo(title_set_info);
			return(1);
		}
	} else {
		if ( DVDMirrorTitleX(_dvd, title_set_info, title_set, targetdir, title_name) != 0 ) {
			fprintf(stderr,"Mirror of Title set %d faild\n", title_set);
			DVDFreeTitleSetInfo(title_set_info);
			return(1);
		}
	}
	DVDFreeTitleSetInfo(title_set_info);
	return(0);
}

int DVDMirrorMainFeature(dvd_reader_t * _dvd, char * targetdir,char * title_name) {

	title_set_info_t * title_set_info=NULL;
	titles_info_t * titles_info=NULL;


	titles_info = DVDGetInfo(_dvd);
	if (!titles_info) {
		fprintf(stderr, "Guess work of main feature film faild\n");
		return(1);
	}

	title_set_info = DVDGetFileSet(_dvd);
	if (!title_set_info) {
		DVDFreeTitlesInfo(titles_info);
		return(1);
	}

	if ( DVDMirrorTitleX(_dvd, title_set_info, titles_info->main_title_set, targetdir, title_name) != 0 ) {
		fprintf(stderr,"Mirror of main featur file which is title set %d faild\n", titles_info->main_title_set);
		DVDFreeTitleSetInfo(title_set_info);
		return(1);
	}

	DVDFreeTitlesInfo(titles_info);
	DVDFreeTitleSetInfo(title_set_info);
	return(0);
}


int DVDMirrorChapters(dvd_reader_t * _dvd, char * targetdir,char * title_name, int start_chapter,int  end_chapter, int titles) {


	int result;
	int chapters = 0;
	int feature;
	int i, s;
	int spg, epg;
	int pgc;
	int start_cell, end_cell;
	int vts_title;

	title_set_info_t * title_set_info=NULL;
	titles_info_t * titles_info=NULL;
	ifo_handle_t * vts_ifo_info=NULL;
	int * cell_start_sector=NULL;
	int * cell_end_sector=NULL;

	/* To do free memory*/

	titles_info = DVDGetInfo(_dvd);
	if (!titles_info) {
		fprintf(stderr, "Faild to obtain titles information\n");
		return(1);
	}

	title_set_info = DVDGetFileSet(_dvd);
	if (!title_set_info) {
		DVDFreeTitlesInfo(titles_info);
		return(1);
	}

	if(titles == 0) {
		fprintf(stderr, "No title specified for chapter extraction, will try to figure out main feature title\n");
		feature = titles_info->main_title_set;
		for (i=0; i < titles_info->number_of_titles ; i++ ) {
			if ( titles_info->titles[i].title_set == titles_info->main_title_set ) {
				if(chapters < titles_info->titles[i].chapters) {
					chapters = titles_info->titles[i].chapters;
					titles = i + 1;
				}
			}
		}
	}

	vts_ifo_info = ifoOpen(_dvd, titles_info->titles[titles - 1].title_set);
	if(!vts_ifo_info) {
		fprintf(stderr, "Coundn't open tile_set %d IFO file\n", titles_info->titles[titles - 1].title_set);
		DVDFreeTitlesInfo(titles_info);
		DVDFreeTitleSetInfo(title_set_info);
		return(1);
	}

	vts_title = titles_info->titles[titles - 1].vts_title;

	if (end_chapter > titles_info->titles[titles - 1].chapters) {
		end_chapter = titles_info->titles[titles - 1].chapters;
		fprintf(stderr, "Turncated the end_chapter only %d chapters in %d title\n", end_chapter,titles);
	}

	if (start_chapter > titles_info->titles[titles - 1].chapters) {
		start_chapter = titles_info->titles[titles - 1].chapters;
		fprintf(stderr, "Turncated the end_chapter only %d chapters in %d title\n", end_chapter,titles);
	}



	/* We assume the same PGC for the whole title - this is not true and need to be fixed later on */

	pgc = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[start_chapter - 1].pgcn;


	/* Lookup  PG for start chapter */

	spg = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[start_chapter - 1].pgn;

	/* Look up start cell for this pgc/pg */

	start_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->program_map[spg - 1];


	/* Lookup end cell*/


	if ( end_chapter < titles_info->titles[titles - 1].chapters ) {
		epg = vts_ifo_info->vts_ptt_srpt->title[vts_title - 1].ptt[end_chapter].pgn;
#ifdef DEBUG
		fprintf(stderr,"DVDMirrorChapter: epg %d\n", epg);
#endif

		end_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->program_map[epg -1] - 1;
#ifdef DEBUG
		fprintf(stderr,"DVDMirrorChapter: end cell adjusted %d\n", end_cell);
#endif

	} else {

		end_cell = vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->nr_of_cells;
#ifdef DEBUG
		fprintf(stderr,"DVDMirrorChapter: end cell adjusted 2 %d\n",end_cell);
#endif

	}

#ifdef DEBUG
	fprintf(stderr,"DVDMirrorChapter: star cell %d\n", start_cell);
#endif


	/* Put all the cells start and end sector in a dual array */

	cell_start_sector = (int *)malloc( (end_cell - start_cell + 1) * sizeof(int));
	if(!cell_start_sector) {
		fprintf(stderr,"Memory allocation error 1\n");
		DVDFreeTitlesInfo(titles_info);
		DVDFreeTitleSetInfo(title_set_info);
		ifoClose(vts_ifo_info);
		return(1);
	}
	cell_end_sector = (int *)malloc( (end_cell - start_cell + 1) * sizeof(int));
	if(!cell_end_sector) {
		fprintf(stderr,"Memory allocation error\n");
		DVDFreeTitlesInfo(titles_info);
		DVDFreeTitleSetInfo(title_set_info);
		ifoClose(vts_ifo_info);
		free(cell_start_sector);
		return(1);
	}
#ifdef DEBUG
	fprintf(stderr,"DVDMirrorChapter: start cell is %d\n", start_cell);
	fprintf(stderr,"DVDMirrorChapter: end cell is %d\n", end_cell);
	fprintf(stderr,"DVDMirrorChapter: pgc is %d\n", pgc);
#endif

	for (i=0, s=start_cell; s < end_cell +1 ; i++, s++) {

		cell_start_sector[i] =  vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].first_sector;
		cell_end_sector[i] =  vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].last_sector;
#ifdef DEBUG
		fprintf(stderr,"DVDMirrorChapter: S is %d\n", s);
		fprintf(stderr,"DVDMirrorChapter: start sector %d\n", vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].first_sector);
		fprintf(stderr,"DVDMirrorChapter: end sector %d\n", vts_ifo_info->vts_pgcit->pgci_srp[pgc - 1].pgc->cell_playback[s - 1].last_sector);
#endif
	}

	bsort_min_to_max(cell_start_sector, cell_end_sector, end_cell - start_cell + 1);

	align_end_sector(cell_start_sector, cell_end_sector,end_cell - start_cell + 1);

#ifdef DEBUG
	for (i=0 ; i < end_cell - start_cell + 1; i++) {
		fprintf(stderr,"DVDMirrorChapter: Start sector is %d end sector is %d\n", cell_start_sector[i], cell_end_sector[i]);
	}
#endif

	result = DVDWriteCells(_dvd, cell_start_sector, cell_end_sector , end_cell - start_cell + 1, titles, title_set_info, titles_info, targetdir, title_name);

	DVDFreeTitlesInfo(titles_info);
	DVDFreeTitleSetInfo(title_set_info);
	ifoClose(vts_ifo_info);
	free(cell_start_sector);
	free(cell_end_sector);

	if( result != 0) {
		return(1);
	} else {
		return(0);
	}
}






int DVDMirrorTitles(dvd_reader_t * _dvd, char * targetdir,char * title_name, int titles) {

	int end_chapter;

	titles_info_t * titles_info=NULL;

#ifdef DEBUG
	fprintf(stderr,"In DVDMirrorTitles\n");
#endif



	titles_info = DVDGetInfo(_dvd);
	if (!titles_info) {
		fprintf(stderr, "Faild to obtain titles information\n");
		return(1);
	}


	end_chapter = titles_info->titles[titles - 1].chapters;
#ifdef DEBUG
	fprintf(stderr,"DVDMirrorTitles: end_chapter %d\n", end_chapter);
#endif

	if (DVDMirrorChapters( _dvd, targetdir, title_name, 1, end_chapter, titles) != 0 ) {
		DVDFreeTitlesInfo(titles_info);
		return(1);
	}

	DVDFreeTitlesInfo(titles_info);

	return(0);
}

int DVDDisplayInfo(dvd_reader_t * _dvd, char * dvd) {


	int i, f;
	int chapters;
	int channels;
	char title_name[33]="";
	title_set_info_t * title_set_info=NULL;
	titles_info_t * titles_info=NULL;


	titles_info = DVDGetInfo(_dvd);
	if (!titles_info) {
		fprintf(stderr, "Guess work of main feature film faild\n");
		return(1);
	}

	title_set_info = DVDGetFileSet(_dvd);
	if (!title_set_info) {
		DVDFreeTitlesInfo(titles_info);
		return(1);
	}

	DVDGetTitleName(dvd,title_name);


	fprintf(stdout,"\n\n\nDVD-Video information of the DVD with tile %s\n\n", title_name);

	/* Print file structure */

	fprintf(stdout,"File Structure DVD\n");
	fprintf(stdout,"VIDEO_TS/\n");
	fprintf(stdout,"\tVIDEO_TS.IFO\t%i\n", title_set_info->title_set[0].size_ifo);

	if (title_set_info->title_set[0].size_menu != 0 ) {
		fprintf(stdout,"\tVIDEO_TS.VOB\t%i\n", title_set_info->title_set[0].size_menu);
	}

	fprintf(stdout,"\tVIDEO_TS.BUP\t%i\n", title_set_info->title_set[0].size_bup);

	for( i = 0 ; i < title_set_info->number_of_title_sets ; i++) {
		fprintf(stdout,"\tVTS_%02i_0.IFO\t%i\n", i + 1, title_set_info->title_set[i + 1].size_ifo);
		if (title_set_info->title_set[i + 1].size_menu != 0 ) {
			fprintf(stdout,"\tVTS_%02i_0.VOB\t%i\n", i + 1, title_set_info->title_set[i + 1].size_menu);
		}
		if (title_set_info->title_set[i + 1].number_of_vob_files != 0) {
			for( f = 0; f < title_set_info->title_set[i + 1].number_of_vob_files ; f++ ) {
				fprintf(stdout,"\tVTS_%02i_%i.VOB\t%i\n", i + 1, f + 1, title_set_info->title_set[i + 1].size_vob[f]);
			}
		}
		fprintf(stdout,"\tVTS_%02i_0.BUP\t%i\n", i + 1, title_set_info->title_set[i + 1].size_bup);
	}

	fprintf(stdout,"\n\nMain feature:\n");
	fprintf(stdout,"\tTitle set containing the main feature is  %d\n", titles_info->main_title_set);
	for (i=0; i < titles_info->number_of_titles ; i++ ) {
		if (titles_info->titles[i].title_set == titles_info->main_title_set) {
			if(titles_info->titles[i].aspect_ratio == 3) {
				fprintf(stdout,"\tThe aspect ratio of the main feature is 16:9\n");
			} else if (titles_info->titles[i].aspect_ratio == 0) {
				fprintf(stdout,"\tThe aspect ratio of the main feature is 4:3\n");
			} else {
				fprintf(stdout,"\tThe aspect ratio of the main feature is unknown\n");
			}
			fprintf(stdout,"\tThe main feature has %d angle(s)\n", titles_info->titles[i].angles);
			fprintf(stdout,"\tThe main feature has %d audio_track(s)\n", titles_info->titles[i].angles);
			fprintf(stdout,"\tThe main feature has %d subpicture channel(s)\n",titles_info->titles[i].sub_pictures);
			chapters=0;
			channels=0;

			for (f=0; f < titles_info->number_of_titles ; f++ ) {
                        	if ( titles_info->titles[i].title_set == titles_info->main_title_set ) {
                                	if(chapters < titles_info->titles[f].chapters) {
                                        	chapters = titles_info->titles[f].chapters;
					}
					if(channels < titles_info->titles[f].audio_channels) {
						channels = titles_info->titles[f].audio_channels;
					}
				}
			}
			fprintf(stdout,"\tThe main feature has a maximum of %d chapter(s) in on of it's titles\n", chapters);
			fprintf(stdout,"\tThe main feature has a maximum of %d audio channel(s) in on of it's titles\n", channels);
			break;
                }

        }

	fprintf(stdout,"\n\nTitle Sets:");
	for (f=0; f < title_set_info->number_of_title_sets ; f++ ) {
		fprintf(stdout,"\n\n\tTitle set %d\n", f + 1);
		for (i=0; i < titles_info->number_of_titles ; i++ ) {
			if (titles_info->titles[i].title_set == f + 1) {
				if(titles_info->titles[i].aspect_ratio == 3) {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is 16:9\n", f + 1);
				} else if (titles_info->titles[i].aspect_ratio == 0) {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is 4:3\n", f + 1);
				} else {
					fprintf(stdout,"\t\tThe aspect ratio of title set %d is unknown\n", f + 1);
				}
				fprintf(stdout,"\t\tTitle set %d has %d angle(s)\n", f + 1, titles_info->titles[i].angles);
				fprintf(stdout,"\t\tTitle set %d has %d audio_track(s)\n", f + 1, titles_info->titles[i].angles);
				fprintf(stdout,"\t\tTitle set %d has %d subpicture channel(s)\n", f + 1, titles_info->titles[i].sub_pictures);
				break;
			}
		}
		fprintf(stdout,"\n\t\tTitles included in title set %d is/are\n", f + 1);
		for (i=0; i < titles_info->number_of_titles ; i++ ) {
			if (titles_info->titles[i].title_set == f + 1) {
				fprintf(stdout,"\t\t\tTitle %d:\n", i + 1);
				fprintf(stdout,"\t\t\t\tTitle %d has %d chapter(s)\n", i + 1, titles_info->titles[i].chapters);
				fprintf(stdout,"\t\t\t\tTitle %d has %d audio channle(s)\n", i + 1, titles_info->titles[i].audio_channels);
			}
		}
	}
	DVDFreeTitlesInfo(titles_info);
	DVDFreeTitleSetInfo(title_set_info);

	return(0);
}


int Get_DVD_Size(dvd_reader_t * _dvd) {

	int i;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
		
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;

	if(i == 0) {
		fprintf(stderr, "Get_DVD_Size Error : Can't find partition\n");
		return 0;	
	}
	
	fprintf(stderr, "\n\nGet_DVD_Size  Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscBlockSize %d\n",(TotalDiscSize * 4));
	
	return (TotalDiscSize * 4);
}






int COPY_TO_DEV(dvd_reader_t * _dvd, char * targetdevfile ) {

	int i;
	DVDRip_title_set_info_t *title_set_info=NULL;
	struct Partition  	part;	
	PartInfo 		pInfo[10];
	unsigned int 		PartCount = 0;
	unsigned int 		TotalDiscSize = 0;
	int			LastPartIndex = 0;
	AddressMap		*Map = NULL;
	int 			file;
	
	
	title_set_info = DVDGetFileSet_image(_dvd);
	if (!title_set_info) {
		DVDClose(_dvd);
		return(1);
	}
	
	/*get partition infomation */
	for(i = 0; i<10; i++)
	{
		if(UDFFindPartition(_dvd,i,&part))
		{
			pInfo[i].num = part.Number;
			pInfo[i].start = part.Start;
			pInfo[i].length = part.Length;
		}else {
			break;
		}
	}
	
	PartCount = i;
	
	
	fprintf(stderr, "\n\nDVDMirror Image Address Calc  START!\n");
	

	for(i = 0; i < PartCount; i++)
	{
		fprintf(stderr,"PART NUM %d, START:%d, SEC_COUNT:%d\n",
			pInfo[i].num, pInfo[i].start, pInfo[i].length);
		if(pInfo[LastPartIndex].start < pInfo[i].start)
		{
			LastPartIndex = i;
		}		
	}
	
	TotalDiscSize = pInfo[LastPartIndex].start + pInfo[LastPartIndex].length;
	
	fprintf(stderr, "TotalDiscBlockSize(per 2048) %d\n",TotalDiscSize);

	if((Map = GenerateAddressMap(title_set_info, TotalDiscSize)) == NULL)
	{
		fprintf(stderr,"Cant Get AddressMap\n");
		DVDFreeTitleSetInfo_image(title_set_info);
		return(1);	
	} 		
	DVDFreeTitleSetInfo_image(title_set_info);	
	PrintAddressMap(Map);

	

	if( (file = open(targetdevfile, O_WRONLY)) == -1)
	{
		fprintf(stderr, "Error openning %s\n",targetdevfile);
		FreeAddressMap(Map);
		return(1);
	}	

	/* generate all keys */
	initAllCSSKeys(_dvd);
	fprintf(stderr, "dvd->css_state %d\n", SetCssState(_dvd, 2));

	
	/* read and copy */
	if(!DVDcopyDiscToISO(_dvd, file, Map))
	{
		fprintf(stderr, "DVDcopyDiscToISO Fail!\n");
		FreeAddressMap(Map);
		return(1);
	}
	
	FreeAddressMap(Map);
	close(file);	
	
	return(0);
}

int GetDVDSize( char * Sdevfile, int * size)
{
	dvd_reader_t * _dvd = NULL;
	unsigned int 		TotalDiscSize = 0;
	
	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDSize Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}	

	if(!(TotalDiscSize = Get_DVD_Size(_dvd)))
	{
		fprintf(stderr, "GetDVDSize Fail Get_DVD_Size\n");
		DVDClose(_dvd);
		return -1;
	}
	
	*size = TotalDiscSize;
	DVDClose(_dvd);
	return 0;
	
}

int SetDVDCopytoMedia( char * Sdevfile, char * Ddevfile)
{
	dvd_reader_t * _dvd = NULL;
	_dvd = DVDOpen(Sdevfile);
	if(!_dvd) {
		fprintf(stderr, "GetDVDCopytoMedia Can't Open Dvd dev file %s\n", Sdevfile);
		return -1;
	}
	
	if(COPY_TO_DEV(_dvd, Ddevfile) != 0)
	{
		fprintf(stderr, "GetDVDCopytoMedia Error : COPY_TO_DEV fail!\n");
		DVDClose(_dvd);	
		return -1;
	} 	
	DVDClose(_dvd);	
	return 0;
}

int GetDVDTitleName(char * Sdevfile, char * buf)
{
	if( DVDGetTitleName(Sdevfile, buf) != 0)
	{
		fprintf(stderr, "GetDVDTitleName fail get title %s\n", Sdevfile);
		return -1;
	}

	return 0;
}
