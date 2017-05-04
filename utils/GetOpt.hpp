#pragma once


class GetOpt
{
public:
    explicit GetOpt();
    int getopt(int nargc, char * const nargv[], const char *ostr);

    int opterr;
    int optopt;
    int optind;
    char * optarg;
    int optreset;
};
