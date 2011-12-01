
#ifndef __MIO_SIMPLE_STRING_H__
#define __MIO_SIMPLE_STRING_H__

#if defined(__IOP__)

class sstring {
public:
    sstring() : myString(0) { };
    sstring(const char *o);
    sstring(const sstring&);
    ~sstring();

    int operator==(const sstring& o) const;
    int operator==(const char *o) const;

    sstring& operator=(const sstring& o);

    const char *c_str() const { return myString; };

private:
    const char *myString;
};

#else
#include <string>
#define sstring string
#endif

#endif // __MIO_SIMPLE_STRING_H__
