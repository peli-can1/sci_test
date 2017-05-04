
#include "GetOpt.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>

GetOpt::GetOpt() :
    opterr(1),
    optopt(63),
    optind(1),
    optarg(nullptr),
    optreset(0)
{

}

int GetOpt::getopt(int nargc, char * const nargv[], const char *ostr)
{
   	char BADCH[2];
    strcpy(BADCH, "?");
    char BADARG[2];
    strcpy(BADARG, ";");    
    char EMSG[2];
    strcpy(EMSG, "");

    //static char *place = EMSG;              /* option letter processing */
    char *place = EMSG;
    const char *oli;                        /* option letter list index */

    if (this->optreset || !*place) {              /* update scanning pointer */
        this->optreset = 0;
        if (this->optind >= nargc || *(place = nargv[this->optind]) != '-') {
            place = EMSG;
            return (-1);
        }
        if (place[1] && *++place == '-') {      /* found "--" */
            ++this->optind;
            place = EMSG;
            return (-1);
        }
    }                                       /* option letter okay? */
    if ((this->optopt = (int)*place++) == (int)':' ||
        !(oli = strchr(ostr, this->optopt))) {
        /*
        * if the user didn't specify '-' as an option,
        * assume it means -1.
        */
        if (this->optopt == (int)'-')
            return (-1);
        if (!*place)
            ++this->optind;
        if (this->opterr && *ostr != ':')
            (void)printf("illegal option -- %c\n", this->optopt);
        return (BADCH[0]);
    }
    if (*++oli != ':') {                    /* don't need argument */
        this->optarg = NULL;
        if (!*place)
            ++this->optind;
    }
    else {                                  /* need an argument */
        if (*place)                     /* no white space */
            this->optarg = place;
        else if (nargc <= ++this->optind) {   /* no arg */
            place = EMSG;
            if (*ostr == ':')
                return (BADARG[0]);
            if (this->opterr)
                (void)printf("option requires an argument -- %c\n", this->optopt);
            return (BADCH[0]);
        }
        else                            /* white space */
            this->optarg = nargv[this->optind];
        place = EMSG;
        ++this->optind;
    }
    return (this->optopt);                        /* dump back option letter */
}
