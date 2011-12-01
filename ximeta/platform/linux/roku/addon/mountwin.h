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
#ifndef _MOUNT_WIN_
#define _MOUNT_WIN_

#include <cascade/Cascade.h>
#include "defs.h"
#include "devicewidget.h"
#include "jpeg.h"


extern jpeg g_jpeg;

class MountWin: public CascadeWindow
{
private:
    DeviceWidget m_listWin;
    CascadeFont m_font;
    
public:
    MountWin() : m_listWin()
    {
        
        CascadeFont::Attributes attributes;
        strcpy(attributes.m_faceName, "univercb");
        attributes.m_nPointSize = FONT_HEIGHT;
        attributes.m_weightFlags = CascadeFont::kBold;
        m_font.SetAttributes(attributes);
    }
    virtual void OnMaterialize() { 
        debug("MountWin:rect={%d,%d,%d,%d}\n", this->GetRectAbsolute().x,
            this->GetRectAbsolute().y,
            this->GetRectAbsolute().w,
            this->GetRectAbsolute().h);
        
        CascadeRect rect(this->GetRectAbsolute()); 
        rect.x += INSET + FONT_HEIGHT + FONT_HEIGHT_GAP;
        rect.y += INSET + FONT_HEIGHT + FONT_HEIGHT_GAP;
        rect.w -= (INSET + FONT_HEIGHT + FONT_HEIGHT_GAP) * 2;
        rect.h -= (INSET + FONT_HEIGHT + FONT_HEIGHT_GAP) * 2;
        
        m_listWin.SetRectAbsolute(rect);
        m_listWin.Materialize(CascadeWindow::kTopMost); 
        g_jpeg.Init(this->GetRectAbsolute());

    }
    virtual void OnPaint(CascadeBitmap & bitmap) { 
        CascadeColor black(0, 0, 0);
        CascadeColor transparent(0, 0, 0, 0); 
        CascadeRect rect(this->GetRectAbsolute()); 
        bitmap.FillRect(rect, transparent); 
        CascadePoint p(rect.x + INSET + FONT_HEIGHT + FONT_HEIGHT_GAP , rect.y + INSET);
        bitmap.TextOut(p, "Please select one of the following NDAS Devices", m_font,black);
        
        CascadeString copyright("Copyright (C) 2004-2005 XIMETA, Inc.");
        p.y = rect.h + FONT_HEIGHT_GAP;
        p.x = rect.w - m_font.GetTextExtent(copyright).w + FONT_HEIGHT_GAP;
        bitmap.TextOut(p, copyright, m_font, black);
    }
};
#endif

