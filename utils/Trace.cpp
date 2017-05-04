/**
 * \file    Trace.cpp
 *
 * \author  Peter Lidbjork
 *
 * Copyright &copy; Maquet Critical Care AB, Sweden
 *
 ******************************************************************************/

#include "Trace.hpp"

#ifdef USE_TRACE 


//#include "ErrorMacros.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <cstdint>

#include <thread>

// #include <QThread>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#define CHECK(a)


static std::vector<std::ofstream> logFiles_;


std::vector<Trace::Context*> Trace::contexts_;

std::map<std::string, Trace::Configuration*> Trace::configMap_;

std::mutex Trace::mutex_;

std::ostream* Trace::m_logStream = nullptr;

FILE* Trace::logFile_ = stderr;
//std::ofstream Trace::logFile_;

// int Trace::udpPort_ = -1;
// bool Trace::udpActive_ = false;
std::atomic<bool> Trace::s_disabled(false);

//QTime Trace::timeElapsedStart_ ;
boost::timer Trace::timeElapsedStart_ ;

static const std::string entrySymbol = ">";
static const std::string exitSymbol = "<";

static std::atomic<unsigned long>s_rowNumber{0};

// Option constants
#define OPT_NO_OPTIONS 0x0
#define OPT_FILE_NAME 0x1
#define OPT_LINE_NUMBER 0x2
#define OPT_EXECUTION_TIME 0x4
#define OPT_THREAD_ID 0x8
#define OPT_THREAD_NAME 0x10
#define OPT_STRINGS 0x20
#define OPT_NESTING 0x40
#define OPT_DATE_TIME 0x80
#define OPT_CHECK 0x100
#define OPT_FUNC_NAME 0x200
#define OPT_ROW_NUMBER 0x400
#define OPT_TIME_ELAPSED 0x800

// 'f'
#define PRINT_FILE_NAME(a) (a & OPT_FILE_NAME)

// 'l'
#define PRINT_LINE_NUMBER(a) (a & OPT_LINE_NUMBER)

// 'm'
#define PRINT_EXECUTION_TIME(a) (a & OPT_EXECUTION_TIME)

// 'i'
#define PRINT_THREAD_ID(a) (a & OPT_THREAD_ID)

// 'n'
#define PRINT_THREAD_NAME(a) (a & OPT_THREAD_NAME)

// 'p'
#define PRINT_STRINGS(a) (a & OPT_STRINGS)

// 't'
#define PRINT_NESTING(a) (a & OPT_NESTING)

// 'd'
#define PRINT_DATE_TIME(a) (a & OPT_DATE_TIME)

// 'c'
#define PRINT_CHECK(a) (a & OPT_CHECK)

// 'a'
#define PRINT_FUNC_NAME(a) (a & OPT_FUNC_NAME)

// 'r'
#define PRINT_ROW_NUMBER(a) (a & OPT_ROW_NUMBER)

// 'T'
#define PRINT_TIME_ELAPSED(a) (a & OPT_TIME_ELAPSED)

#define NO_PRINT(a) (a == 0)

static char s_argBuffer[256];

Trace::Trace(const std::string& func, const std::string& file, const int line):
    funcName_(func),
    fileName_(file),
    line_(line),
    exitLine_(-1)
{
    if (s_disabled) return;


    Context* ct = context();

    if (ct != 0) {
        const options_t opt = ct->conf->options;
  
        if (PRINT_EXECUTION_TIME(opt)){
            timer_.restart();
        }

        if (PRINT_NESTING(opt)) {
            traceOut((const Context*) ct, entrySymbol, funcName_, "", fileName_, line_);
        }
        ct->nestingLevel++;
    }
}

Trace::~Trace()
{
    if (s_disabled) return;

    Context* ct = context();

    if (ct != 0) {
        ct->nestingLevel--;
        const options_t opt = ct->conf->options;
        if (!PRINT_NESTING(opt)){
            return;
        }
        if (PRINT_EXECUTION_TIME(opt)) {
            traceOut((const Context*) ct, exitSymbol, funcName_, "", fileName_, exitLine_, timer_.elapsed());
        } else {
            traceOut((const Context*) ct, exitSymbol, funcName_, "",fileName_, exitLine_);
        }
    }
}


void Trace::setName(const std::string& name)
{
    Context* c = Trace::context();
    if (c != nullptr)
    {
        c->conf->name=name;
    }
}

void Trace::setOptions(options_t options)
{
    Context* c = Trace::context();
    if (c != nullptr)
    {
        c->conf->options=options;
    }    
}

void Trace::setSimpleSearchStr(const std::string& str)
{
    Context* c = context();
    if (c != 0) {
        c->conf->simpleSearchStr = str;
    }
}

void Trace::setRegExpStr(const std::string& re)
{
    Context* c = context();
    if (c != 0) {
        c->conf->regexpStr = re;
    }
}

void Trace::setPrompt(const std::string& p)
{
    Context* c = context();
    if (c != 0) {
        c->conf->prompt = p;
    }
}

void Trace::setLogFile(const std::string& fileName, bool overWrite)
{
    if (fileName == "stdout")
	{
        logFile_ = stdout;
    } 
	else if (fileName == "stderr") 
	{
        logFile_ = stderr;
    } 
	else if (fileName.length() > 0) 
	{
        const char* fName = fileName.c_str();
        FILE* tmpPtr = 0;
        if (overWrite) {
            tmpPtr = fopen(fName, "w");
        } else {
            tmpPtr = fopen(fName, "a");
        }
        if (tmpPtr == 0) {
            fprintf(stderr, "File %s, line %d: fopen failed for %s: %s", __FILE__, __LINE__, fName, strerror(errno));
            return;
        }
        logFile_ = tmpPtr;
    }
}

void Trace::setLogFile(FILE* fp)
{
    if (fp == 0) {
        fprintf(stderr, "File %s, line %d: NULL file pointer!\n", __FILE__, __LINE__);
        return;
    } else if (fp == stdout || fp == stderr) {
        return;
    }

    // Attempt to get file position to verify that file is open.
    fpos_t pos;
    if (fgetpos (fp, &pos) < 0) {
        fprintf(stderr, "File %s, line %d: Failed to get file position (%s), is this a valid file pointer?", __FILE__, __LINE__, strerror(errno));
        return;
    }
    logFile_ = fp;
}

void Trace::closeLogFile()
{
    if (logFile_ != stderr && logFile_ != stdout) {
        (void) fclose(logFile_);
    }
}

void Trace::out(const int line)
{
    exitLine_ = line;
}

void Trace::printState(const std::string& keyword, const char* file, int line, char* args)
{
    if (s_disabled) return;

    const Context* ct = context();
    if  (ct != 0) {
        if (PRINT_STRINGS(ct->conf->options)) {
            const std::string& simpstr = ct->conf->simpleSearchStr;
            const std::string& regxp = ct->conf->regexpStr;
            bool printString = false;
            if (keyword.empty() || (simpstr.empty() && regxp.empty())) {
                printString = true;
            } else if (simpstr == keyword) {
                printString = true;
            } else if (!regxp.empty()) {
                /*
                QRegExp re(regxp);
                if (re.indexIn(keyword) != -1){
                    printString = true;
                }
                */
            }
            if (printString) {
                traceOut(ct, " ", funcName_, args, file, line);
            }
        }
    }
}

char*  Trace::printArgs(const char *format, ...)
{
    Context* ct = context();
    if (ct && !NO_PRINT(ct->conf->options)){
        va_list args;
        va_start(args, format);
        (void) vsprintf(s_argBuffer, format, args);
        va_end(args);
    } else {
        s_argBuffer[0] = '\0';
    }
    return s_argBuffer;
}

Trace::Context* Trace::context()
{
    // Is the context for this thread already created?
    const std::thread::id tId= std::this_thread::get_id();
    for(size_t i=0; i<contexts_.size(); ++i){
        Context* c = contexts_[i];
        if (c->threadId == tId){
            return c;
        }
    }
    return nullptr;
}

void Trace::traceOut(const Context* ct, const std::string& extra, const std::string& funcName, const std::string& args, const std::string& fileName, int lineNo, double ms) // Construct string based on options.
{
    std::ostream* s = ct->logStream_;

    CHECK(ct != 0);
    const Configuration* conf = ct->conf;
    const options_t opt = conf->options;

   if (NO_PRINT(opt))
        return;

    if (PRINT_ROW_NUMBER(opt)) {
        char rownumstr[16];
        sprintf(rownumstr, "#%08ld:  ", s_rowNumber++);
        *s << std::string(rownumstr);
    }

    if (PRINT_THREAD_ID(opt)){
        //const uintptr_t number = (uintptr_t)ct->threadId;  
        //        s += '(' + QString::number(ct->threadId) + ')';
        *s << '(' << ct->threadId << ')';
    }

    *s << ct->conf->prompt;
    if (conf->regexpStr.length() > 0) {
        *s << " \"" + conf->regexpStr + "\" ";
    }

    if (PRINT_NESTING(opt)) { // Print nesting level.
        for(int i=0; i<ct->nestingLevel; i++) {
            *s << "| ";
        }
    }

    *s << extra;

    if (extra == entrySymbol || PRINT_FUNC_NAME(opt)){ // Always print function name at entry and exit
        *s << funcName;
        *s << ": ";
    } else if (extra == exitSymbol) {
        *s << funcName;
        *s << " ";
    }

    *s << args;

    if (PRINT_FILE_NAME(opt)){
        *s << " File:" << fileName;
    }
    if (lineNo != -1 && PRINT_LINE_NUMBER(opt)){
        *s << " Line:" << lineNo;
    }
/*
    if (PRINT_THREAD_ID(opt)){
        //const uintptr_t number = (uintptr_t)ct->threadId;  
        //        s += '(' + QString::number(ct->threadId) + ')';
        s << '(' << ct->threadId << ')';
    }
    */
    if (PRINT_EXECUTION_TIME(opt) && ms != -1){
        *s << " T: " << ms << " ms";
    }
//    if (PRINT_DATE_TIME(opt)) {
  //      s << QDateTime::currentDateTime().toString(" yyyy-MM-d, hh:mm:ss.zzz");
 //   }
    if (PRINT_TIME_ELAPSED(opt)) { // TBD: Compensate for wrap-around at 23:59:59.999 -> 00:00:00.0
        int ms = timeElapsedStart_.elapsed();
        const int hours = ms / (60*60*1000);
        ms -= hours * (60*60*1000);
        const int minutes = ms / (60*1000) ;
        ms -= minutes * (60*1000);
        const int seconds = ms / (1000);
        ms -= seconds*1000;
        *s << " T:" << hours << ":" << minutes << ":" << seconds << "."  << ms;
    }
    *s << std::endl; 
}

void Trace::profTimerStart(int lineNo)
{
    if (s_disabled) return;
    const Context* ct = context();
    if (ct != 0) {
        profTime_.restart();
        traceOut(ct, " ", funcName_, "PTime started", fileName_, lineNo);
    }
}

void Trace::profTimerElapsed(int lineNo)
{
    if (s_disabled) return;
    Context* ct = context();
    if (ct != 0) {

        // Check if PRINT_EXECUTION_TIME is defined. If not, we define it temporarily.
        const bool prExecTime = static_cast<bool>(PRINT_EXECUTION_TIME(ct->conf->options));
        if (!prExecTime) {
            ct->conf->options |= OPT_EXECUTION_TIME;
        }
        traceOut((const Context*) ct, " ", funcName_, "PTime elapsed", fileName_, lineNo, profTime_.elapsed());
        // If PRINT_EXECUTION_TIME wasn't defined, we clear it.
        if (!prExecTime) {
            ct->conf->options &= ~OPT_EXECUTION_TIME;
        }
    }
}


void Trace::check(const char* expression, bool result, int lineNo)
{
    if (s_disabled) return;
    const Context* ct = context();
    if (ct != 0) {
        if (PRINT_STRINGS(ct->conf->options)) {
            std::string s(expression);
            s += " : ";
            s += result ? "true" : "false";
            traceOut((const Context*) ct, " ", funcName_, s, fileName_, lineNo);
        }
    }
}

void Trace::compare(const char* first, const char* second, int firstVal, int secondVal, int lineNo)
{
    /*
    compareHelper(first, second, (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)),
                  lineNo, QString::number(firstVal), QString::number(secondVal));
                  */
}

void Trace::compare(const char* first, const char* second, unsigned int firstVal, unsigned int secondVal, int lineNo)
{
    compareHelper(first, second, (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)), lineNo);
}
void Trace::compare(const char* first, const char* second, float firstVal, float secondVal, int lineNo)
{
    compareHelper(first, second,  (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)), lineNo);
}
void Trace::compare(const char* first, const char* second, double firstVal, double secondVal, int lineNo)
{
    compareHelper(first, second, (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)), lineNo);
}
void Trace::compare(const char* first, const char* second, char firstVal, char secondVal, int lineNo)
{
    compareHelper(first, second, (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)), lineNo);
}
void Trace::compare(const char* first, const char* second, unsigned char firstVal, unsigned char secondVal, int lineNo)
{
    compareHelper(first, second, (firstVal < secondVal ? -1 : (firstVal == secondVal ? 0 : 1)), lineNo);
}

void Trace::compareHelper(const char* first, const char* second, int result, int lineNo, const std::string& valStr1, const std::string& valStr2)
{
    const Context* ct = context();
    if (ct != nullptr) {
        if (PRINT_STRINGS(ct->conf->options)) {
            std::string s;
            const std::string s1 = std::string(first) + "{" + valStr1 + "}";
            const std::string s2 = std::string(second) + "{" + valStr2 + "}";

            if (result > 0) {
                s = s1 + " > " + s2;
            } else if (result < 0) {
                s = s1 + " < " + s2;
            } else {
                s = s1 + " == " + s2;
            }
            traceOut((const Context*) ct, " ", funcName_, s, fileName_, lineNo);
        }
    }
}

/*****************************************************************************************
*
*  Conceptual hiearchy 
*  "trace" -- application 1 -- thread 1 -- Configuration -- options 
*                                                     -- simple search string
*                                                     -- regexp
*                                                     -- prompt
*
*                           -- thread 2  -- Configuration -- options
*                                                     -- simple search string
*                                                     -- regexp
*                                                     -- prompt
*  
*          -- application 2 -- thread 1 -- Configuration -- options 
*                                                     -- simple search string
*                                                     -- regexp
*                                                     -- prompt
*
*
*******************************************************************************************/

bool Trace::readConfig(const std::string& appName, const std::string& pathToConfigFile)
{
    namespace pt = boost::property_tree;
    pt::ptree conf;

    try
    {
        pt::read_json(pathToConfigFile, conf);
        //std::size_t appcnt = conf.count(appName);
        pt::ptree subconf = conf.get_child(appName);

        BOOST_FOREACH(const pt::ptree::value_type &v, conf.get_child(appName)) 
        {
            // v.first is the name of the child.
            // v.second is the child tree.
            const pt::ptree subTree = v.second;
            if (v.first == "thr")
            {    
                Configuration* c = new Configuration;
                try{
                    c->name = subTree.get<std::string>("name");
                    c->options=parseOptions(subTree.get<std::string>("options"));
                    c->simpleSearchStr=subTree.get<std::string>("searchStr");
                    c->regexpStr=subTree.get<std::string>("regexp");
                    c->prompt=subTree.get<std::string>("prompt");
                    pt::ptree logfile = subTree.get_child("logfile");
                    c->logFileName_ = logfile.get<std::string>("name");
                    c->logFileMode_ = logfile.get<std::string>("mode");
                    configMap_[c->name] = c;

                } catch(std::exception& e) {
                    delete c;
                    std::cerr << e.what() << std::endl;
                    return false;
                }
            } 
        }
    }catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return false;
    }
    return true;
}

Trace::options_t Trace::parseOptions(const std::string& o)
{
	options_t options = OPT_NO_OPTIONS;
	
	if (boost::algorithm::contains(o,"f")) {
		options += OPT_FILE_NAME;
    }
	if (boost::algorithm::contains(o,"l")) {
		options += OPT_LINE_NUMBER;
    }
	if (boost::algorithm::contains(o,"m")){
		options += OPT_EXECUTION_TIME;
    }
	if (boost::algorithm::contains(o,"i")){
		options += OPT_THREAD_ID;
    }
	if (boost::algorithm::contains(o,"n")){
		options += OPT_THREAD_NAME;
    }
	if (boost::algorithm::contains(o,"p")){
		options += OPT_STRINGS;
    }
	if (boost::algorithm::contains(o,"t")){
		options += OPT_NESTING;
    }
	if (boost::algorithm::contains(o,"d")){
		options += OPT_DATE_TIME;
    }
	if (boost::algorithm::contains(o,"c")){
		options += OPT_CHECK;
    }
	if (boost::algorithm::contains(o,"a")){
		options += OPT_FUNC_NAME;
    }
	if (boost::algorithm::contains(o,"r")){
		options += OPT_ROW_NUMBER;
    }
	if (boost::algorithm::contains(o,"T")){
		options += OPT_TIME_ELAPSED;
    }
	return options;
}

void Trace::createContext(const std::string& name, const std::string& opts)
{
	if (s_disabled) return;

    std::lock_guard<std::mutex> lock(mutex_);
	Context* ct = new Context();
	ct->threadId = std::this_thread::get_id();
	ct->nestingLevel = 1;
	contexts_.push_back(ct);

	// Check if configuration is already read from json file, otherwise create empty configuration.
	Configuration* c = 0;
	if (configMap_.find(name) != configMap_.end()){
		c = configMap_[name];       
	} else {
		c = new Configuration;
		c->options = parseOptions(opts);
	}
	ct->conf = c;
    setLogStream(*ct);
}
/*
void Trace::disable(const std::string& file, const int line)
{
	s_disabled = true;
    std::stringstream ss;
	ss << "Trace disabled in file " << file << " at line " << line;
	output(ss.str());
	fflush(logFile_);
}
*/
void Trace::setTimeElapsedStart()
{
	timeElapsedStart_.restart();
}

void Trace::flush()
{
	fflush(logFile_);
}

void Trace::setLogStream(Trace::Context& c)
{
    try
    {
        if (c.logFile_.is_open()) {
            c.logFile_.close();
        }
        if(!c.conf->logFileName_.empty())
        {
            std::ios_base::openmode mode = std::ios_base::out;
            if (c.conf->logFileMode_ == "a") {
                mode = std::ios_base::app;
            }
            c.logFile_.open(c.conf->logFileName_, mode);
            c.logStream_ = &c.logFile_;
        } 
        else
        {
            c.logStream_ = &std::cout;  
            std::cout << "BEPA" << std::endl;
        }
    } catch(std::exception& e)
    {
        std::cerr << "Failed to open " << c.conf->logFileName_ << ":" << e.what() << std::endl;
    }
}

std::ostream& operator<<(std::ostream& os, const Trace::Configuration& c)
{
    os << "name=" << c.name << "&options=" << std::hex << c.options <<"&prompt=" << c.prompt << "&simpleSearchStr=" 
        << c.simpleSearchStr << "&regexpStr=" << c.regexpStr << "&logfileName=" << c.logFileName_ << "&logFileMode=" << c.logFileMode_;
    return os;
}

std::ostream& operator<<(std::ostream& os, const Trace::Context& c)
{
    os << "threadId=" << c.threadId << "&nestingLevel=" << c.nestingLevel << *c.conf; 
    return os;
}

/*
bool Trace::activateUdp()
{
	bool success = false;
	if (udpActive_ && (!hostAddress_.isNull()) && udpPort_ != -1)
	{
		success = socket_.bind(hostAddress_, udpPort_);
	}
	return success;
}
*/
#endif
