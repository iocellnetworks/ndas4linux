#ifndef __TRACE_H__
#define __TRACE_H__
#include <_ansi.h>

#define TRACE_MAX_LINES 100
#define TRACE_LINE_SIZE 80

typedef int (*MioTraceWriter)(void *cookie, const unsigned char *msg, int length);

typedef struct {
    char tx[TRACE_LINE_SIZE+1];
} TraceText;

typedef struct {
    int next;
    TraceText lines[TRACE_MAX_LINES];
} TracePackage;

_BEGIN_STD_C
extern void TraceInit(TracePackage*);
extern int TraceGetNext(TracePackage*);
extern void TraceAdd(TracePackage*, const char *text);
extern const char *TraceGetText(TracePackage*, int ix);
extern void TraceSetWriter(MioTraceWriter writer, void *cookie);
_END_STD_C

#endif // __TRACE_H__
