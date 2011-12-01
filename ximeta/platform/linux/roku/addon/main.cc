/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <libgen.h>
#include <cascade/Cascade.h>
#include "mountapp.h"
#include "defs.h"

jpeg g_jpeg;

int main(int argc, const char **argv) 
{
    char jpegfile[255];
    debug("%s %s %s\n",__FUNCTION__, __DATE__, __TIME__); 
    snprintf(jpegfile,sizeof(jpegfile),"%s/ndas.jpg",dirname((char*)argv[0]));
    
    g_jpeg.SetName(jpegfile);
    MountApp app;
    return app.Run(argc,argv);
}
