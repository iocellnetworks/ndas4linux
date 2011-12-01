/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _NDAS_GUI_DEVICE_WIDGET_H_
#define _NDAS_GUI_DEVICE_WIDGET_H_

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

using namespace std;

#include <cascade/Cascade.h>

#include "mountingthread.h"

extern "C" {
#include <stdio.h> // snprintf jpeglib.h
#include <string.h> //strcpy
#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/ioctl.h> // ioctl
#include <sys/types.h> // open
#include <sys/stat.h> // open
#include "ndasdev.h"
};

/* Support one unit disk per NDAS device */
class NDAS 
{
public:
   
    static string get_first_slot(string serial)
    {
    	char ret[10];
    	string path("/proc/ndas/devices/");
    	path += serial;
        path += "/slots";

        debug("reading %s", path.c_str());
        memset(ret,0, sizeof(ret));
    	ifstream ifs(path.data());
        
    	if ( ifs.fail() ) 
        {
            return string("");
    	}
        
    	ifs.getline(ret, sizeof(ret));
        debug("/proc/ndas/devices/%s/slots = %s", serial.c_str(), ret);
    	if ( ifs.fail() ) 
        {
            return string("");
    	}
        
    	ifs.close();
        debug("slot=%s", string(ret).c_str());
    	return string(ret);
    }
    
    static void start() 
    {
        int fd = open("/dev/ndas", O_NDELAY);
        ioctl(fd, IOCTL_NDCMD_START, NULL);
        close(fd);
    }

    static void stop() 
    {
        int fd = open("/dev/ndas", O_NDELAY);
        ioctl(fd, IOCTL_NDCMD_STOP, NULL);
        close(fd);
    }

    static ndas_error_t enable(int slot, int write_mode) {
        int fd = open("/dev/ndas", O_NDELAY);
        if ( fd < 0 ) return false;

        ndas_ioctl_enable_t arg;
        arg.slot = slot;
        arg.write_mode = write_mode;
        
        ioctl(fd, IOCTL_NDCMD_ENABLE, &arg);

        close(fd);
        return arg.error;
    }

    static ndas_error_t disable_all(string serial)
    {
        char ret[10];
        string path = string("/proc/ndas/devices/") + serial + "/slots"; 
        debug("ing path=%s", path.c_str());
        ifstream ifs(path.c_str());
        if ( ifs.fail() )
            return NDAS_ERROR;
        // TODO check unit disks and raid
        ifs.getline(ret, sizeof(ret));
        ifs.close();        
        if ( ifs.fail() )
            return NDAS_ERROR;
        debug("ing slot %d", atoi(ret));
        return disable(atoi(ret));
    }
    static ndas_error_t register_device(string name, string ndasid) 
    {
        if ( ndasid.empty() ) return NDAS_ERROR;
        
        int fd = open("/dev/ndas", O_NDELAY);
        if ( fd < 0 ) return NDAS_ERROR;

        ndas_ioctl_register_t arg;
        
        strncpy(arg.ndas_idserial, ndasid.c_str(), ndasid.length() + 1);
        arg.ndas_key[0] = 0;
        strncpy(arg.name, name.c_str(), name.length() + 1);
        
        ioctl(fd, IOCTL_NDCMD_REGISTER, &arg);

        close(fd);
        return arg.error;
    }
    
    static ndas_error_t unregister_device(string name) 
    {
        debug("ing name=%s",name.c_str());
        if ( name.empty() ) return false;
        debug("ing name=%s",name.c_str());
        
        int fd = open("/dev/ndas", O_NDELAY);
        if ( fd < 0 ) return false;
        debug("ing name=%s",name.c_str());

        ndas_ioctl_unregister_t arg;
        debug("ing name=%s",name.c_str());
        strncpy(arg.name, name.c_str(), NDAS_MAX_NAME_LENGTH);
        
        debug("ing name=%s",name.c_str());
        int ret = ioctl(fd, IOCTL_NDCMD_UNREGISTER, &arg);
        debug("ing name=%s",name.c_str());
        close(fd);
        debug("ret=%d err=%d", ret, arg.error);
        return arg.error;
    }
    
    static ndas_error_t disable(int slot) {
        debug("ing");

        int fd = open("/dev/ndas", O_NDELAY);
        if ( fd < 0 ) return false;

        ndas_ioctl_arg_disable_t arg;
        arg.slot = slot;

        debug("disabling slot=%d", arg.slot);
        
        ioctl(fd, IOCTL_NDCMD_DISABLE, &arg);

        close(fd);
        debug("err=%d", arg.error);
        return arg.error;

    }
    
    /* Get NDAS list in the network */
    static vector<string>* get_serial_list() 
    {
        char name[NDAS_MAX_NAME_LENGTH];
        debug("ing");
        ifstream ifs("/proc/ndas/probed");

        if ( ifs.fail() )
            return NULL;
        
        vector<string> *ret = new vector<string>;
        
        while( ({ ifs.getline(name, sizeof(name)); !ifs.fail(); }) ) 
        {
            ret->push_back(string(name));
            debug("name=%s size=%d", string(name).c_str(), string(name).size());
        }
        ifs.close();          
        
        return ret;
    }

    static vector<string>* get_slot_list() 
    {
        char slot[10];
        debug("ing");
        ifstream ifs("/proc/ndas/slot_list");
        if ( ifs.fail() ) {
            return NULL;
        }
        vector<string> *ret = new vector<string>;
        
        while( ({ ifs.getline(slot, sizeof(slot)); !ifs.fail(); }) ) 
        {
            ret->push_back(slot);
        }
        ifs.close();

        debug("1st ioctl #ofdisk=%d", ret->size());
        
        return ret;
    }
    static bool unregister_all() 
    {
        int r;
        char name[NDAS_MAX_NAME_LENGTH];
        debug("ing");
        ifstream ifs("/proc/ndas/registered");

        if ( ifs.fail() )
            return 0;

        while( ({ ifs.getline(name, sizeof(name)); !ifs.fail(); }) ) {
            disable_all(name);
            unregister_device(name);
        }
        ifs.close();            
        
        return 1;
    }
    static string get_serial(string slot) 
    {
        char serial[NDAS_SERIAL_LENGTH+1];
        string path("/proc/ndas/slots/");
        path += slot;
        path += "/ndas_serial";
        debug("ing slot %s", slot.c_str());
        
        ifstream ifs(path.c_str());
        if ( ifs.fail() )
            return string();
        ifs.getline(serial, NDAS_SERIAL_LENGTH);
        ifs.close();
        return string(serial);
        
    }
};

class DeviceWidget : public CascadeWidget
{
private:
    
    /* the index of which disk is enable. -1 is no disk is enabled */
    int m_enabled;
    /* index where the cursor is */
    int m_index;
    int m_viewable_first;
    int m_viewable_size;
    CascadeFont m_font;
    CascadeBitmap m_logo;
    bool m_mounting;
    MountingThread m_mounter;

public:
    vector<string>* m_serial_list;
    
    
    DeviceWidget() 
        : m_mounting(false)
        , m_index(0)
        , m_viewable_first(0)
        , m_enabled(-1)
        , m_serial_list(NULL)
    {
        CascadeFont::Attributes attributes;
        debug("ing");
        //NDAS::start();
        strcpy(attributes.m_faceName, "system");
        attributes.m_nPointSize = FONT_HEIGHT;
        attributes.m_weightFlags = CascadeFont::kBold;
        m_font.SetAttributes(attributes);
    }
    
    ~DeviceWidget() 
    {
        if ( m_serial_list ) delete m_serial_list;
    }

    void CancelMounting() 
    {
        m_mounting = false;
    }

    virtual void Select()
    {
        debug("ing");
        vector<string>* serials = NDAS::get_serial_list();
    	if ( !serials || serials->size() == 0) 
        {
    		debug("no list");
    		return;
    	}

        m_mounting = true;
        m_mounter.start(this, m_index);
        Redraw();

    }


    virtual void GetList() 
    {
        m_serial_list = NDAS::get_serial_list();    
    }
    virtual void OnMaterialize() { 
        debug("ing");
        SetFocus(); 
        debug("SetFocus-ed");
        
        GetList();
        debug("GetNetDiskList-ed");
        m_viewable_size = (GetRectRelative().h - INSET*2)/(FONT_HEIGHT+ FONT_HEIGHT_GAP);
    
    }
    virtual bool OnNavKey(u32 key, CascadeWindow * & pProposedNewFocusWnd)
    {
        debug("%s key=%u",__FUNCTION__,key);
        if ( m_mounting ) return true;
        switch (key) {
        case CK_NORTH:
            if ( --m_index < 0 )
                m_index = 0;
            if ( m_index < m_viewable_first )
                m_viewable_first = m_index;
            this->Redraw();
            break;
        case CK_SOUTH:
            if ( ++m_index >= m_serial_list->size())
                m_index = m_serial_list->size() - 1;
            if ( m_index >= m_viewable_first + m_viewable_size )
                m_viewable_first++;
            Redraw();
            break;
        case CK_SELECT:
            Select();
            break;
        }
        return true;
    }
    virtual bool OnKeyDown(u32 key) 
    {
        if ( m_mounting ) return true;
        debug("key=%u",key);
        switch (key) {
        case CK_EXIT:
            CascadeApp::GetApp()->Terminate(0);
            break;
        case CK_SELECT:
            Select();
            break;
        }
        return true;
    }
    virtual void OnPaint(CascadeBitmap & bitmap) { 

        int i;
        CascadeColor red(255, 0, 0); 
        CascadeColor black(0, 0, 0); 
        CascadeColor white(255, 255, 255); 


        CascadeRect r(this->GetRectAbsolute()); 
        CascadePoint p(r.x + INSET + 5 ,r.y + INSET);
        bitmap.FillRect(r, CascadeColor(0,0,0,0)); 
        
        
        if ( m_mounting ) {
            CascadeString message("Mounting...");
            bitmap.TextOut(p, message, m_font,black);
            debug("ed mounting");

            return;            
        }
        if ( m_serial_list->size() <=0 ) {
            CascadeString message("Not Available");
            bitmap.TextOut(p, message, m_font,black);
            return;
        }

        for (i = m_viewable_first; 
                i < m_viewable_first + m_viewable_size 
                    && i < m_serial_list->size();i++)
        {
            char buffer[10+2];
            const char *ndasid = (*m_serial_list)[i].data();
            strncpy(buffer,ndasid,3);
            buffer[3] = '-';
            strncpy(buffer + 4,(const char *)ndasid + 3, 5); 
            buffer[9] = 0;
            CascadeString string(buffer); 
            if ( i == m_index ) {
                bitmap.TextOut(p, string, m_font, red); 
            } else {
                bitmap.TextOut(p, string, m_font, black); 
            }
            p.y += FONT_HEIGHT + FONT_HEIGHT_GAP;
        }
    };
};

#endif
