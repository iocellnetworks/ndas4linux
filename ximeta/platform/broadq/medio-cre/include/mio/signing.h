
#ifndef __MIO_SIGNING_H__
#define __MIO_SIGNING_H__
#include <string>
#include <mio/iohelper.h>
#include <mp32number.h>
#include <dlkp.h> // keypair
using namespace std;

typedef struct _sig {
    mp32number r;
    mp32number s;
} sig_t;

class MioSignedFile {
public:
    MioSignedFile(const string& filename, MioIOHelper *io);
    ~MioSignedFile();

    int isSigned();
    int isValid() const;

    static int loadMasterKeys(const string& keyfilename);
    static int isSignatureFile(const string& filename);
    static string signatureFilename(const string& filename);

private:
    string myFilename;
    MioIOHelper *myIo;
    sig_t mySig;
    static dlkp_p ourKeypair;
    void sha1Hash(mp32number *hash) const;
    int readSigfd(int fd);
};

#endif  // __MIO_SIGNING_H__
