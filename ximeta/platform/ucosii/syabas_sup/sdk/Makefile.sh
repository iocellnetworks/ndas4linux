
# [20020815] Peter, 1st modify Makefile
# [20031013] Roland Hii, generic Makefile

#
# Compiler and Tools
#
BIN			= /opt/tools-install/bin

ifneq (true,$(strip $(debug_mem)))
NEWLIB_DIR	= /usr/local/arm-elf.1.10.0
else
NEWLIB_DIR	= /usr/local/arm-elf.1.10.0_debug
override DFLAGS += -DDEBUG_MEM
endif

GCCDIR		= /opt/tools-install/lib/gcc-lib/arm-elf/2.95.3
LIBC		= $(NEWLIB_DIR)/lib
INCLUDES	= -I$(NEWLIB_DIR)/include -I$(GCCDIR)/include
#-I$//opt/tools-install/arm-elf/include

TFTPBOOT	= /tftpboot

CC			= $(BIN)/arm-elf-gcc
LD			= $(BIN)/arm-elf-ld
AS			= $(BIN)/arm-elf-as
OBJCOPY		= $(BIN)/arm-elf-objcopy
RM			= rm -f
OBJDUMP		= arm-elf-objdump

COPY		= cp
RENAME		= mv
ECHO		= @echo
GCC			= gcc
GZIP		= gzip
SPLIT		= split

# Syabas tools
ROMDUP		= ./romdup

#
# compile flags
#
override CFLAGS	+=	-O2 -fomit-frame-pointer -fno-builtin -nostdinc -nostdlib \
					-ffunction-sections -fdata-sections -Wall -Werror
#-mapcs-frame
C2FLAGS	= -g -O2 -fomit-frame-pointer -fno-builtin -nostdinc -nostdlib -ffunction-sections -fdata-sections -Wall
# -g					(debugging option)
# -O2					(Optimization level 2)
# -fomit-frame-pointer	(don't keep frame pointer in reg - make debugging impossible on some machines)
# -fno-builtin			(don't recognise built-in functions that do not begin with __builtin_)
# -nostdinc				(don't search std system dir for header files, only those specified by -I)
# -nostdlib				(don't use std system startup files or lib when linking)
# -ffunction-sections	(place each function
# -fdata-sections
# -Wall					(turn on all warnings)
# -mthumb				(thumb mode) not applicable

#
# define flags (general)
#
#  platform related:
#    SYABAS3				- Syabas3 project - Sigma Design's Mambo platfrom
#    SYB_LITTLE_ENDIAN		- compile in little endian mode
#  TCPIP related:
#    TCPIP_PORT				- TCPIP porting
#    ARM					- NOS for ARM CPU
#    USE_RECV_ISR			- legacy Syabas1 NE2000 driver, interrupt or polling mode rx
#    SNTP					- Simple Network Time Protocol support
#	 MULTI_SOCKET_PROC		- concurrent socket
#    NIC_MAC				- not hardcode MAC address
#    DHCP					- Dynamic Host Configuration Protocol
#    LOCAL_BUS				- EM855x local bus interface
#  video related:
#    HWLIB_PORT				- Sigma Designs hwlib porting
#	 OLD_HWLIB				- Hardware hwlib_20040818 and before
#    WITH_OSDFB				- use OSD framebuffer
#    _ARM_ASSEM_			-
#    MUTEX_SD_API			- use mutex for Sigma Design's API
#	 MBUF_MEM_FIX			- fix mbuf mem leak problem //neoh
#	 MBUF_MEM_FIX			- fix mbuf mem leak problem //neoh
#  IR related:
#    IR						- infrared signal handling
#    IRREP					- handling of IR repeat signal
#  flash related:
#    USE_FLASH				- use flash drive (D:\)
#  setup related:
#    LANSETUP				- enable LAN setup module
#  WiFi related
#    PCMCIA_WIFI			- enable PCMCIA WiFi
#  uCOS related:
#  	 USE_OS_VIEW			- enable OS View, disables the debugger

DFLAGS_MAIN = -DSYABAS3 -DSYB_LITTLE_ENDIAN \
			  -DTCPIP_PORT -DARM -DSNTP -DDHCP -DUSE_PCI \
			  -DRMPLATFORM=RMPLATFORMID_JASPERMAMBO \
			  -DHWLIB_PORT \
			  -DIR -DIRREP \
			  -DUSE_FLASH -DPING_PONG_UPDATE \
			  -DLANSETUP -DRUA_MALLOC
#			  -DUSE_OS_VIEW \
#			  -DMBUF_MEM_FIX \
#			  -DNIC_MAC -DLOCAL_BUS \
#			  -DWITH_OSDFB -DMUTEX_SD_API \
#			  -DPCMCIA_WIFI \
#
# obsolete
# -DUCOS_ARM
# -DDOUBLE_OSD_BUFFER

#-DPRODUCTION_MODE -DOS_STACK_CHECK -DHAS_DVI

#
# define flags (additional)
#
#  tasks:
#    YAHOO_MESSENGER_TASK	- Yahoo! messenger client
#    UPDATE_TASK			- update firmware
#    SCREEN_SAVER_TASK		- screen saver after browser idle for some period of time
#  VOD related:
#    VOD					- Video on Demand
#    PIP					- Picture in picture
#    AVI_SEEK				- Enable seeking in AVI
#    ASF_SEEK				- Enable seeking in ASF
#    PHOTO_ALBUM			- Support Photo Album
#    IRADIO					- Support Internet Radio
#    INCLUDE_AAC			- Support AAC audio codec
#    INCLUDE_WMA			- Support WMA audio codec
#    SUPPORT_PAL_YUV		- Enable PAL yuv
#    SEM_VOD				- VOD control giveup to TCPIP
#    INCLUDE_OGG			- Ogg Vorbis codec support in AVI
#    WMA_RADIO				- WMA Internet Radio (in progress)
#    AUDIO_PROPERTIES		- Enable Audio selection for Stereo or AC35.1
#    MALLOC_PHOTO			- decode JPEG in heap memory
#    ROTATE_JPEG			- Support JPEG rotation
#    RANDOM_PLAYLIST		- RANDOM and REPEAT RANDOM playlist
#    LIVE_TV				- enable live tv streaming frm WinDVR
#    MPEG4_OVER_MPEG2_PS	- MPEG4 over MPEG2 Program Stream
#    _DIVX3					- DivX 3.11
#    HAS_ADVERTISEMENT		- Advertisement capability for Video On Demand
#	 ZOOM_SQUARE_PIXEL		- Zoom mode - adjust height to 7/8,look squeare on tv
#  OSD related:
#    OSD_DISPLAY			- enable display of OSD message on the screen
#    OSD_IMAGE				- enable display of OSD message using images
#	 OSD_LAPTIME_DISPLAY	- enable display of playback elapsed, remaining and total time
#  browser related:
#    CONSUMER_HTML			- enable consumer html (tv html)
#    CACHEBMP=n				- caching of BITMAP (including JPEG and PNG) images
#    CACHEGIF=n				- caching of GIF images for faster page scrolling
#							n = 0 - disable images caching
#								1 - enable caching of images in the page only
#								2 - also enable caching of image as background of the page
#    PNG_FORMAT_SUPPORTED	- PNG image format support
#    IMG_ONFOCUS			- enable emulation of image onfocus
#    IFRAMES				- enable iframe
#    COMPACT_BROWSER		- enable compact mode
#    HD_BROWSER				- enable HD mode mode browser (Syabas3)
#    HD_PANEL				- use HD mode panel (should only use with HD_BROWSER)
#    SY_ONFOCUSSET			- enable onfocusset (syabas html extension - onfocus emulation)
#    SY_FOCUSCOLOR			- enable dynamic assigning of focus color (syabas html extension - focuscolor)
#    SY_IRKEY				- enable dynamic remap of key function (syabas html extension)
#    SY_BGYUV				- render background image (JPEG) in YUV frame
#    SY_AUTOSUBMIT			- simulate keypress ENTER after a keypress is confirmed
#	 USE_16BPP				- use 16-bit color browser (RGBA:4444 format)
#	 DEPTH15ONLY			- use 15-bit color depth for jpeg library
#  UPnP related:
#    WAKE_ON_LAN			- enable the Wake on LAN (WoL)
#    UPNP					- Universal Plug n Play
#    RENDEZVOUS				- Apple Rendezvous
#  SSL related:
#    USE_SSL				- enable SSL
#  DVD related:
#    DVD_PORT				-
#    PLAYER_NEXTBASE		-
#  File player related
#    SY_USB					- support IDE harddisk access (may need the DVD_PORT to be there
#    HARDDISK				- enable IDE harddisk support
#	 MEDIA_UPLOAD			- uploads selected file to the current iHome
#  PCMCIA memory card
#    PC_MEM_CARD			-
#  FIP related
#    USE_FIP				- enable FIP (VFD controller) support.
#							- Have to define FIP_** to use in sybopt.h. Default is FIP_SYABAS
#  TV related:
#	 CHANGE_TV_MODE			- for TV mode on the fly - Prompt: satisfy with current TV mode?
#							- If no "OK" press after 2sec, roll bck 2previous previous
#  USING_SEPERATE_INT		- for USB and wired/wireless LAN interrupt to be seperated
# HAS_SECRET_MENU			- secret menu handling, by pressing slow key followed by 4 integer value

DFLAGS_ADD = -DUPDATE_TASK -DSCREEN_SAVER_TASK \
			 -DVOD -DPIP -DAVI_SEEK -DPHOTO_ALBUM -DIRADIO -DINCLUDE_AAC -DINCLUDE_WMA \
			 -DSEM_VOD -DINCLUDE_OGG \
			 -DMALLOC_PHOTO \
			 -DOSD_DISPLAY \
			 -DCONSUMER_HTML -DCACHEBMP=2 -DCACHEGIF=2 -DPNG_FORMAT_SUPPORTED \
			 -DIMG_ONFOCUS -DIFRAMES -DCOMPACT_BROWSER -DSY_ONFOCUSSET -DSY_FOCUSCOLOR \
			 -DEM86XX_MODE=EM86XX_MODEID_STANDALONE \
			 -DSY_BGYUV \
			 -DWAKE_ON_LAN -DUPNP -DRENDEZVOUS \
			 -DHD_BROWSER -DHD_PANEL \
			 -DDVD_PORT -DNEW_SW_STANDBY -DUSE_SSL \
			 -DASF_SEEK -DPHOTO_FADING -DMPEG4_OVER_MPEG2_PS \
			 -DINCLUDE_NDAS
#			 -DUSE_16BPP -DDEPTH15ONLY \
#			 -DSUPPORT_PAL_YUV \
#			 -DAUDIO_PROPERTIES \
#			 -DSY_IRKEY -DSY_AUTOSUBMIT \
#			 -DHAS_SECRET_MENU

# [20021226] Roland Hii, default goal
override DFLAGS += $(DFLAGS_MAIN) $(DFLAGS_ADD)

# [20021226] Roland Hii, flash
ifeq (flash,$(MAKECMDGOALS))
override DFLAGS = $(DFLAGS_MAIN) -DFLASHMAP
endif

#
# [20030305] Roland Hii, customer list:
#
#   Japan 		- IOData (JP_IODATA), Vertexlink (JP_VERTEXLINK), Elsa (JP_ELSA);
#	Taiwan		- LiteonIT (TW_LITEONIT), Twinhead (TW_TWINHEAD),
#				  FIC (TW_FIC), DoTop (TW_DOTOP_1);
#   US 			- Microsoft (US_MICROSOFT);

ifdef customer

ifneq (JP_IODATA_1,$(strip $(customer)))
ifneq (JP_IODATA_2,$(strip $(customer)))
ifneq (JP_IODATA_3,$(strip $(customer)))
ifneq (JP_IODATA_4,$(strip $(customer)))
ifneq (JP_VERTEXLINK,$(strip $(customer)))
ifneq (JP_ELSA,$(strip $(customer)))
ifneq (TW_LITEONIT_1,$(strip $(customer)))
ifneq (TW_LITEONIT_2,$(strip $(customer)))
ifneq (TW_LITEONIT_3,$(strip $(customer)))
ifneq (TW_LITEONIT_4,$(strip $(customer)))
ifneq (TW_TWINHEAD,$(strip $(customer)))
ifneq (TW_FIC,$(strip $(customer)))
ifneq (TW_DOTOP_1,$(strip $(customer)))
ifneq (US_MICROSOFT,$(strip $(customer)))
$(error invalid customer `$(customer)')
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif

override DFLAGS_ADD += -D$(customer) -DCUSTOMER_BUILD

#avlp2_dvdg (NetDVD)
ifeq (JP_IODATA_1,$(strip $(customer)))
override DFLAGS_ADD += -DFILE_FILTER -UIRREP\
					   -DDIVX_SUB \
					   -DUSB_PORT2 -DSY_BGYUV_LFILE \
					   -DJP_IODATA\
					   -UHD_PANEL -UCOMPACT_BROWSER -USUBTITLE \
					   -DNEW_SW_STANDBY \
					   -DIODATA_DVDG
#					   -DUSE_ATMEL_EEPROM -DZOOM_SQUARE_PIXEL

#override DFLAGS_ADD += -UYAHOO_MESSENGER_TASK -UHAS_PAL -D_DIVX3 \
#					   -DFILE_FILTER -DDIVX_SUB \
#					   -DH_INT_DISPATCH -DUSE_ATMEL_EEPROM
#					   -DHARDDISK -DDIVX_CERT
#pcmcia_mem_card=true
#hwlib-revb-406=true
bdtype=avlp2_dvdg
#dboard_a1=true
divxdrm=true
dvdplayer=true
himatplayer=true
fip=true
usb=viavt6202
broadcom_minipci=true
#OEM: CN_YAHSIN

flashdskfn		= $(CUSTOMERDIR)/iodata255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_iodata
RESOURCEDIR		= $(RESOURCEROOTDIR)/iodata
SETUPDIR		= $(SETUPROOTDIR)/setup_iodata
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/iodata
endif	#AVLP2/DVDG

#avlp2_g (DMA)
ifeq (JP_IODATA_2,$(strip $(customer)))
override DFLAGS_ADD += -DFILE_FILTER -UIRREP\
					   -DDIVX_SUB \
					   -DUSB_PORT2 -DSY_BGYUV_LFILE \
					   -DJP_IODATA \
					   -UHD_PANEL -UCOMPACT_BROWSER -USUBTITLE \
					   -DNEW_SW_STANDBY \
					   -DIODATA_G
#					   -DUSE_ATMEL_EEPROM -DZOOM_SQUARE_PIXEL

#override DFLAGS_ADD += -UYAHOO_MESSENGER_TASK -UHAS_PAL -D_DIVX3 \
#					   -DFILE_FILTER -DDIVX_SUB \
#					   -DH_INT_DISPATCH -DUSE_ATMEL_EEPROM
#					   -DHARDDISK -DDIVX_CERT
#pcmcia_mem_card=true
#hwlib-revb-406=true
bdtype=avlp2_dvdg
#dboard_a1=true
divxdrm=true
#dvdplayer=true
#himatplayer=true
#fip=true
usb=viavt6202
broadcom_minipci=true
#OEM: CN_YAHSIN

flashdskfn		= $(CUSTOMERDIR)/iodata255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_iodata
RESOURCEDIR		= $(RESOURCEROOTDIR)/iodata
SETUPDIR		= $(SETUPROOTDIR)/setup_iodata
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/iodata
endif	#AVLP2/G

#avlp2_dvdl_us (NetDVD - costdown version)
ifeq (JP_IODATA_3,$(strip $(customer)))
override DFLAGS_ADD += -DFILE_FILTER -UIRREP\
					   -DDIVX_SUB \
					   -DUSB_PORT2 -DSY_BGYUV_LFILE \
					   -DJP_IODATA\
					   -DNEW_SW_STANDBY\
					   -DIODATA_DVDL_US\
					   -UHD_PANEL -UCOMPACT_BROWSER -USUBTITLE
#					   -DUSE_ATMEL_EEPROM -DZOOM_SQUARE_PIXEL

#override DFLAGS_ADD += -UYAHOO_MESSENGER_TASK -UHAS_PAL -D_DIVX3 \
#					   -DFILE_FILTER -DDIVX_SUB \
#					   -DH_INT_DISPATCH -DUSE_ATMEL_EEPROM
#					   -DHARDDISK -DDIVX_CERT
bdtype=avlp2_dvdl
divxdrm=true
dvdplayer=true
himatplayer=true
fip=true
usb=viavt6202
#OEM: CN_YAHSIN

flashdskfn		= $(CUSTOMERDIR)/iodata_en255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_iodata
RESOURCEDIR		= $(RESOURCEROOTDIR)/iodata
SETUPDIR		= $(SETUPROOTDIR)/setup_iodata
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/iodata
endif	#AVLP2/DVDL - US version

#avlp2_dvdl_jp (NetDVD - costdown version)
ifeq (JP_IODATA_4,$(strip $(customer)))
override DFLAGS_ADD += -DFILE_FILTER -UIRREP\
					   -DDIVX_SUB \
					   -DUSB_PORT2 -DSY_BGYUV_LFILE \
					   -DJP_IODATA\
					   -DNEW_SW_STANDBY \
					   -DIODATA_DVDL_JP \
					   -UHD_PANEL -UCOMPACT_BROWSER -USUBTITLE
#					   -DUSE_ATMEL_EEPROM

#override DFLAGS_ADD += -UYAHOO_MESSENGER_TASK -UHAS_PAL -D_DIVX3 \
#					   -DFILE_FILTER -DDIVX_SUB -DZOOM_SQUARE_PIXEL \
#					   -DH_INT_DISPATCH -DUSE_ATMEL_EEPROM
#					   -DHARDDISK -DDIVX_CERT
bdtype=avlp2_dvdl
divxdrm=true
dvdplayer=true
himatplayer=true
fip=true
usb=viavt6202
#OEM: CN_YAHSIN

flashdskfn		= $(CUSTOMERDIR)/iodata255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_iodata
RESOURCEDIR		= $(RESOURCEROOTDIR)/iodata
SETUPDIR		= $(SETUPROOTDIR)/setup_iodata
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/iodata
endif	#AVLP2/DVDL - Japan version

#LiteonIT - Buffalo release
ifeq (TW_LITEONIT_1,$(strip $(customer)))
override DFLAGS_ADD += -DBUFFALO_INC -DSUBTITLE \
					   -DRANDOM_PLAYLIST -DVOD_INFO \
					   -DID3V2_JPEG -DDIVX_SUB -DDIMMER \
					   -UUSE_SSL -UCOMPACT_BROWSER -DFILE_FILTER \
					   -DSY_BGYUV_LFILE -DROTATE_JPEG -DZOOM_JPEG -DPORT_ZOOM \
					   -DCHANGE_TV_MODE -DTW_LITEONIT \
					   -UCOMPACT_BROWSER -UNEW_SW_STANDBY -DHAS_SECRET_MENU

divxdrm=true
dvdplayer=true
fip=true
bdtype=liteonit_lvd2020
usb=viavt6202
broadcom_minipci=true
#buffalosdk=true
#aoss=true
#dboard_a1=true

flashdskfn		= $(CUSTOMERDIR)/liteonit_buff255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_liteonit
SETUPDIR		= $(SETUPROOTDIR)/setup_liteonit
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/liteonit_buffalo

ifeq (true,$(strip $(aoss)))
override DFLAGS_ADD += -DUSE_AOSS
endif
endif	#Buffalo

#LiteonIT - Nagase DVX-600
ifeq (TW_LITEONIT_2,$(strip $(customer)))
override DFLAGS_ADD += -DNAGASE_DVX_600 -DSUBTITLE \
					   -DRANDOM_PLAYLIST -DVOD_INFO \
					   -DID3V2_JPEG -DDIVX_SUB -DDIMMER \
					   -UUSE_SSL -UCOMPACT_BROWSER -DFILE_FILTER \
					   -DSY_BGYUV_LFILE -DROTATE_JPEG -DZOOM_JPEG -DPORT_ZOOM \
					   -DCHANGE_TV_MODE -DTW_LITEONIT \
					   -UCOMPACT_BROWSER -UNEW_SW_STANDBY -DHAS_SECRET_MENU

divxdrm=true
dvdplayer=true
fip=true
bdtype=nagase_dvx_600
usb=viavt6202
broadcom_minipci=true

flashdskfn		= $(CUSTOMERDIR)/liteonit_nagase255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_liteonit
SETUPDIR		= $(SETUPROOTDIR)/setup_liteonit
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/liteonit_buffalo
endif	#Nagase DVX-600

#Liteon IT Brain
ifeq (TW_LITEONIT_3,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DRANDOM_PLAYLIST -DVOD_INFO \
					   -DID3V2_JPEG -DDIVX_SUB -DDIMMER \
					   -UUSE_SSL -UCOMPACT_BROWSER -DFILE_FILTER \
					   -DSY_BGYUV_LFILE -DROTATE_JPEG -DZOOM_JPEG -DPORT_ZOOM \
					   -DCHANGE_TV_MODE -DTW_LITEONIT \
					   -UCOMPACT_BROWSER -UNEW_SW_STANDBY -DHAS_SECRET_MENU

divxdrm=true
dvdplayer=true
fip=true
bdtype=liteonit_lvd2020
usb=viavt6202
broadcom_minipci=true

flashdskfn		= $(CUSTOMERDIR)/liteonit255.dsk
FIPDIR			= $(FIPROOTDIR)/fip_liteonit
SETUPDIR		= $(SETUPROOTDIR)/setup_liteonit
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/liteonit
endif	#Liteon IT Brain

#LiteonIT - Nagase DMA-100
ifeq (TW_LITEONIT_4,$(strip $(customer)))
override DFLAGS_ADD += -DNAGASE_DMA_100 -DSUBTITLE \
					   -DRANDOM_PLAYLIST -DVOD_INFO \
					   -DID3V2_JPEG -DDIVX_SUB \
					   -UUSE_SSL -UCOMPACT_BROWSER -DFILE_FILTER \
					   -DSY_BGYUV_LFILE -DROTATE_JPEG -DZOOM_JPEG -DPORT_ZOOM \
					   -DCHANGE_TV_MODE -DTW_LITEONIT \
					   -UCOMPACT_BROWSER -UNEW_SW_STANDBY -DHAS_SECRET_MENU \
					   -DOSD_LAPTIME_DISPLAY

use_64m=true
divxdrm=true
#dvdplayer=true
fip=true
bdtype=liteonit_lvd2020
usb=viavt6202
broadcom_minipci=true

flashdskfn		= $(CUSTOMERDIR)/liteonit_nagase255.dsk
SETUPDIR		= $(SETUPROOTDIR)/setup_liteonit
DIVXDRMDIR		= $(VODDIR)/divx_drm_customer/liteonit_buffalo
endif	#Nagase DMA-100

#Vertexlink
ifeq (JP_VERTEXLINK,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DROTATE_JPEG -DZOOM_JPEG -DPHOTO_FADING -DUSB_PORT2 \
					   -DAUDIO_PROPERTIES
#-DMS_DRM
#					   -D_DIVX3
usb=viavt6202
bdtype=rev_a2
broadcom_minipci=true

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF

flashdskfn		= $(CUSTOMERDIR)/mediawiz255.dsk
RESOURCEDIR		= $(RESOURCEROOTDIR)/vertexlink
endif	#JP_VERTEXLINK

#Twinhead
ifeq (TW_TWINHEAD,$(strip $(customer)))

override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DWIFI_TEST_PAGE

#hwlib-revb-406=true
new_hwlib=true
dvdplayer=true
fip=true
himatplayer=true
broadcom_minipci=true
usb=viavt6202
#language: English + Japanese
#use_gb2312=true
bdtype=twd_rev_a2	#for separate PCI & usb interrupt, no DVI
use_64m=true
oem=OEM_TW_TWINHEAD

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF

flashdskfn		= $(CUSTOMERDIR)/twinhead255.dsk
RESOURCEDIR		= $(RESOURCEROOTDIR)/twinhead
SETUPDIR		= $(SETUPROOTDIR)/setup_twinhead
endif	#TW_TWINHEAD

#FIC
ifeq (TW_FIC,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DCHANGE_TV_MODE -DHAS_SECRET_MENU -DUSB_PORT2

dvdplayer=true
fip=true
himatplayer=true
usb=viavt6202
broadcom_minipci=true
bdtype=fic_rev_a2	#for separate PCI & usb interrupt
use_gb2312=true
use_64m=true
#oem=

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF

flashdskfn		= $(CUSTOMERDIR)/fic255.dsk
#RESOURCEDIR	= $(RESOURCEROOTDIR)/fic
FIPDIR			= $(FIPROOTDIR)/fip_fic
endif	#FIC

#DOTOP
ifeq (TW_DOTOP_1,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES -DHARDDISK \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DCHANGE_TV_MODE -DTW_DOTOP -DHAS_SECRET_MENU -DSY_USB

bdtype=rev_a2
dvdplayer=true
fip=true
himatplayer=true
usb=viavt6202
broadcom_minipci=true

flashdskfn		= $(CUSTOMERDIR)/dotop255_1.dsk
FIPDIR			= $(FIPROOTDIR)/
RESOURCEDIR		= $(RESOURCEROOTDIR)/dotop_1
SETUPDIR		= $(SETUPROOTDIR)/setup_dotop

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF
endif	#TW_DOTOP_1

#Elsa
ifeq (JP_ELSA,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DCHANGE_TV_MODE

# -DUSE_16BPP -DDEPTH15ONLY
#-DZOOM_JPEG -DPORT_ZOOM2 must on together, neoh temporary flag to turn more zoom port on
#ZOOM_VIDEO zoom actual,fit, fullscreen
#-DMS_DRM
#					   -DROTATE_JPEG -DZOOM_JPEG
#					   -D_DIVX3
#
bdtype=rev_a2
dvdplayer=true
fip=true
broadcom_minipci=true
#xml_parser=true
himatplayer=true
usb=viavt6202
#use_gb2312=true
#use_64m=true
#upnpav?=localcp
#upnpav?=remoteui

flashdskfn		= $(CUSTOMERDIR)/elsa255.dsk
#FIPDIR			= $(FIPROOTDIR)/
#RESOURCEDIR		= $(RESOURCEROOTDIR)/dotop_1
#SETUPDIR		= $(SETUPROOTDIR)/setup_dotop

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF
endif	#ELSA

#Microsoft
ifeq (US_MICROSOFT,$(strip $(customer)))
override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DCHANGE_TV_MODE \
					   -DUSE_16BPP -DDEPTH15ONLY

bdtype=rev_a2
dvdplayer=true
fip=true
himatplayer=true
usb=viavt6202
broadcom_minipci=true
use_64m=true
upnpav=localcp

flashdskfn		= $(CUSTOMERDIR)/microsoft255.dsk
endif	#US_MICROSOFT

else # ndef customer

override DFLAGS_ADD += -DSUBTITLE \
					   -DLIVE_TV -DRANDOM_PLAYLIST \
					   -DSY_BGYUV_LFILE -DID3V2_JPEG \
					   -DNUMERIC_GOTO -DVOD_INFO -DVOD_PROGRESSBAR \
					   -DDIVX_SUB \
					   -DAUDIO_PROPERTIES \
					   -DZOOM_VIDEO -DZOOM_JPEG -DPORT_ZOOM2 \
					   -DCHANGE_TV_MODE -DOSD_LAPTIME_DISPLAY

# -DUSE_16BPP -DDEPTH15ONLY
#-DZOOM_JPEG -DPORT_ZOOM2 must on together, neoh temporary flag to turn more zoom port on
#ZOOM_VIDEO zoom actual,fit, fullscreen
#-DMS_DRM
#					   -DROTATE_JPEG -DZOOM_JPEG
#					   -D_DIVX3
#
#hwlib-revb-406=true
#bdtype=compact
#bdtype=rev_a2
#dvdplayer=true
#fip=true
#xml_parser=true
#himatplayer=true
#usb=sl811hs
#dboard_a1=true
#usb=viavt6202
#use_gb2312=true
broadcom_minipci=true
#atheros_madhal=true
#pcmcia_mem_card=true
#use_64m=true
#upnpav?=localcp
#upnpav?=remoteui

# -DYAHOO_MESSENGER_TASK -DHAS_ADVERTISEMENT -DSY_USB -DHARDDISK -DWMA_IRADIO
# -DDIVX_CERT -DVIDEO_ZOOM_16_9 -DINCLUDE_ASF

endif	#Syabas3 generic

#
# OEM list:
#

ifdef oem

ifneq (OEM_TW_LITEONIT,$(strip $(oem)))
ifneq (OEM_CN_YAHSIN,$(strip $(oem)))
ifneq (OEM_TW_TWINHEAD,$(strip $(oem)))
ifneq (OEM_CN_SYABAS,$(strip $(oem)))
$(error invalid oem `$(oem)')
endif
endif
endif
endif

override DFLAGS_ADD += -D$(oem)

endif # def oem

#
# [20030826] Kenny Cheah, modified for different board type
#
ifdef bdtype

ifneq (liteonit_lvd2020,$(strip $(bdtype)))
ifneq (nagase_dvx_600,$(strip $(bdtype)))
ifneq (avlp2_dvdg,$(strip $(bdtype)))
ifneq (avlp2_dvdl,$(strip $(bdtype)))
ifneq (rev_a2,$(strip $(bdtype)))
ifneq (twd_rev_a2,$(strip $(bdtype)))
ifneq (fic_rev_a2,$(strip $(bdtype)))
$(error invalid bdtype `$(bdtype)')
endif
endif
endif
endif
endif
endif
endif

ifeq (liteonit_lvd2020,$(strip $(bdtype)))
override DFLAGS_MAIN += -DHAS_SCART -UHAS_VGA -DHAS_COMPONENT -UHAS_DVI -UHAS_PAL -DUSE_SEPARATE_INT -DLITEONIT_LVD2020
endif

ifeq (nagase_dvx_600,$(strip $(bdtype)))
override DFLAGS_MAIN += -DHAS_SCART -UHAS_VGA -DHAS_COMPONENT -UHAS_DVI -DHAS_PAL -DUSE_SEPARATE_INT -DLITEONIT_LVD2020
endif

ifeq (avlp2_dvdg,$(strip $(bdtype)))
override DFLAGS_MAIN += -UHAS_SCART -UHAS_VGA -UHAS_PAL -DHAS_COMPONENT -DHAS_DVI -DHAS_D4 -DUSE_SEPARATE_INT -DREV_A2
endif

ifeq (avlp2_dvdl,$(strip $(bdtype)))
override DFLAGS_MAIN += -UHAS_SCART -UHAS_VGA -UHAS_PAL -DHAS_COMPONENT -UHAS_DVI -DHAS_D4 -DUSE_SEPARATE_INT -DREV_A2
endif

ifeq (twd_rev_a2,$(strip $(bdtype)))
override DFLAGS_MAIN += -UHAS_SCART -UHAS_VGA -DHAS_PAL -DHAS_COMPONENT -UHAS_DVI -DUSE_SEPARATE_INT -DREV_A2
endif

ifeq (fic_rev_a2,$(strip $(bdtype)))
override DFLAGS_MAIN += -DHAS_SCART -UHAS_VGA -DHAS_PAL -DHAS_COMPONENT -DHAS_DVI -DUSE_SEPARATE_INT -DREV_A2
endif

ifeq (rev_a2,$(strip $(bdtype)))
override DFLAGS_MAIN += -UHAS_SCART -UHAS_VGA -DHAS_PAL -DHAS_COMPONENT -DHAS_DVI -DUSE_SEPARATE_INT -DREV_A2
endif

else # ndef bdtype

override DFLAGS_MAIN += -UHAS_SCART -UHAS_VGA -DHAS_PAL -DHAS_COMPONENT -DHAS_DVI
#-DHAS_SCART -DHAS_VGA -DHAS_COMPONENT -UHAS_DVI

endif


# [20030611] Peter, dvdplayer
# [20040218] CW Cheah, enable AUDIO property settings only for netDVD
ifeq (true,$(strip $(dvdplayer)))
fip=true
override DFLAGS_ADD += -DUSE_DVD -DAUDIO_PROPERTIES
endif

#/*[20040524] 10:35A neoh2, */
ifeq (true,$(strip $(multi_socket)))
override DFLAGS_ADD += -DMULTI_SOCKET_PROC
endif

# [20031028] Angch, usb compile option (UHCI or EHCI)
ifdef usb
ifneq (viavt6202,$(strip $(usb)))
$(error invalid usb option `$(usb)')
endif
endif

# [20041026] OngEK, minipci wifi
ifeq (true,$(strip $(broadcom_minipci)))
override DFLAGS_ADD += -DBROADCOM_MINIPCI
endif
ifeq (true,$(strip $(atheros_madhal)))
override DFLAGS_ADD += -DATHEROS_WIMAD
endif

ifeq (viavt6202,$(strip $(usb)))
override DFLAGS_ADD += -DSY_USB -DVIA_VT6202_UHCI_HC -DVIA_VT6202_EHCI_HC
endif

#/*[20031101] 12:03P neoh1, for pcmcia memory card*/
ifeq (true,$(strip $(pcmcia_mem_card)))
override DFLAGS_ADD += 	-DPCMCIA_CARD -DPC_MEM_CARD -DIO16ONLY2
endif

# [20040407] Roland Hii, use gb2312
ifeq (true,$(strip $(use_gb2312)))
override DFLAGS_ADD += -DUSE_GB2312
endif

# [20031028] SY Tee, use big5
ifeq (true,$(strip $(use_big5)))
override DFLAGS_ADD += -DUSE_FONT_BIG5
endif

# [20031211] CW Cheah, use korean
ifeq (true,$(strip $(use_korean)))
override DFLAGS_ADD += -DUSE_KOREAN
endif

# [20040107] CW Cheah, FIP compilation
ifeq (true,$(strip $(fip)))
override DFLAGS_ADD += -DUSE_FIP
endif

# [20040119] KennyKhoo, DVI custom setting
ifeq (true,$(strip $(dvi_custom_page)))
override DFLAGS_ADD += -DUSE_DVI_CUSTOM
endif

# [20040714] Roland Hii, use 64MB RAM
ifeq (true,$(strip $(use_64m)))
override DFLAGS_ADD += -DWITH_64M
endif

#
# Directories
#
BIOSDIR			= ./bios
BROWSERDIR		= ./browser
CGIDIR			= ./cgi
FILESYSDIR		= ./file_sys
FLASHDIR		= ./flash
GRAPHICSDIR		= ./graphics
FONTSDIR		= $(GRAPHICSDIR)/fonts
JPGDIR			= $(GRAPHICSDIR)/jpeg
LIBPNGDIR		= $(GRAPHICSDIR)/libpng
ZLIBDIR			= $(GRAPHICSDIR)/zlib
OSDIR			= ./os
SMALLDIR		= ./small
STDLIBDIR		= ./stdlib
SYABAS3DIR		= ./syabas3
AVITESTDIR		= ./avitest
VODDIR			= ./vod
LIBMADDIR		= $(VODDIR)/libmad_1_5
LIBTREMORDIR	= $(VODDIR)/libtremor
LIBWMADIR		= $(VODDIR)/libwma
LIBAACDIR		= $(VODDIR)/libaac
TRANSCODERDIR	= $(VODDIR)/libmp43
ASFDIR			= $(VODDIR)/asf_demux/src
UPNPDIR			= ./upnp
USBDIR          = ./usb
WIFIDIR			= ./wifi
YMDIR			= ./ym
DVDDIR			= ./dvdplayer
ATADIR			= ./atadrvr
CFDIR			= ./cf
CUSTOMERDIR		= ./customer
BROADCOMDIR		= ./wifi/broadcom
HIMATDIR		= ./himatplayer
RHAPSODYDIR		= ./browser/rhapsody

# SW Ang, 20041011
# Expat XML Parser directories
ifeq (true,$(strip $(xml_parser)))
XMLDIR			= ./xml
XMLLIBDIR		= $(XMLDIR)/lib
XMLWFDIR		= $(XMLDIR)/xmlwf
endif

FIPROOTDIR		= ./fip
FIPDIR			?= $(FIPROOTDIR)
RESOURCEROOTDIR	= $(CUSTOMERDIR)/resource
RESOURCEDIR		?= $(RESOURCEROOTDIR)/syabas
SETUPROOTDIR	= ./setup
SETUPDIR		?= $(SETUPROOTDIR)
DIVXDRMROOTDIR	= $(VODDIR)/divx_drm
DIVXDRMDIR		?= $(DIVXDRMROOTDIR)

# Sigma Designs hardware lib
ifeq (true,$(strip $(new_hwlib)))
HWLIB_DIR		= ./hwlib
else
HWLIB_DIR		= ./hwlib_20040818
override DFLAGS_ADD += -DOLD_HWLIB
endif

# Atheros MadWifi directories
MADHAL_DIR		= ./wifi/athelos/madhal
ATHELOS_DIR     = ./wifi/athelos

libmad = true

# TCPIP stack source directories
TCPIP_ROOT		= ./tcpip
TCPIP_INCLUDE	= $(TCPIP_ROOT)/include
TCPIP_INTERNET	= $(TCPIP_ROOT)/internet
TCPIP_NET		= $(TCPIP_ROOT)/net
TCPIP_MIPSX		= $(TCPIP_ROOT)/mipsx
TCPIP_SDESIGNS	= $(TCPIP_ROOT)/sdesigns
TCPIP_CLIENTS	= $(TCPIP_ROOT)/clients
TCPIP_DHCP		= $(TCPIP_CLIENTS)/dhcp

# [20020305] YC Yeoh, SSL Library
SSLDIR			= ./ssl
SSL_CRYPTO		= $(SSLDIR)/crypto

#Symmetric key algorithm
SSL_DES			= $(SSL_CRYPTO)/des
SSL_BF			= $(SSL_CRYPTO)/bf
SSL_CAST		= $(SSL_CRYPTO)/cast
SSL_IDEA		= $(SSL_CRYPTO)/idea
SSL_RC2			= $(SSL_CRYPTO)/rc2
SSL_RC4			= $(SSL_CRYPTO)/rc4
SSL_RC5			= $(SSL_CRYPTO)/rc5

#Hash functions
SSL_SHA			= $(SSL_CRYPTO)/sha
SSL_LHASH		= $(SSL_CRYPTO)/lhash
SSL_MD2			= $(SSL_CRYPTO)/md2
SSL_MD5			= $(SSL_CRYPTO)/md5
SSL_MDC2		= $(SSL_CRYPTO)/mdc2
SSL_RIPEMD		= $(SSL_CRYPTO)/ripemd
SSL_HMAC		= $(SSL_CRYPTO)/hmac

#Asymmetric key algorithm
SSL_RSA			= $(SSL_CRYPTO)/rsa
SSL_DSA			= $(SSL_CRYPTO)/dsa

#Others
SSL_ASN1		= $(SSL_CRYPTO)/asn1
SSL_BIO			= $(SSL_CRYPTO)/bio
SSL_BN			= $(SSL_CRYPTO)/bn
SSL_BUFFER		= $(SSL_CRYPTO)/buffer
SSL_CONF		= $(SSL_CRYPTO)/conf
SSL_DH			= $(SSL_CRYPTO)/dh
SSL_ERR			= $(SSL_CRYPTO)/err
SSL_EVP			= $(SSL_CRYPTO)/evp
SSL_OBJECTS		= $(SSL_CRYPTO)/objects
SSL_PEM			= $(SSL_CRYPTO)/pem
SSL_PKCS7		= $(SSL_CRYPTO)/pkcs7
SSL_RAND		= $(SSL_CRYPTO)/rand
SSL_STACK		= $(SSL_CRYPTO)/stack
SSL_TXT_DB		= $(SSL_CRYPTO)/txt_db
SSL_X509		= $(SSL_CRYPTO)/x509

#
# Source
#
LOADER0ASM = loader0.S

MAINASM = crt0.S #cloader.S
#MAINSRC = main.c uart.c sybdebug.c crc.c des_sc.c ucsconv.c

MAINSRC = main.c uart.c crc.c des_sc.c sybdebug0.c em86xxapi.c pcicommon.c pciem86xx.c ucsconv.c
#mamboapi.c pcimambo.c
#bootflash.c kunzip.c inflate.c qdt.c atapi.c specific.c bootdisc.c fip.c

LOADER1ASM = crt0.S
LOADER1SRC = loader1.c uart.c em86xxapi.c sybdebug0.c dummy_os.c LzmaDecode.c LzmaTest.c
#mamboapi.c kunzip.c inflate.c

DFMAINASM = crt0.S
DFMAINSRC = $(FLASHDIR)/dfmain.c uart.c sybdebug0.c em86xxapi.c dummy_os.c

# Sigma Designs sourcecode

HWLIB_DEPS = $(HWLIB_DIR)/irq_handler/src/irq_handler.bin \
			 $(HWLIB_DIR)/rua/emhwlib_user/src/librua.o \
			 $(HWLIB_DIR)/llad/direct/user_src/libllad.o \
			 $(HWLIB_DIR)/rmcore/src/rmcore.o \
			 $(HWLIB_DIR)/dcc/src/libdcc.o \
			 $(HWLIB_DIR)/rmvdemux/src/rmvdemuxlibrary.o \
			 $(HWLIB_DIR)/cipher/cipher.o

HWLIB_OBJS = $(filter %.o,$(HWLIB_DEPS))


##HWLIB_OBJS = $(HWLIB_DIR)/samples/colorbars.o \
##			$(HWLIB_DIR)/rua/emhwlib_user/src/librua.o \
##			$(HWLIB_DIR)/llad/direct/user_src/libllad.o

#			$(HWLIB_DIR)/samples/set_cursor.o \
#			$(HWLIB_DIR)/samples/move_cursor.o \

#HWLIB_OBJS		= $(HWLIB_DIR)/test/play_crc.o \
#				$(HWLIB_DIR)/test/dbgimplementation.o \
#				$(HWLIB_DIR)/test/rmmmimplementation.o \
#				$(HWLIB_DIR)/test/get_key.o \
#				$(HWLIB_DIR)/test/get_time.o \
#				$(HWLIB_DIR)/test/load_microcode.o \
#				$(HWLIB_DIR)/test/load_bitstream_chunk.o \
#				$(HWLIB_DIR)/rua/emhwlib_user/src/librua.o \
#				$(HWLIB_DIR)/llad/direct/user_src/libllad.o

#$(HWLIB_DIR)/test/colorbars.o $(HWLIB_DIR)/test/get_key.o \
#				$(HWLIB_DIR)/test/set_display.o $(HWLIB_DIR)/test/set_display_output.o \
#				$(HWLIB_DIR)/test/set_display_scaler.o $(HWLIB_DIR)/test/set_display_mixer.o \
#				$(HWLIB_DIR)/test/set_display_routing.o $(HWLIB_DIR)/test/set_display_input.o \
#				$(HWLIB_DIR)/test/rmmmimplementation.o $(HWLIB_DIR)/test/dbgimplementation.o \
#				$(HWLIB_DIR)/test/get_display_param.o \
#				$(HWLIB_DIR)/llad/direct/user_src/libllad.o \
#				$(HWLIB_DIR)/rua/emhwlib_user/src/librua.o \
#				$(HWLIB_DIR)/test/cursor.o $(HWLIB_DIR)/test/set_cursor.o
#
#				$(HWLIB_DIR)/emhwlib/src/emhwlib.o \
#				$(HWLIB_DIR)/rmmemorymanager/src/rmmemorymanager.o \

OSSRC	= $(OSDIR)/os_cpu_c.c $(OSDIR)/os_core.c $(OSDIR)/os_flag.c $(OSDIR)/os_mbox.c \
		  $(OSDIR)/os_mem.c $(OSDIR)/os_mutex.c $(OSDIR)/os_q.c $(OSDIR)/os_sem.c \
		  $(OSDIR)/os_task.c $(OSDIR)/os_time.c $(OSDIR)/os_alloc.c \
		  $(OSDIR)/os_view.c $(OSDIR)/os_view_c.c
#$(OSDIR)/ucos_ii.c

# Atheros MadWifi source
ifeq (true,$(strip $(atheros_madhal)))
ATHEROSSRC = $(ATHELOS_DIR)/if_ieee80211subr.c \
             $(ATHELOS_DIR)/rc4.c $(ATHELOS_DIR)/madwifi_pkt.c \
             $(ATHELOS_DIR)/if_media.c  $(ATHELOS_DIR)/if_ath.c \
             $(MADHAL_DIR)/ah_eeprom.c $(MADHAL_DIR)/ah_osdep.c \
             $(MADHAL_DIR)/ah.c $(MADHAL_DIR)/ah_regdomain.c \
             $(MADHAL_DIR)/ar5212/ar5212_attach.c \
             $(MADHAL_DIR)/ar5212/ar5212_phy.c \
             $(MADHAL_DIR)/ar5212/ar5212_beacon.c \
             $(MADHAL_DIR)/ar5212/ar5212_interrupts.c \
             $(MADHAL_DIR)/ar5212/ar5212_keycache.c \
             $(MADHAL_DIR)/ar5212/ar5212_misc.c \
             $(MADHAL_DIR)/ar5212/ar5212_power.c \
             $(MADHAL_DIR)/ar5212/ar5212_recv.c \
             $(MADHAL_DIR)/ar5212/ar5212_reset.c \
             $(MADHAL_DIR)/ar5212/ar5212_xmit.c \
             $(MADHAL_DIR)/ar5212/ar5212_ani.c \
             $(MADHAL_DIR)/ar5211/ar5211_attach.c \
             $(MADHAL_DIR)/ar5211/ar5211_phy.c \
             $(MADHAL_DIR)/ar5211/ar5211_beacon.c \
             $(MADHAL_DIR)/ar5211/ar5211_interrupts.c \
             $(MADHAL_DIR)/ar5211/ar5211_keycache.c \
             $(MADHAL_DIR)/ar5211/ar5211_misc.c \
             $(MADHAL_DIR)/ar5211/ar5211_power.c \
             $(MADHAL_DIR)/ar5211/ar5211_recv.c \
             $(MADHAL_DIR)/ar5211/ar5211_reset.c \
             $(MADHAL_DIR)/ar5211/ar5211_xmit.c \
             $(MADHAL_DIR)/ar5210/ar5210_attach.c \
             $(MADHAL_DIR)/ar5210/ar5210_phy.c \
             $(MADHAL_DIR)/ar5210/ar5210_beacon.c \
             $(MADHAL_DIR)/ar5210/ar5210_interrupts.c \
             $(MADHAL_DIR)/ar5210/ar5210_keycache.c \
             $(MADHAL_DIR)/ar5210/ar5210_misc.c \
             $(MADHAL_DIR)/ar5210/ar5210_power.c \
             $(MADHAL_DIR)/ar5210/ar5210_recv.c \
             $(MADHAL_DIR)/ar5210/ar5210_reset.c \
             $(MADHAL_DIR)/ar5210/ar5210_xmit.c
endif

SYABAS3SRC	= $(SYABAS3DIR)/syabas3.c

SMALLSRC = $(SMALLDIR)/small.c

GRAPHICSSRC = $(GRAPHICSDIR)/color.c $(GRAPHICSDIR)/gif.c $(GRAPHICSDIR)/graphic.c \
			  $(GRAPHICSDIR)/mypal.c $(GRAPHICSDIR)/port_api.c $(GRAPHICSDIR)/readbmp.c \
			  $(GRAPHICSDIR)/osd.c $(GRAPHICSDIR)/bmp2osd.c

SMALLGUISRC = $(GRAPHICSDIR)/graphic.c $(GRAPHICSDIR)/mypal.c $(GRAPHICSDIR)/port_api.c \
			  $(VODDIR)/vod_api.c

JPGSRC = $(JPGDIR)/jcomapi.c $(JPGDIR)/jdapimin.c $(JPGDIR)/jdapistd.c \
		 $(JPGDIR)/jdatasrc.c $(JPGDIR)/jdcoefct.c $(JPGDIR)/jdcolor.c \
		 $(JPGDIR)/jddctmgr.c $(JPGDIR)/jdhuff.c $(JPGDIR)/jdinput.c \
		 $(JPGDIR)/jdmainct.c $(JPGDIR)/jdmarker.c $(JPGDIR)/jdmaster.c \
		 $(JPGDIR)/jdmerge.c $(JPGDIR)/jdphuff.c $(JPGDIR)/jdpostct.c \
		 $(JPGDIR)/jdsample.c $(JPGDIR)/jdtrans.c $(JPGDIR)/jerror.c \
		 $(JPGDIR)/jidctflt.c $(JPGDIR)/jidctfst.c $(JPGDIR)/jidctint.c \
		 $(JPGDIR)/jidctred.c $(JPGDIR)/jmemmgr.c $(JPGDIR)/jmemnobs.c \
		 $(JPGDIR)/jpeg_lib.c $(JPGDIR)/jquant1.c $(JPGDIR)/jquant2.c \
		 $(JPGDIR)/jutils.c

LIBPNGSRC = $(LIBPNGDIR)/pngport.c

BIOSSRC = $(BIOSDIR)/bioskey.c $(BIOSDIR)/ir.c


ifeq (viavt6202,$(strip $(usb)))
# USBSRC = $(USBDIR)/sy_sl811hst.c $(USBDIR)/sy_usb.c $(USBDIR)/sy_umass.c
USBSRC = $(USBDIR)/sy_uhci.c $(USBDIR)/sy_usb.c $(USBDIR)/sy_umass.c \
		 $(USBDIR)/sy_ehci-hcd.c
endif

# Peter, NOS source
NOSSRC = $(TCPIP_NET)/kernel.c \
		 $(TCPIP_NET)/ksubr.c \
		 $(TCPIP_NET)/timer.c \
		 $(TCPIP_NET)/mbuf.c \
		 $(TCPIP_NET)/misc.c \
		 $(TCPIP_NET)/skbuff.c \
		 $(TCPIP_ROOT)/nosport.c

# Peter, TCPIP stack source
TCPIPSRC =	$(TCPIP_NET)/iface.c \
			$(TCPIP_INTERNET)/netuser.c \
			$(TCPIP_INTERNET)/icmp.c \
			$(TCPIP_INTERNET)/icmphdr.c \
			$(TCPIP_INTERNET)/iproute.c \
			$(TCPIP_INTERNET)/ip.c \
			$(TCPIP_INTERNET)/iphdr.c \
			$(TCPIP_INTERNET)/tcphdr.c \
			$(TCPIP_INTERNET)/tcpin.c \
			$(TCPIP_INTERNET)/tcpout.c \
			$(TCPIP_INTERNET)/tcpsubr.c \
			$(TCPIP_INTERNET)/tcptimer.c \
			$(TCPIP_INTERNET)/udp.c \
			$(TCPIP_INTERNET)/udphdr.c \
			$(TCPIP_NET)/socket.c \
			$(TCPIP_NET)/sockuser.c \
			$(TCPIP_NET)/sockutil.c \
			$(TCPIP_INTERNET)/tcpsock.c \
			$(TCPIP_INTERNET)/udpsock.c \
			$(TCPIP_INTERNET)/ipsock.c \
			$(TCPIP_INTERNET)/tcpuser.c \
			$(TCPIP_INTERNET)/arp.c \
			$(TCPIP_INTERNET)/arphdr.c \
			$(TCPIP_INTERNET)/domain.c \
			$(TCPIP_INTERNET)/domhdr.c \
			$(TCPIP_SDESIGNS)/enet.c \
			$(TCPIP_SDESIGNS)/rtl8139_pkt.c \
			$(TCPIP_SDESIGNS)/netdevice.c \
			$(TCPIP_CLIENTS)/sntp.c \
			$(TCPIP_CLIENTS)/dhcp.c \
			$(TCPIP_CLIENTS)/multi_socket.c


#			$(TCPIP_SDESIGNS)/dm9000x_pkt.c \
#			$(TCPIP_SDESIGNS)/ne_pkt.c \
#			$(TCPIP_CLIENTS)/supdate.c \
#			$(TCPIP_CLIENTS)/http.c \

SMALLTCPIPSRC = $(TCPIPSRC) $(TCPIP_CLIENTS)/supdate.c

FILESYSSRC = $(FILESYSDIR)/fnmerge.c $(FILESYSDIR)/fnsplit.c \
			 $(FILESYSDIR)/file_sys.c $(FILESYSDIR)/fileio.c

FLASHSRC = $(FLASHDIR)/flash.c $(FLASHDIR)/flashrw.c
# $(FLASHDIR)/jaspermap.c

BROWSERSRC = $(BROWSERDIR)/atoms.c \
			 $(BROWSERDIR)/b2g.c $(BROWSERDIR)/b2gtbl.c $(BROWSERDIR)/base64.c \
			 $(BROWSERDIR)/browser.c \
			 $(BROWSERDIR)/cgiquery.c $(BROWSERDIR)/config.c \
			 $(BROWSERDIR)/djtmr.c $(BROWSERDIR)/drawtime.c \
			 $(BROWSERDIR)/entity.c $(BROWSERDIR)/errors.c \
			 $(BROWSERDIR)/fexists.c $(BROWSERDIR)/ftrace.c \
			 $(BROWSERDIR)/globals.c $(BROWSERDIR)/gui.c $(BROWSERDIR)/guidraw.c \
			 $(BROWSERDIR)/guievent.c $(BROWSERDIR)/guitick.c \
			 $(BROWSERDIR)/highligh.c $(BROWSERDIR)/html.c $(BROWSERDIR)/htmlcsim.c \
			 $(BROWSERDIR)/htmldraw.c $(BROWSERDIR)/htmlread.c $(BROWSERDIR)/htmlstat.c \
			 $(BROWSERDIR)/htmlutil.c $(BROWSERDIR)/htmtable.c \
			 $(BROWSERDIR)/ie_bin.c $(BROWSERDIR)/ie_fce.c $(BROWSERDIR)/ie_key.c \
			 $(BROWSERDIR)/iekernel.c $(BROWSERDIR)/image.c $(BROWSERDIR)/inetstr.c \
			 $(BROWSERDIR)/init.c \
			 $(BROWSERDIR)/mime.c $(BROWSERDIR)/misc.c \
			 $(BROWSERDIR)/onmouse.c $(BROWSERDIR)/outs.c \
			 $(BROWSERDIR)/parsecss.c $(BROWSERDIR)/port_htk.c $(BROWSERDIR)/porthttp.c \
			 $(BROWSERDIR)/preparse.c $(BROWSERDIR)/protocol.c \
			 $(BROWSERDIR)/scrolbar.c $(BROWSERDIR)/search.c $(BROWSERDIR)/str.c \
			 $(BROWSERDIR)/urlovrl.c $(BROWSERDIR)/urlstat.c
#$(BROWSERDIR)/svgastat.c

AVITESTSRC = $(AVITESTDIR)/play_avi.c $(AVITESTDIR)/rmproperties/src/rmproperties.c \
			 $(AVITESTDIR)/rmlibcw/src_linux/rmfile.c

AVITESTCPPSRC = $(AVITESTDIR)/rmavi/src/rmavi.cpp $(AVITESTDIR)/rmavi/src/rmavitrack.cpp \
				$(AVITESTDIR)/rmaviapi/externalapi/rmaviapi.cpp \
				$(AVITESTDIR)/rmavicore/src/rmavicore.cpp $(AVITESTDIR)/rmavicore/src/rmavistream.cpp \
				$(AVITESTDIR)/rmmpeg4_framework/src/rmmpeg4client.cpp \
				$(AVITESTDIR)/rmmpeg4_framework/src/rmmpeg4track.cpp \
				$(AVITESTDIR)/rmmpeg4_framework/src/rmmpeg4decoder.cpp \
				$(AVITESTDIR)/rmmpeg4_framework/src/rmmpeg4audiodecoder.cpp \
				$(AVITESTDIR)/rmcpputils/src/rmbufferstream.cpp \
				$(AVITESTDIR)/rmcpputils/src/rmreadwritebufferstream.cpp \
				$(AVITESTDIR)/rmcpputils/src/rmobject.cpp \
				$(AVITESTDIR)/rmcpputils/src/rmfilestream.cpp

VODSRC = $(VODDIR)/vod_main.c $(VODDIR)/vod_fifo.c $(VODDIR)/vod_api.c \
		 $(VODDIR)/demuxer.c $(VODDIR)/mp3.c $(VODDIR)/mpa.c $(VODDIR)/mpg.c $(VODDIR)/data.c \
		 $(VODDIR)/avi.c $(VODDIR)/avicheck.c $(VODDIR)/ogg.c $(VODDIR)/wav.c $(VODDIR)/mov.c \
		 $(VODDIR)/wma.c $(VODDIR)/aac.c $(VODDIR)/ac3.c $(VODDIR)/jpg.c $(VODDIR)/subtitle.c \
		 $(VODDIR)/vod_gui.c $(VODDIR)/vol_core.c $(VODDIR)/id3v2.c \
		 $(VODDIR)/em86xx.c $(VODDIR)/jpg_helper.c $(VODDIR)/buffer.c \
		 $(VODDIR)/rua_required.c  $(VODDIR)/asf.c \
		 $(VODDIR)/lpcm.c $(VODDIR)/hwdemux.c

ASFSRC = $(ASFDIR)/GUID.c $(ASFDIR)/asf_common.c $(ASFDIR)/asf_header.c $(ASFDIR)/asf_data.c \
		 $(ASFDIR)/asf_index.c $(ASFDIR)/asf_file.c

STDLIBSRC = $(STDLIBDIR)/string_.c $(STDLIBDIR)/time_.c

SETUPSRC = $(SETUPDIR)/lansetup.c $(SETUPDIR)/preferen.c $(SETUPDIR)/ntpsetup.c \
		   $(SETUPDIR)/dvdsetup.c $(SETUPDIR)/macaddr.c $(SETUPDIR)/languages_pack.c \
		   $(SETUPDIR)/init_option.c $(SETUPDIR)/wifisetup.c
#		   $(SETUPDIR)/dvisetup.c
ifeq (true,$(strip $(aoss)))
SETUPSRC += $(SETUPDIR)/aoss_api.c
endif

SMALLSETUPSRC = $(SETUPDIR)/lansetup.c

CGISRC = $(CGIDIR)/cgi.c

YMSRC = $(YMDIR)/gslist.c $(YMDIR)/memtok.c $(YMDIR)/sm_gui.c  \
		$(YMDIR)/ym_crypt.c $(YMDIR)/ym_idle.c $(YMDIR)/ym_pkt.c $(YMDIR)/ymrender.c \
		$(YMDIR)/ym_utils.c $(YMDIR)/ym.c $(YMDIR)/libyahoo.c

ATASRC = $(ATADIR)/ataioreg.c $(ATADIR)/ataiosub.c $(ATADIR)/ataiopio.c \
		 $(ATADIR)/ataiotrc.c $(ATADIR)/ataioint.c $(ATADIR)/ataiotmr.c \
		 $(ATADIR)/file_player.c $(ATADIR)/ataiopci.c $(ATADIR)/atapi.c \
		 $(ATADIR)/ide.c $(ATADIR)/pc_mem_card.c $(ATADIR)/atapi_mem_reader.c \
		 $(ATADIR)/sy_usb.c $(ATADIR)/use_dvd.c $(ATADIR)/fat.c

SSLSRC = $(SSL_CRYPTO_MD5_SRC) $(SSL_CRYPTO_DES_SRC) $(SSL_CRYPTO_BF_SRC) \
		 $(SSL_CRYPTO_CAST_SRC) $(SSL_CRYPTO_IDEA_SRC) $(SSL_CRYPTO_RC2_SRC) \
		 $(SSL_CRYPTO_RC4_SRC) $(SSL_CRYPTO_RC5_SRC) $(SSL_CRYPTO_SHA_SRC) \
		 $(SSL_CRYPTO_LHASH_SRC) $(SSL_CRYPTO_MD2_SRC) $(SSL_CRYPTO_MDC2_SRC) \
		 $(SSL_CRYPTO_RIPEMD_SRC) $(SSL_CRYPTO_HMAC_SRC) $(SSL_CRYPTO_ASN1_SRC) \
		 $(SSL_CRYPTO_BIO_SRC) $(SSL_CRYPTO_BN_SRC) $(SSL_CRYPTO_BUFFER_SRC) \
		 $(SSL_CRYPTO_CONF_SRC) $(SSL_CRYPTO_DH_SRC) $(SSL_CRYPTO_DSA_SRC) \
		 $(SSL_CRYPTO_ERR_SRC) $(SSL_CRYPTO_EVP_SRC) $(SSL_CRYPTO_OBJECTS_SRC) \
		 $(SSL_CRYPTO_PEM_SRC) $(SSL_CRYPTO_PKCS7_SRC) $(SSL_CRYPTO_RAND_SRC) \
		 $(SSL_CRYPTO_RSA_SRC) $(SSL_CRYPTO_STACK_SRC) $(SSL_CRYPTO_TXT_DB_SRC) \
		 $(SSL_CRYPTO_X509_SRC) $(SSL_CRYPTO_SRC) $(SSL_SRC)

SSL_CRYPTO_MD5_SRC = $(SSL_MD5)/md5_dgst.c $(SSL_MD5)/md5_one.c
SSL_CRYPTO_DES_SRC = $(SSL_DES)/cbc_cksm.c $(SSL_DES)/cbc_enc.c  \
					 $(SSL_DES)/cfb64enc.c $(SSL_DES)/cfb_enc.c \
					 $(SSL_DES)/ecb3_enc.c $(SSL_DES)/ecb_enc.c \
					 $(SSL_DES)/fcrypt.c $(SSL_DES)/ofb64enc.c \
					 $(SSL_DES)/ofb_enc.c $(SSL_DES)/pcbc_enc.c \
					 $(SSL_DES)/qud_cksm.c $(SSL_DES)/rand_key.c \
					 $(SSL_DES)/set_key.c $(SSL_DES)/des_enc.c \
					 $(SSL_DES)/fcrypt_b.c $(SSL_DES)/rpc_enc.c \
					 $(SSL_DES)/xcbc_enc.c $(SSL_DES)/str2key.c \
					 $(SSL_DES)/cfb64ede.c $(SSL_DES)/ofb64ede.c
SSL_CRYPTO_BF_SRC =  $(SSL_BF)/bf_skey.c $(SSL_BF)/bf_ecb.c \
					 $(SSL_BF)/bf_enc.c $(SSL_BF)/bf_cfb64.c $(SSL_BF)/bf_ofb64.c
SSL_CRYPTO_CAST_SRC = $(SSL_CAST)/c_skey.c $(SSL_CAST)/c_ecb.c \
					  $(SSL_CAST)/c_enc.c $(SSL_CAST)/c_cfb64.c $(SSL_CAST)/c_ofb64.c
SSL_CRYPTO_IDEA_SRC = $(SSL_IDEA)/i_cbc.c $(SSL_IDEA)/i_cfb64.c \
					  $(SSL_IDEA)/i_ofb64.c $(SSL_IDEA)/i_ecb.c $(SSL_IDEA)/i_skey.c
SSL_CRYPTO_RC2_SRC =  $(SSL_RC2)/rc2_ecb.c $(SSL_RC2)/rc2_skey.c \
					  $(SSL_RC2)/rc2_cbc.c $(SSL_RC2)/rc2cfb64.c $(SSL_RC2)/rc2ofb64.c
SSL_CRYPTO_RC4_SRC =	$(SSL_RC4)/rc4_skey.c $(SSL_RC4)/rc4_enc.c
SSL_CRYPTO_RC5_SRC = $(SSL_RC5)/rc5_skey.c $(SSL_RC5)/rc5_ecb.c \
					 $(SSL_RC5)/rc5_enc.c $(SSL_RC5)/rc5cfb64.c $(SSL_RC5)/rc5ofb64.c
SSL_CRYPTO_SHA_SRC = $(SSL_SHA)/sha_dgst.c $(SSL_SHA)/sha1dgst.c \
					 $(SSL_SHA)/sha_one.c $(SSL_SHA)/sha1_one.c
SSL_CRYPTO_LHASH_SRC =	$(SSL_LHASH)/lhash.c $(SSL_LHASH)/lh_stats.c
SSL_CRYPTO_MD2_SRC =	$(SSL_MD2)/md2_dgst.c $(SSL_MD2)/md2_one.c
SSL_CRYPTO_MDC2_SRC =	$(SSL_MDC2)/mdc2dgst.c $(SSL_MDC2)/mdc2_one.c
SSL_CRYPTO_RIPEMD_SRC =	$(SSL_RIPEMD)/rmd_dgst.c $(SSL_RIPEMD)/rmd_one.c
SSL_CRYPTO_HMAC_SRC =	$(SSL_HMAC)/hmac.c
SSL_CRYPTO_ASN1_SRC =	$(SSL_ASN1)/a_object.c $(SSL_ASN1)/a_bitstr.c \
						$(SSL_ASN1)/a_utctm.c $(SSL_ASN1)/a_int.c $(SSL_ASN1)/a_octet.c \
						$(SSL_ASN1)/a_print.c $(SSL_ASN1)/a_type.c \
						$(SSL_ASN1)/a_set.c $(SSL_ASN1)/a_dup.c $(SSL_ASN1)/a_d2i_fp.c \
						$(SSL_ASN1)/a_i2d_fp.c $(SSL_ASN1)/a_sign.c $(SSL_ASN1)/a_digest.c \
						$(SSL_ASN1)/a_verify.c $(SSL_ASN1)/x_algor.c $(SSL_ASN1)/x_val.c \
						$(SSL_ASN1)/x_pubkey.c $(SSL_ASN1)/x_sig.c $(SSL_ASN1)/x_req.c \
						$(SSL_ASN1)/x_attrib.c $(SSL_ASN1)/x_name.c $(SSL_ASN1)/x_cinf.c \
						$(SSL_ASN1)/x_x509.c $(SSL_ASN1)/x_crl.c $(SSL_ASN1)/x_info.c \
						$(SSL_ASN1)/x_spki.c $(SSL_ASN1)/d2i_r_pr.c $(SSL_ASN1)/i2d_r_pr.c \
						$(SSL_ASN1)/d2i_r_pu.c $(SSL_ASN1)/i2d_r_pu.c $(SSL_ASN1)/d2i_s_pr.c \
						$(SSL_ASN1)/i2d_s_pr.c $(SSL_ASN1)/d2i_s_pu.c $(SSL_ASN1)/i2d_s_pu.c \
						$(SSL_ASN1)/d2i_pu.c $(SSL_ASN1)/d2i_pr.c $(SSL_ASN1)/i2d_pu.c \
						$(SSL_ASN1)/i2d_pr.c $(SSL_ASN1)/t_req.c $(SSL_ASN1)/t_x509.c \
						$(SSL_ASN1)/t_pkey.c $(SSL_ASN1)/p7_i_s.c $(SSL_ASN1)/p7_signi.c \
						$(SSL_ASN1)/p7_signd.c $(SSL_ASN1)/p7_recip.c $(SSL_ASN1)/p7_enc_c.c \
						$(SSL_ASN1)/p7_evp.c $(SSL_ASN1)/p7_dgst.c $(SSL_ASN1)/p7_s_e.c \
						$(SSL_ASN1)/p7_enc.c $(SSL_ASN1)/p7_lib.c $(SSL_ASN1)/f_int.c \
						$(SSL_ASN1)/f_string.c $(SSL_ASN1)/i2d_dhp.c $(SSL_ASN1)/i2d_dsap.c \
						$(SSL_ASN1)/d2i_dhp.c $(SSL_ASN1)/d2i_dsap.c $(SSL_ASN1)/n_pkey.c \
						$(SSL_ASN1)/a_hdr.c $(SSL_ASN1)/x_pkey.c $(SSL_ASN1)/a_bool.c \
						$(SSL_ASN1)/x_exten.c $(SSL_ASN1)/asn1_par.c $(SSL_ASN1)/asn1_lib.c \
						$(SSL_ASN1)/asn1_err.c $(SSL_ASN1)/a_meth.c $(SSL_ASN1)/a_bytes.c \
						$(SSL_ASN1)/evp_asn1.c
SSL_CRYPTO_BIO_SRC =	$(SSL_BIO)/bio_lib.c $(SSL_BIO)/bio_cb.c \
						$(SSL_BIO)/bio_err.c $(SSL_BIO)/bss_mem.c $(SSL_BIO)/bss_null.c \
						$(SSL_BIO)/bss_fd.c $(SSL_BIO)/bss_file.c $(SSL_BIO)/bss_sock.c \
						$(SSL_BIO)/bss_conn.c $(SSL_BIO)/bf_null.c $(SSL_BIO)/bf_buff.c \
						$(SSL_BIO)/b_print.c $(SSL_BIO)/b_dump.c $(SSL_BIO)/b_sock.c \
						$(SSL_BIO)/bss_acpt.c $(SSL_BIO)/bf_nbio.c
SSL_CRYPTO_BN_SRC =		$(SSL_BN)/bn_add.c $(SSL_BN)/bn_div.c \
						$(SSL_BN)/bn_exp.c $(SSL_BN)/bn_lib.c $(SSL_BN)/bn_mod.c \
						$(SSL_BN)/bn_mul.c $(SSL_BN)/bn_print.c $(SSL_BN)/bn_rand.c \
						$(SSL_BN)/bn_shift.c $(SSL_BN)/bn_sub.c $(SSL_BN)/bn_word.c \
						$(SSL_BN)/bn_blind.c $(SSL_BN)/bn_gcd.c $(SSL_BN)/bn_prime.c \
						$(SSL_BN)/bn_err.c $(SSL_BN)/bn_sqr.c $(SSL_BN)/bn_mulw.c \
						$(SSL_BN)/bn_recp.c $(SSL_BN)/bn_mont.c $(SSL_BN)/bn_mpi.c
SSL_CRYPTO_BUFFER_SRC =	$(SSL_BUFFER)/ssl_buf.c $(SSL_BUFFER)/buf_err.c
SSL_CRYPTO_CONF_SRC =	$(SSL_CONF)/conf.c $(SSL_CONF)/conf_err.c
SSL_CRYPTO_DH_SRC =	$(SSL_DH)/dh_gen.c $(SSL_DH)/dh_key.c \
					$(SSL_DH)/dh_lib.c $(SSL_DH)/dh_check.c $(SSL_DH)/dh_err.c
SSL_CRYPTO_DSA_SRC =	$(SSL_DSA)/dsa_gen.c $(SSL_DSA)/dsa_key.c \
						$(SSL_DSA)/dsa_lib.c $(SSL_DSA)/dsa_vrf.c \
						$(SSL_DSA)/dsa_sign.c $(SSL_DSA)/dsa_err.c
SSL_CRYPTO_ERR_SRC =	$(SSL_ERR)/err.c $(SSL_ERR)/err_all.c $(SSL_ERR)/err_prn.c
SSL_CRYPTO_EVP_SRC =	$(SSL_EVP)/encode.c $(SSL_EVP)/digest.c \
						$(SSL_EVP)/evp_enc.c $(SSL_EVP)/evp_key.c $(SSL_EVP)/e_ecb_d.c \
						$(SSL_EVP)/e_cbc_d.c $(SSL_EVP)/e_cfb_d.c $(SSL_EVP)/e_ofb_d.c \
						$(SSL_EVP)/e_ecb_i.c $(SSL_EVP)/e_cbc_i.c $(SSL_EVP)/e_cfb_i.c \
						$(SSL_EVP)/e_ofb_i.c $(SSL_EVP)/e_ecb_3d.c $(SSL_EVP)/e_cbc_3d.c \
						$(SSL_EVP)/e_rc4.c $(SSL_EVP)/names.c $(SSL_EVP)/e_cfb_3d.c \
						$(SSL_EVP)/e_ofb_3d.c $(SSL_EVP)/e_xcbc_d.c $(SSL_EVP)/e_ecb_r2.c \
						$(SSL_EVP)/e_cbc_r2.c $(SSL_EVP)/e_cfb_r2.c $(SSL_EVP)/e_ofb_r2.c \
						$(SSL_EVP)/e_ecb_bf.c $(SSL_EVP)/e_cbc_bf.c $(SSL_EVP)/e_cfb_bf.c \
						$(SSL_EVP)/e_ofb_bf.c $(SSL_EVP)/e_ecb_c.c $(SSL_EVP)/e_cbc_c.c \
						$(SSL_EVP)/e_cfb_c.c $(SSL_EVP)/e_ofb_c.c $(SSL_EVP)/e_ecb_r5.c \
						$(SSL_EVP)/e_cbc_r5.c $(SSL_EVP)/e_cfb_r5.c $(SSL_EVP)/e_ofb_r5.c \
						$(SSL_EVP)/m_null.c $(SSL_EVP)/m_md2.c $(SSL_EVP)/m_md5.c \
						$(SSL_EVP)/m_sha.c $(SSL_EVP)/m_sha1.c $(SSL_EVP)/m_dss.c \
						$(SSL_EVP)/m_dss1.c $(SSL_EVP)/m_mdc2.c $(SSL_EVP)/m_ripemd.c \
						$(SSL_EVP)/p_open.c $(SSL_EVP)/p_seal.c $(SSL_EVP)/p_sign.c \
						$(SSL_EVP)/p_verify.c $(SSL_EVP)/p_lib.c $(SSL_EVP)/p_enc.c \
						$(SSL_EVP)/p_dec.c $(SSL_EVP)/bio_md.c $(SSL_EVP)/bio_b64.c \
						$(SSL_EVP)/bio_enc.c $(SSL_EVP)/evp_err.c $(SSL_EVP)/e_null.c \
						$(SSL_EVP)/c_all.c $(SSL_EVP)/evp_lib.c
SSL_CRYPTO_OBJECTS_SRC =	$(SSL_OBJECTS)/obj_dat.c $(SSL_OBJECTS)/obj_lib.c $(SSL_OBJECTS)/obj_err.c
SSL_CRYPTO_PEM_SRC =	$(SSL_PEM)/pem_sign.c $(SSL_PEM)/pem_seal.c \
						$(SSL_PEM)/pem_info.c $(SSL_PEM)/pem_lib.c \
						$(SSL_PEM)/pem_all.c $(SSL_PEM)/pem_err.c
SSL_CRYPTO_PKCS7_SRC =	$(SSL_PKCS7)/pk7_lib.c $(SSL_PKCS7)/pkcs7err.c $(SSL_PKCS7)/pk7_doit.c
SSL_CRYPTO_RAND_SRC =	$(SSL_RAND)/md_rand.c $(SSL_RAND)/randfile.c
SSL_CRYPTO_RSA_SRC =	$(SSL_RSA)/rsa_eay.c $(SSL_RSA)/rsa_gen.c \
						$(SSL_RSA)/rsa_lib.c $(SSL_RSA)/rsa_sign.c $(SSL_RSA)/rsa_saos.c \
						$(SSL_RSA)/rsa_err.c $(SSL_RSA)/rsa_pk1.c $(SSL_RSA)/rsa_ssl.c \
						$(SSL_RSA)/rsa_none.c
SSL_CRYPTO_STACK_SRC =	$(SSL_STACK)/stack.c
SSL_CRYPTO_TXT_DB_SRC =	$(SSL_TXT_DB)/txt_db.c
SSL_CRYPTO_X509_SRC =	$(SSL_X509)/x509_def.c $(SSL_X509)/x509_d2.c \
						$(SSL_X509)/x509_r2x.c $(SSL_X509)/x509_cmp.c $(SSL_X509)/x509_obj.c \
						$(SSL_X509)/x509_req.c $(SSL_X509)/x509_vfy.c $(SSL_X509)/x509_set.c \
						$(SSL_X509)/x509rset.c $(SSL_X509)/x509_err.c $(SSL_X509)/x509name.c \
						$(SSL_X509)/x509_v3.c $(SSL_X509)/x509_ext.c $(SSL_X509)/x509pack.c \
						$(SSL_X509)/x509type.c $(SSL_X509)/x509_lu.c $(SSL_X509)/x_all.c \
						$(SSL_X509)/x509_txt.c $(SSL_X509)/by_file.c $(SSL_X509)/by_dir.c \
						$(SSL_X509)/v3_net.c $(SSL_X509)/v3_x509.c

SSL_CRYPTO_SRC =	$(SSL_CRYPTO)/cryptlib.c \
					$(SSL_CRYPTO)/cversion.c $(SSL_CRYPTO)/ex_data.c \
					$(SSL_CRYPTO)/cpt_err.c $(SSL_CRYPTO)/ssl_port.c
# $(SSL_CRYPTO)/mem.c

SSL_SRC =	$(SSLDIR)/s2_meth.c $(SSLDIR)/s2_srvr.c $(SSLDIR)/s2_clnt.c \
			$(SSLDIR)/s2_lib.c  $(SSLDIR)/s2_enc.c $(SSLDIR)/s2_pkt.c \
			$(SSLDIR)/s3_meth.c $(SSLDIR)/s3_srvr.c $(SSLDIR)/s3_clnt.c \
			$(SSLDIR)/s3_lib.c  $(SSLDIR)/s3_enc.c $(SSLDIR)/s3_pkt.c \
			$(SSLDIR)/s3_both.c $(SSLDIR)/s23_meth.c $(SSLDIR)/s23_srvr.c \
			$(SSLDIR)/s23_clnt.c $(SSLDIR)/s23_lib.c $(SSLDIR)/s23_pkt.c \
			$(SSLDIR)/t1_meth.c $(SSLDIR)/t1_srvr.c $(SSLDIR)/t1_clnt.c \
			$(SSLDIR)/t1_lib.c  $(SSLDIR)/t1_enc.c $(SSLDIR)/ssl_lib.c \
			$(SSLDIR)/ssl_err2.c $(SSLDIR)/ssl_cert.c $(SSLDIR)/ssl_sess.c \
			$(SSLDIR)/ssl_ciph.c $(SSLDIR)/ssl_stat.c $(SSLDIR)/ssl_rsa.c \
			$(SSLDIR)/ssl_asn1.c $(SSLDIR)/ssl_txt.c $(SSLDIR)/ssl_algs.c \
			$(SSLDIR)/bio_ssl.c $(SSLDIR)/ssl_err.c

WIFISRC = $(WIFIDIR)/hermes.c $(WIFIDIR)/wifi.c $(WIFIDIR)/pcmcia.c

UPNPSRC = $(UPNPDIR)/upnp.c $(UPNPDIR)/ihome.c $(UPNPDIR)/rendezvous.c

ifdef upnpav

ifneq (localcp,$(strip $(upnpav)))
ifneq (remoteui,$(strip $(upnpav)))
$(error invalid option for upnpav `$(upnpav)')
endif
endif

# [20040924] Roland Hii, added another directory for local CP temporary, until we merge in the renderer code into new UPnP AV code.
#UPNPAVDIR = $(UPNPDIR)/av
UPNPAV_DBDIR = $(UPNPAVDIR)/_DeviceBuilder
UPNPAV_DMPSDIR = $(UPNPAVDIR)/_DigitalMediaPlayerStack
UPNPAV_MRCPSDIR = $(UPNPAVDIR)/_MediaRendererControlPointStack
UPNPAV_MRDSDIR = $(UPNPAVDIR)/_MediaRendererDeviceStack
UPNPAV_MSCPSDIR = $(UPNPAVDIR)/_MediaServerControlPointStack
UPNPAV_MSDSDIR = $(UPNPAVDIR)/_MediaServerDeviceStack
UPNPAV_RXCSDIR = $(UPNPAVDIR)/_RioXrtClientStack
UPNPAV_UTILITYDIR = $(UPNPAVDIR)/_Utility

WMDRM_NDDIR = ./wmdrm_nd
WMDRM_ND_OEMDIR = $(WMDRM_NDDIR)/oem

WMDRM_NDSRC = $(WMDRM_NDDIR)/DrmAesEx.c $(WMDRM_NDDIR)/DrmByteManip.c $(WMDRM_NDDIR)/DrmCrt.c \
			  $(WMDRM_NDDIR)/DrmDebug.c $(WMDRM_NDDIR)/DrmInt64.c $(WMDRM_NDDIR)/DrmRsaEx.c \
			  $(WMDRM_NDDIR)/DrmSha1.c $(WMDRM_NDDIR)/DrmXmr.c $(WMDRM_NDDIR)/Receiver.c \
			  $(WMDRM_NDDIR)/WmdrmNet.c $(WMDRM_NDDIR)/WmdrmUtil.c $(WMDRM_NDDIR)/WmdrmHardware.c \
			  $(WMDRM_ND_OEMDIR)/OemImpl.c $(WMDRM_ND_OEMDIR)/DrmAes.c $(WMDRM_ND_OEMDIR)/DrmRsa.c \
			  $(WMDRM_ND_OEMDIR)/SybAes.c $(WMDRM_ND_OEMDIR)/SybAesKey.c $(WMDRM_ND_OEMDIR)/SybAesTabs.c \
			  $(WMDRM_ND_OEMDIR)/SybRsa.c

UPNPAV_SRC = $(UPNPAVDIR)/CommandLineShell.c $(UPNPAVDIR)/MicroSTB.c
UPNPAV_DB_SRC = $(UPNPAV_DBDIR)/ILibParsers.c $(UPNPAV_DBDIR)/ILibSSDPClient.c \
				$(UPNPAV_DBDIR)/ILibAsyncServerSocket.c $(UPNPAV_DBDIR)/ILibAsyncSocket.c \
				$(UPNPAV_DBDIR)/ILibWebClient.c $(UPNPAV_DBDIR)/ILibWebServer.c
UPNPAV_DMPS_SRC =
UPNPAV_MRCPS_SRC =
UPNPAV_MRDS_SRC =
UPNPAV_MSCPS_SRC =
UPNPAV_MSDS_SRC =
UPNPAV_RXCS_SRC =
UPNPAV_UTILITY_SRC = $(UPNPAV_UTILITYDIR)/MyString.c

UPNPSRC += $(UPNPAV_SRC) $(UPNPAV_DB_SRC) $(UPNPAV_DMPS_SRC) \
		   $(UPNPAV_MRCPS_SRC) $(UPNPAV_MRDS_SRC) \
		   $(UPNPAV_MSCPS_SRC) $(UPNPAV_MSDS_SRC) \
		   $(UPNPAV_RXCS_SRC) $(UPNPAV_UTILITY_SRC)

INCLUDES += -I$(UPNPAVDIR) -I$(UPNPAV_DBDIR) -I$(UPNPAV_DMPSDIR) \
			-I$(UPNPAV_MRCPSDIR) -I$(UPNPAV_MRDSDIR) \
			-I$(UPNPAV_MSCPSDIR) -I$(UPNPAV_MSDSDIR) \
			-I$(UPNPAV_RXCSDIR) -I$(UPNPAV_UTILITYDIR)

ifeq (remoteui,$(strip $(upnpav)))
UPNPAVDIR := $(UPNPDIR)/av
override DFLAGS_ADD += -DUPNP_AV -DUPNP_AV_PORT -DUPNP_AV_TASK \
					   -DUPnPAV_No_ServerDevice \
					   -DUPnPAV_No_ServerControlPoint -DUPnPAV_No_RendererControlPoint
#-DUPnPAV_No_RendererDevice -DUPnPAV_No_RemoteIO

UPNPAV_SRC += $(UPNPAVDIR)/MicroSTBState.c
UPNPAV_DB_SRC += $(UPNPAV_DBDIR)/UpnpMicroStack.c
UPNPAV_MRDS_SRC += $(UPNPAV_MRDSDIR)/CodecWrapper.c \
				   $(UPNPAV_MRDSDIR)/Emulator_Methods.c \
				   $(UPNPAV_MRDSDIR)/HttpPlaylistParser.c \
				   $(UPNPAV_MRDSDIR)/MicroMediaRenderer.c \
				   $(UPNPAV_MRDSDIR)/RendererStateLogic.c
UPNPAV_RXCS_SRC += $(UPNPAV_RXCSDIR)/RemoteIOClientStack.c $(UPNPAV_RXCSDIR)/XrtVideo.c
endif

ifeq (localcp,$(strip $(upnpav)))
UPNPAVDIR := $(UPNPDIR)/av2
override DFLAGS_ADD += -DUPNP_AV -DUPNP_AV_PORT \
					   -DUPnPAV_No_RendererDevice -DUPnPAV_No_ServerDevice \
					   -DUPnPAV_No_RendererControlPoint -DUPnPAV_No_RemoteIO \
					   -DWMDRM_ND -DNO_DRM_CRT
#-DUPNP_AV_TASK -DUPnPAV_No_ServerControlPoint

UPNPAV_DB_SRC += $(UPNPAV_DBDIR)/UPnPControlPoint.c
# $(UPNPAV_DBDIR)/M_CP_ControlPoint.c
UPNPAV_DMPS_SRC += $(UPNPAV_DMPSDIR)/FilteringBrowser.c
UPNPAV_MSCPS_SRC += $(UPNPAV_MSCPSDIR)/MmsCp.c $(UPNPAV_MSCPSDIR)/WMCClient.c

UPNPSRC += $(WMDRM_NDSRC)
INCLUDES += -I$(WMDRM_NDDIR) -I$(WMDRM_ND_OEMDIR)
endif

endif # def upnpav

ifeq (true,$(strip $(pcmcia_mem_card)))
CFSRC = $(CFDIR)/ide-cs.c $(CFDIR)/cs.c $(CFDIR)/cistpl.c \
		 $(CFDIR)/ataioreg.c $(CFDIR)/ataiopio.c \
		$(CFDIR)/ataiosub.c
endif

ifeq (true,$(strip $(broadcom_minipci)))
BROADCOMSRC = $(BROADCOMDIR)/wl_syabas.c $(BROADCOMDIR)/d11ucode.c $(BROADCOMDIR)/wlc.c\
			  $(BROADCOMDIR)/wlc_led.c $(BROADCOMDIR)/wlc_phy.c \
			  $(BROADCOMDIR)/wlc_rate.c $(BROADCOMDIR)/wlc_security.c \
			  $(BROADCOMDIR)/shared/syabas_osl.c $(BROADCOMDIR)/shared/nvramstubs.c \
			  $(BROADCOMDIR)/shared/bcmsrom.c $(BROADCOMDIR)/shared/hnddma.c \
			  $(BROADCOMDIR)/shared/bcmutils.c $(BROADCOMDIR)/shared/sbutils.c \
 			  $(BROADCOMDIR)/crypto/rc4.c $(BROADCOMDIR)/crypto/tkhash.c
endif


#RHAPSODYSRC = $(RHAPSODYDIR)/RdkAes.c $(RHAPSODYDIR)/RdkKeyBlock.c $(RHAPSODYDIR)/RdkSha1.c \
#			  $(RHAPSODYDIR)/RdkStreamCipher.c

#$(ATADIR)/harddisk.c
#$(CFDIR)/example.c
#$(CFDIR)/ataiotmr.c

# SW Ang, 20041011
# Expat XML Parser Source files
ifeq (true,$(strip $(xml_parser)))
XMLWFSRC = $(XMLWFDIR)/xmlwf.c $(XMLWFDIR)/xmlfile.c $(XMLWFDIR)/codepage.c $(XMLDIR)/simpleparser.c $(XMLDIR)/outline.c
#$(XMLWFDIR)/unixfilemap.c

XMLLIBSRC = $(XMLLIBDIR)/xmlparse.c $(XMLLIBDIR)/xmlrole.c $(XMLLIBDIR)/xmltok.c
#$(XMLLIBDIR)/xmltok_impl.c $(XMLLIBDIR)/xmltok_ns.c

INCLUDES += -I$(XMLDIR) -I$(XMLWFDIR) -I$(XMLLIBDIR)
# Expat XML Parser Object files
XMLOBJS = $(XMLWFDIR)/xmlwf.o $(XMLWFDIR)/xmlfile.o $(XMLWFDIR)/codepage.o  \
		  $(XMLLIBDIR)/xmlparse.o $(XMLLIBDIR)/xmlrole.o $(XMLLIBDIR)/xmltok.o $(XMLDIR)/simpleparser.o $(XMLDIR)/outline.o
#		  $(XMLLIBDIR)/xmltok_impl.o $(XMLLIBDIR)/xmltok_ns.o $(XMLWFDIR)/unixfilemap.o
endif

ifeq (true,$(strip $(divxdrm)))
override DFLAGS_ADD += -DINCLUDE_DIVX_DRM -DDIVX_BUTTON
ifeq (JP_IODATA,$(findstring JP_IODATA,$(customer)))
DIVXDRMOBJS = $(DIVXDRMDIR)/divx_drm.o
DIVXDRMCOBJS =	$(DIVXDRMDIR)/libAES/rijndael-alg-fst.o  \
				$(DIVXDRMDIR)/libAES/rijndael-api-fst.o  \
				$(DIVXDRMDIR)/libDrmCommon/base64.o      \
				$(DIVXDRMDIR)/libDrmCommon/crypt_util.o  \
				$(DIVXDRMDIR)/libDrmCommon/DrmHash.o     \
				$(DIVXDRMDIR)/libDrmCommon/master_key.o  \
				$(DIVXDRMDIR)/libDrmDecrypt/Bits.o  \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmAdpApi.o  \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmLocal.o  \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmMemory.o  \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmMessage.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmAdpHelper.o \
				$(DIVXDRMDIR)/libDrmCommon/sha1.o
else
ifeq (TW_LITEONIT,$(findstring TW_LITEONIT,$(customer)))
DIVXDRMOBJS = $(DIVXDRMDIR)/divx_drm.o
DIVXDRMCOBJS = $(DIVXDRMDIR)/libAES/rijndael-alg-fst.o \
				$(DIVXDRMDIR)/libAES/rijndael-api-fst.o \
				$(DIVXDRMDIR)/libDrmCommon/base64.o \
				$(DIVXDRMDIR)/libDrmCommon/crypt_util.o \
				$(DIVXDRMDIR)/libDrmCommon/DrmHash.o \
				$(DIVXDRMDIR)/libDrmCommon/master_key.o \
				$(DIVXDRMDIR)/libDrmDecrypt/Bits.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmAdpApi.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmMemory.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmMessage.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmAdpHelper.o \
				$(DIVXDRMDIR)/libDrmDecrypt/DrmLocal.o \
				$(DIVXDRMDIR)/libDrmCommon/sha1.o
else	#Liteon IT DIVXDRM Object code
DIVXDRMSRC = $(DIVXDRMDIR)/divx_drm.cpp
DIVXDRMCSRC =
endif	#IO-DATA
endif	#LiteonIT
endif	#dixv_drm=true

ifeq (true,$(strip $(dvdplayer)))
DVDCOREOBJS	= $(DVDDIR)/dvdcore/rmcorequeue.o \
			$(DVDDIR)/dvdcore/rmdvdcommandfactory.o \
			$(DVDDIR)/dvdcore/rmdvdcommandhelper.o \
			$(DVDDIR)/dvdcore/rmdvdcomplexcommands.o \
			$(DVDDIR)/dvdcore/rmdvddata.o \
			$(DVDDIR)/dvdcore/rmdvddomain.o \
			$(DVDDIR)/dvdcore/rmdvdnav.o \
			$(DVDDIR)/dvdcore/rmdvdnavpack.o \
			$(DVDDIR)/dvdcore/rmdvdobject.o \
			$(DVDDIR)/dvdcore/rmdvdparser.o \
			$(DVDDIR)/dvdcore/rmdvdrsm.o \
			$(DVDDIR)/dvdcore/rmdvdsimplecommands.o \
			$(DVDDIR)/dvdcore/rmdvdstatemachine.o \
			$(DVDDIR)/dvdcore/rmdvdtimer.o \
			$(DVDDIR)/dvdcore/rmpgci.o \
			$(DVDDIR)/dvdcore/rmregisters.o \
			$(DVDDIR)/dvdcore/rmtables.o \
			$(DVDDIR)/dvdcore/rmtests.o \
			$(DVDDIR)/dvdcore/rmttsrpt.o \
			$(DVDDIR)/dvdcore/rmdvdutils.o \
			$(DVDDIR)/vcdcore/vcdcore.o \
			$(DVDDIR)/cddacore/cddacore.o

# new dvd player
DVDPLAYERSRC	= $(DVDDIR)/dvd_entry.cpp \
				  $(DVDDIR)/detect_disc.cpp \
				  $(DVDDIR)/iso9660/iso9660parser.cpp \
				  $(DVDDIR)/dvd.cpp \
				  $(DVDDIR)/vcd.cpp \
				  $(DVDDIR)/cdda.cpp \
				  $(DVDDIR)/caribbean.cpp \
				  $(DVDDIR)/rmdvddemux.cpp \
				  $(DVDDIR)/dvd_softdemux.cpp

ifeq (US_SIGMADESIGN,$(findstring US_SIGMADESIGN,$(customer)))
DVDPLAYERGUISRC	= $(DVDDIR)/cheesy_gui.cpp \
				  $(DVDDIR)/cheesy_oem.cpp
else
ifeq (CN_LEGEND,$(strip $(customer)))
DVDPLAYERGUISRC	= $(DVDDIR)/legend_gui.cpp \
				  $(DVDDIR)/legend_oem.cpp
else
ifeq (TW_LITEONIT,$(findstring TW_LITEONIT,$(customer)))
DVDPLAYERGUISRC	= $(DVDDIR)/liteonit_gui.cpp \
				  $(DVDDIR)/liteonit_oem.cpp
else
ifeq (JP_IODATA,$(findstring JP_IODATA,$(customer)))
DVDPLAYERGUISRC	= $(DVDDIR)/iodata_gui.cpp \
				  $(DVDDIR)/iodata_oem.cpp
else
DVDPLAYERGUISRC	= $(DVDDIR)/syabas_gui.cpp \
				  $(DVDDIR)/syabas_oem.cpp
endif #JP_IODATA
endif #TW_LITEONIT
endif #CN_LEGEND
endif #US_SIGMADESIGN

DVDCSRC			=  $(DVDDIR)/caribbean_plainc.c

endif # dvdplayer=true

ifeq (true,$(strip $(fip)))

ifeq (TW_LITEONIT,$(findstring TW_LITEONIT,$(customer)))
ifneq (TW_LITEONIT_4,$(strip $(customer)))
FIPSRC	= $(FIPDIR)/fipkernel.c $(FIPDIR)/fipOps_liteon.c \
		  $(FIPDIR)/fiphw.c $(FIPDIR)/irstate.c \
		  $(FIPDIR)/fipuser_liteon.c $(FIPDIR)/fip_callback.c \
		  $(FIPDIR)/qa_test.c
else # use generic fip source for TW_LITEONIT_4 (Nagase DMA-100)
FIPSRC	= $(FIPDIR)/fipuser_common.c $(FIPDIR)/fipuser_nextbase.c \
		  $(FIPDIR)/fipuser_boot.c $(FIPDIR)/fip.c $(FIPDIR)/fip_callback.c
endif # neq TW_LITEONIT_4
else
FIPSRC	= $(FIPDIR)/fipuser_common.c $(FIPDIR)/fipuser_nextbase.c \
		  $(FIPDIR)/fipuser_boot.c $(FIPDIR)/fip.c $(FIPDIR)/fip_callback.c
endif # not TW_LITEONIT FIP

endif

ifeq (true,$(strip $(himatplayer)))
override DFLAGS += -DHIMAT_PLAYER
HIMATSRC	= $(HIMATDIR)/hmcore/hmcore.cpp \
			$(HIMATDIR)/hmmenu.cpp \
			$(HIMATDIR)/himat.cpp
endif # himatplayer=true

#
# Support for Ximeta Netdisk
#
NDASDIR = ./ndas
NDASINC = $(NDASDIR)/inc

#
# Includes
#
INCLUDES += -I. -I$(HWLIB_DIR) -I$(HWLIB_DIR)/rua/include -I$(HWLIB_DIR)/dcc/include \
			-I$(HWLIB_DIR)/rmdef -I$(HWLIB_DIR)/cipher \
			-I$(OSDIR) -I$(SYABAS3DIR) -I$(GRAPHICSDIR) -I$(JPGDIR) \
			-I$(LIBPNGDIR) -I$(ZLIBDIR) -I$(BIOSDIR) -I$(TCPIP_INCLUDE) \
			-I$(FILESYSDIR) -I$(FLASHDIR) -I$(BROWSERDIR) -I$(USBDIR) \
			-I$(VODDIR) -I$(STDLIBDIR) -I$(SETUPDIR) -I$(CGIDIR) -I$(UPNPDIR) \
			-I$(SSLDIR) -I$(SSL_CRYPTO) -I$(SSL_DES) -I$(SSL_BF) -I$(SSL_CAST) \
			-I$(SSL_IDEA) -I$(SSL_RC2) -I$(SSL_RC4) -I$(SSL_RC5) -I$(SSL_SHA) \
			-I$(SSL_LHASH) -I$(SSL_MD2) -I$(SSL_MD5) -I$(SSL_MDC2) -I$(SSL_RIPEMD) \
			-I$(SSL_HMAC) -I$(SSL_RSA) -I$(SSL_DSA) -I$(SSL_ASN1) -I$(SSL_BIO) \
			-I$(SSL_BN) -I$(SSL_BUFFER) -I$(SSL_CONF) -I$(SSL_DH) -I$(SSL_ERR) \
			-I$(SSL_EVP) -I$(SSL_OBJECTS) -I$(SSL_PEM) -I$(SSL_PKCS7) \
			-I$(SSL_RAND) -I$(SSL_STACK) -I$(SSL_TXT_DB) -I$(SSL_X509) \
			-I$(DVDDIR) -I$(FIPDIR) -I$(ATADIR) -I$(WIFIDIR) -I$(RESOURCEDIR) \
			-I$(BROADCOMDIR)/include -I$(BROADCOMDIR) -I$(HIMATDIR) \
            		-I$(MADHAL_DIR) -I$(ATHELOS_DIR)/include	\
			-I$(NDASINC)
#
# Libraries
#
LIBS = -L$(LIBC) -L$(GCCDIR) -lc -lm -lgcc -lc

LIBS_WO_SRC = $(LIBWMADIR)/libwma.a $(LIBAACDIR)/libaac.a \
			  $(DVDDIR)/dvdcore/dvdcore.a $(DVDDIR)/vcdcore/vcdcore.a \
			  $(DVDDIR)/cddacore/cddacore.a \
			  $(NDASDIR)/libndas.a
# [20040430] YC Yeoh, libmad
ifeq (true,$(strip $(libmad)))
override DFLAGS_ADD += -DLIBMAD
override LIBS_WO_SRC += $(LIBMADDIR)/libmad.a
endif

#$(TRANSCODERDIR)/core.a

LIBS_W_SRC = $(LIBPNGDIR)/libpng.a $(ZLIBDIR)/libz.a $(SSLDIR)/libssl.la \
			 $(LIBTREMORDIR)/libvorbisidec.la

LIBS_ADD = $(LIBS_WO_SRC) $(LIBS_W_SRC)

#
# Source files
#
LOADER0_SRC = $(LOADER0ASM:.S=.o)
LOADER1_SRC = $(LOADER1ASM:.S=.o) $(LOADER1SRC:.c=.o)
LOADER_SRC = $(LOADER0_SRC) $(LOADER1_SRC)

# source files that are not opened
NOPEN_SRC = $(MAINASM:.S=.o) $(MAINSRC:.c=.o) $(BROADCOMSRC:.c=.o)\
			$(OSSRC:.c=.o) $(SYABAS3SRC:.c=.o) $(GRAPHICSSRC:.c=.o) \
		$(JPGSRC:.c=.o) $(LIBPNGSRC:.c=.o) $(NOSSRC:.c=.o) $(TCPIPSRC:.c=.o) \
			$(FILESYSSRC:.c=.o) $(FLASHSRC:.c=.o) $(BROWSERSRC:.c=.o) \
			$(VODSRC:.c=.o) $(STDLIBSRC:.c=.o) \
			$(UPNPSRC:.c=.o) \
			$(ASFSRC:.c=.o) $(ATASRC:.c=.o) $(VODHELPERSRC:.c=.o) \
			$(USBSRC:.c=.o) \
			$(SETUPSRC:.c=.o) $(ATHEROSSRC:.c=.o)
#			$(AVITESTSRC:.c=.o) $(AVITESTCPPSRC:.cpp=.o)

#			$(YMSRC:.c=.o) $(WIFISRC:.c=.o) \
#			$(CFSRC:.c=.o)
# for single files that are in $(NOPEN_SRC) but need to open
OPENSRC =	main.c $(VODDIR)/em86xx.c $(VODDIR)/asf.c \
			$(GRAPHICSDIR)/mypal.c \
			$(TCPIP_SDESIGNS)/enet.c \
			$(TCPIP_SDESIGNS)/rtl8139_pkt.c


# all source files in directory that are opened
#OPEN_SRC = $(HWLIB_SRC:.c=.o) $(QHWLIB_SRC:.c=.o) $(BOARDS_SRC:.c=.o) \
#			$(DECODERS_SRC:.c=.o) $(TVENCS_SRC:.c=.o) $(PRIVATE_SRC:.c=.o) \
#			$(BIOSSRC:.c=.o) $(DVDPLAYERSRC:.cpp=.o) $(DVDCSRC:.c=.o) \
#			$(DVDPLAYERGUISRC:.cpp=.o) $(FIPSRC:.c=.o) $(LITEONFPSRC:.c=.o) $(CGISRC:.c=.o)
#Syabas3, CW Cheah 20040217
OPEN_SRC = $(BIOSSRC:.c=.o) $(DVDPLAYERSRC:.cpp=.o) $(DVDCSRC:.c=.o) \
		   $(DVDPLAYERGUISRC:.cpp=.o) $(CGISRC:.c=.o)

# Expat XML Parser Open Source
OPEN_SRC += $(XMLWFSRC:.c=.o) $(XMLLIBSRC:.c=.o)

ifneq (true,$(strip $(buffalosdk)))
OPEN_SRC += $(FIPSRC:.c=.o)
OPENSRC	 +=	$(SETUPDIR)/lansetup.c $(SETUPDIR)/dvdsetup.c $(SETUPDIR)/ntpsetup.c \
			$(SETUPDIR)/preferen.c $(SETUPDIR)/languages_pack.c $(SETUPDIR)/init_option.c \
			$(SETUPDIR)/wifisetup.c
else
NOPEN_SRC += $(FIPSRC:.c=.o)
endif

#OPENSRC += $(RHAPSODYDIR)/RdkAes.c $(RHAPSODYDIR)/RdkKeyBlock.c $(RHAPSODYDIR)/RdkSha1.c \
#		   $(RHAPSODYDIR)/RdkStreamCipher.c

ifeq (true,$(strip $(aoss)))
OPENSRC	 +=	$(SETUPDIR)/aoss_api.c
endif

ifeq (true,$(strip $(divxdrm)))
ifneq (JP_IODATA,$(findstring JP_IODATA,$(customer)))
OPEN_SRC += $(DIVXDRMSRC:.cpp=.o) $(DIVXDRMCSRC:.c=.o)
endif
endif

ifeq (true,$(strip $(himatplayer)))
NOPEN_SRC += $(HIMATSRC:.cpp=.o)
endif

# for SDK use only
SDK_SRC =

MAIN_SRC = $(NOPEN_SRC) $(OPEN_SRC) $(SDK_SRC)

# MAIN_OBJS	= $(DVDCOREOBJS)

ifeq (true,$(strip $(divxdrm)))
ifeq (JP_IODATA,$(findstring JP_IODATA,$(customer)))
MAIN_OBJS	+= $(DIVXDRMOBJS) $(DIVXDRMCOBJS)
endif

ifeq (TW_LITEONIT,$(findstring TW_LITEONIT,$(customer)))
MAIN_OBJS	+= $(DIVXDRMOBJS) $(DIVXDRMCOBJS)
endif
endif	#divx_drm=true

#
# rules
#
.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $(DFLAGS) -c -o $@ $<

.c.S:
	$(CC) $(INCLUDES) $(CFLAGS) $(DFLAGS) -c -S -o $@ $<

.S.o:
	$(CC) $(DFLAGS) -c -o $@ $<

.cpp.o:
	$(CC) $(INCLUDES) $(CFLAGS) $(DFLAGS) -c -o $@ $<

#
# Generating prerequisites automatically
#
override define gen-dep
$(CC) $(INCLUDES) $(CFLAGS) $(DFLAGS) -MM $< | sed -e 's|\($(*F)\)\.o[ :]*|$(basename $@).o $@ : |g' > $@
[ -s $@ ] || $(RM) $@
endef # gen-dep

%.d: %.c
	$(gen-dep)

%.d: %.cpp
	$(gen-dep)

%.d: %.S
	$(gen-dep)
