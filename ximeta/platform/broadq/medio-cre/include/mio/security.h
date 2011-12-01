
#ifndef __MIO_SECURITY_H__
#define __MIO_SECURITY_H__
#include <string>
#include <list>
using namespace std;

class MioSecurity {
public:
    enum SignedTrust { IGNORE_SIGNED, SIGNED_GOOD };
    static void initialize();
    static void addTrustedPath(const string& path);

    static int isTrustedFile(const string& completePath, SignedTrust signedMeansTrust = IGNORE_SIGNED);
    static string findTrustedFile(const string& fileSpec, const string& pathsList, SignedTrust signedMeansTrust = IGNORE_SIGNED, char pathComponentSep = ':');

    static int isSignedFile(const string& path);

private:
    MioSecurity();
    static MioSecurity *ourSelf;
    list<string> myTrustedPaths;
};

#endif // __MIO_SECURITY_H__
