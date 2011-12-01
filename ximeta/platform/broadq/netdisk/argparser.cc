
#include <mio/argparser.h>

MioArgParser::MioArgParser(int argc, const char *argv[])
    :    myNext(0), myArgc(argc), myArgv(argv)
{ }

MioArgParser::~MioArgParser()
{ }

const char *
MioArgParser::peek() const
{
    if (myNext < myArgc) return myArgv[myNext];
    return 0;
}

const char *
MioArgParser::get()
{
    const char *arg = peek();
    if (arg) myNext++;
    return arg;
}

const char *
MioArgParser::isOption()
{
    const char *arg = peek();

    if (0 == arg) return 0;

    if ('-' == arg[0]) return get() + 1; // skip past '-'

    return 0;
}
