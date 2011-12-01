
#ifndef __MIO_MESSAGE_FACET_H__
#define __MIO_MESSAGE_FACET_H__
#include <locale>
#include <mio/sstring.h>
using namespace std;

class MyMessages : public std::locale::facet {
public:
    static std::locale::id id;

    explicit MyMessages();
    sstring xlate(const sstring& module, const sstring& id, const char *format) const;

private:
    MyMessages(MyMessages&);        // not defined
    void operator=(MyMessages&);    // not defined
};

#endif // __MIO_MESSAGE_FACET_H__
