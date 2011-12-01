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
#ifndef _NDAS_ROKU_GUI_JPEG_H_
#define _NDAS_ROKU_GUI_JPEG_H_

#include <cascade/Cascade.h>
#include <cascade/util/CascadeJpegFile.h>
#include <deschutes/libraries/hdmachinex225/VideoScaler.h>
#include "defs.h"

class jpeg {
    VideoScaler::ScalerColorFormat m_colorFormat;
    VideoScaler m_scaler;    
    u32 m_nScalerBytesPerPixel;
    CascadeRect m_scalerOutputRect;
    u8 * m_pScalerVRAM;
    u8 * m_pData;
    u32 m_nScalerRowBytes;

    CascadeDims m_jpegSrcDims; 
    
    CascadeJpegFile m_jpegfile;
    u32 m_vmem_alloc_size;
    char m_name[255];
public:
    jpeg() : m_pData(NULL),m_pScalerVRAM(NULL)
    {
        m_colorFormat = VideoScaler::kColorFormatInvalid;
        m_nScalerBytesPerPixel = 0; // compute later
        m_nScalerRowBytes = 0;
        m_vmem_alloc_size = 0;
        
    }
    ~jpeg() 
    {
        debug("~jpeg");
        if (NULL != m_pScalerVRAM)
        {
            m_scaler.FreeScalerVRAM(m_pScalerVRAM);
            m_pScalerVRAM = NULL;
        }
        if (NULL != m_pData )
        {
            delete m_pData;
            m_pData = NULL;
        }
        if (m_scaler.IsOpen()) 
        {
            m_scaler.Close();
        }
    }
    void SetName(const char* name) {
        strncpy(m_name,name,255);
    }
    inline void 
    DumpRect(const char * pName, const CascadeRect & rect)
    {
        debug("%s = (%d, %d, %d, %d)", pName, (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    }
    
    void Hide() 
    {
        debug("-ing");
        if ( !m_pScalerVRAM ) return;
        u32 packed_row_bytes = m_jpegSrcDims.w*m_nScalerBytesPerPixel;
        u32 line_short_count = m_jpegSrcDims.w*m_nScalerBytesPerPixel/2;
        u32 i=0;
        s32 y = m_jpegSrcDims.h;
        u16* to_ptr;
        while (y>0)
        {
            to_ptr =   (u16*)(m_pScalerVRAM + (y-1)*m_nScalerRowBytes+packed_row_bytes);
            for (i=0; i < line_short_count; i++)
            {
                *(--to_ptr) = 0;
            }

            y--;
        }
        debug("ed");
    }

    bool Init(CascadeRect outRect) 
    {
    // get the screen rectangle.  This is the "drawing surface"
        CascadeScreen screen;
        CascadeRect rect(screen.GetRect());
        
        // here's the deal: CascadeWindows draw on CascadeScreen which draws
        // to the graphics plane.  The graphics plane's dimensions are the
        // dimensions of CascadeScreen::GetRect() (currently 1024x576).
        // However, the video scaler operates in the dimensions of the video
        // OUTPUT resolution (e.g. 1080i or 720p), so if we
        // want to open a video scaler window the same size as a CascadeWindow,
        // we need to compute the size and position of the CascadeWindow in OUTPUT resolution
        // coordinate space and use that for the video scaler window.
        // NOTE: we actually use the video output SCALE RECT since that is
        // the actual resolution the graphics plane is scaled to.  The SCALE RECT
        // by the way, is the rectangle that gets set in the "Video Placement"
        // Setup Panel.
        

        // First figure out the horizontal and vertical scale factors for the
        // ratio between the screen resolution rectangle and the screen output
        // SCALE rectangle.
        u32 nIndex = 0;
        CascadeRect scaleRect;

        // figure out what output resolution [index] the screen is set to.
        screen.GetCurrentOutputResolution(nIndex);

        //sets scaleRect with the output resolution scaling rectangle for the output resolution 
        // identified by nIndex. For example, if the output resolution is set to 720p (1280x720),
        // the output resolution scale rect by default will be CascadeRect(0, 0, 1280, 720)
        screen.GetOutputResolutionScaleRect(nIndex, scaleRect);
        
        // just for fun, print out the ScreenRect (CascadeWindow coordinate system)
        // and the scale rect (CascadeScreen output resolution coordinate system)
        DumpRect("ScreenRect", rect);
        DumpRect("ScaleRect", scaleRect);
        double scaleHorz = (double)scaleRect.w / (double)rect.w;
        double scaleVert = (double)scaleRect.h / (double)rect.h;
        // ok, we've computed the scale factors between the two systems
    //    SetRectAbsolute(rect);          // set it as my CascadeWindow's rectangle to full screen

        // now we're ready to recast our CascadeWindow rectangle (rect) into a rectangle
        // of the same size and position in the VideoScaler (output scale rect) coordinate space.
        m_scalerOutputRect.x = (s32)((double)outRect.x * scaleHorz + (double)scaleRect.x + (double)0.5);
        m_scalerOutputRect.y = (s32)((double)outRect.y * scaleVert + (double)scaleRect.y + (double)0.5);
        m_scalerOutputRect.w = (u32)((double)outRect.w * scaleHorz + (double)0.5);
        m_scalerOutputRect.h = (u32)((double)outRect.h * scaleVert + (double)0.5);        
        //DumpRect("outRect",outRect1);
        // first figure out bytes/pixel that we're using
        CascadeJpegFile::ColorSpace jpeg_clr_format = CascadeJpegFile::eYUYV;
        // open and configure the video scaler
        m_colorFormat = VideoScaler::kColorFormatVYUY422;
        m_nScalerBytesPerPixel = 2;

        // Open the JPEG file, we need info, such as dimensions, soon
        

        if (!m_jpegfile.Init(m_name, jpeg_clr_format))
            {debug("jpeg init failed");return false;}

        // open up the scaler in that format
        if (! m_scaler.Open(m_colorFormat)) return false;
        
         // get jpeg's native width and height
        m_jpegSrcDims.w=m_jpegfile.GetImageWidth();
        m_jpegSrcDims.h=m_jpegfile.GetImageHeight();

        // set the dimensions of the source memory.  
        // We can choose any dimensions that we want - the video scaler will
        // scale this source memory to the m_scalerOutputRect we computed in
        // the constructor.  In our case, we'll use the dimensions of the
        // CascadeWindow so that we can stay living in the CascadeWindow coordinate
        // space.
        u32 nPixelPitch = 0;
        if (! m_scaler.SetSourceWindow(m_jpegSrcDims, nPixelPitch)) return false;

        // ok, we told the scaler what we wanted (m_jpegSrcDims) and it gave us back
        // a line-width for memory allocation in terms of pixels (in the current color mode)
        // convert this to a byte pitch:
        m_nScalerRowBytes = nPixelPitch * m_nScalerBytesPerPixel;

        // Ask the JPEG class what size it thinks the bitmap will take (at native resolution)
        u32 jpeg_src_bitmapsize = m_jpegfile.GetBitmapDataSize(0, 0);
        debug("jpeg_src_bitmapsize=%d",jpeg_src_bitmapsize); 

        // Calculate video memory allocation size including alignment padding
        m_vmem_alloc_size = m_jpegSrcDims.h * m_nScalerRowBytes;
        debug("calc bitmapsize (m_vmem_alloc_size) = %d", m_vmem_alloc_size);

        // allocate a video memory buffer with correct padding and alignment
        // Xilleon scaler requires source to be on 64 pix (128 byte) boundry and size
        // But we are using the PixelPitch returned by SetSourceWindow so as to maintain some
        // device indpendence

        if (NULL == (m_pScalerVRAM = m_scaler.AllocateScalerVRAM(m_vmem_alloc_size)))
            {debug("vid mem alloc failed!");return false;}
    
        // tell the scaler about the memory we just allocated
        if (! m_scaler.SetSourceVRAM(m_pScalerVRAM)) return false;

        // tell the scaler about the output rectangle we computed in the constructor
        if (! m_scaler.SetDestWindow(m_scalerOutputRect, false)) return false;

        // crop/zoom the scaler output 1:1 - this is a tricky one
        // what it does is actually take what would be imaged to the output
        // rectangle and crop that result (after scaler pass 1) to the crop
        // rectangle we're setting with SetCropWindow().  It still displays
        // the output in the rect set with SetDestWindow(), so the result is
        // actually zoomed in or zoomed out data.
        if (! m_scaler.SetCropWindow(m_scalerOutputRect)) return false;

        // scaler is ready to go - let's display it on the screen
        // for now I'm showing it before we do the data copies so that I can see the copies in action
        if (! m_scaler.Show(VideoScaler::kBlendModeAdd)) return false; // using scaler for graphics, not video
        CascadeRect noclip(0,0,m_jpegfile.GetImageWidth(), m_jpegfile.GetImageHeight());
        if (!m_jpegfile.GetBitmapData(m_pScalerVRAM , m_vmem_alloc_size,  noclip))
            {debug("jpeg GetBitmapData failed");return false;}
        // decode the jpeg into the scaler's video source memory
        // GetBitmapData returns data packed with no pitch alignment.
        // The following code takes the "packed" data and alligns each line so that it is the correct
        // number of bytes in length for the hardware (the HD1000 hardware requires that each line
        // of the video scaler be a multiple of 128 bytes in length.  But this is hardware
        // dependent and so the "pitch" is returned by cascade to make cascade platfrom independent.
        // We won't be able to see the effects of this until the OnPaint() makes the g0 window
        // transparent.
        u32 packed_row_bytes = m_jpegSrcDims.w*m_nScalerBytesPerPixel;
        u32 line_short_count = m_jpegSrcDims.w*m_nScalerBytesPerPixel/2;
        s32 y = m_jpegSrcDims.h;

        u16* from_ptr = (u16*)(m_pScalerVRAM  + y*packed_row_bytes);
        u16* to_ptr;

        // scan from bottom to the top so that we can do the operation in place
        u32 i=0;
        while (y>0)
        {
            to_ptr =   (u16*)(m_pScalerVRAM + (y-1)*m_nScalerRowBytes+packed_row_bytes);
            for (i=0; i < line_short_count; i++)
            {
                *(--to_ptr) = *(--from_ptr);
            }

            y--;
        }
        return true;
    }

};
#endif

