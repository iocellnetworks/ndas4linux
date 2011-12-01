/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include "pthreadclass.h"

void _r_(PThreadClass *thread)
{
	thread->run();
}

