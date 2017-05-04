#include "Trace.hpp"
#include "GetOpt.hpp"

#include <iostream>


void test2()
{
	TRACE();
	TRACE_PRINT("", ("Hej babberiba!"));
}

void test3()
{
	TRACE();
	test2();
}

void test1()
{
	TRACE();
	int a=3;
	TRACE_CHECK(a == 3);
	TRACE_COMPARE(a,3);
	test3();
}

int main(int argc, char* argv[])
{
    char c;
    std::string opt;
    GetOpt g;
    std::string configFile;
    while ((c = g.getopt(argc, argv, "#:f:")) != -1)
    {        
        switch (c)
        {
    	case '#':
    	 	TRACE_CREATE_CONTEXT("main",  g.optarg);
            break;
        case 'f':
        	TRACE_READ_CONFIG_FILE("example", g.optarg);
        	break;
        case '?':
        	if (g.optopt == 'c') {
                std::cerr << "Option -`" << g.optopt << "' requires an argument." <<std::endl;
        	}
            else if (isprint(g.optopt)) {
                std::cerr << "Unknown option `-" << g.optopt << "'." << std::endl;
            }
            else {
                std::cerr << "Unknown option character `" << std::hex << g.optopt << "'." << std::endl;
            }
            break;
        default:
            exit(1);
            break;
       }
	}

	TRACE_CREATE_CONTEXT("thread1", "tl");

	test1();

	return 0;
}
