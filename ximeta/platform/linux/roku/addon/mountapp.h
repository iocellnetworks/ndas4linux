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
#ifndef _MOUNT_APP_
#define _MOUNT_APP_

#include <cascade/Cascade.h>
#include <cascade/app/CascadeApp.h>
#include "mountwin.h"
#include "defs.h"

#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((x)<(y)?(x):(y))

class MountApp : public CascadeApp
{
private:
    MountWin m_win;

public:
    virtual void OnActivate() { 
        debug("ing");
        CascadeScreen screen;
        CascadeRect rect = screen.GetRect();
        debug("%d %d %d %d",rect.x, rect.y, rect.w, rect.h);
        int w = rect.w;
        int h = rect.h;
        rect.x = max(w/10, 10);
        rect.y = max(h/10, 10);
        rect.w = w - rect.x *2;
        rect.h = h - rect.y *2;
        debug("%d %d %d %d",rect.x, rect.y, rect.w, rect.h);
           m_win.SetRectAbsolute(rect);
        m_win.Materialize(CascadeWindow::kTopMost); 
    } 
    virtual void OnDeactivate() {
        debug("ing");
        
        m_win.Vanish(); 
    }
};
#endif

