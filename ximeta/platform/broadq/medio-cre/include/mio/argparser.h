
#ifndef __MIO_ARG_PARSER_H__
#define __MIO_ARG_PARSER_H__

class MioArgParser {
public:
    MioArgParser(int argc, const char*argv[]);
    ~MioArgParser();

    const char *peek() const;
    const char *get();
    const char *isOption();

private:
    int myNext;
    int myArgc;
    const char **myArgv;
};

#endif // __MIO_ARG_PARSER_H__
