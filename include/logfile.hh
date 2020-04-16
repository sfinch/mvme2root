#ifndef logfile_h
#define logfile_h 1

#include "TString.h"
#include "TDatime.h"

class logfile 
{
  public:
  
    logfile(TString name);
   ~logfile();

  public:
     
    int readLog();

    //setters
  
  private:
     
    TString filename;
    
    //values from messages.log
    TDatime start_time;
    TDatime stop_time;

};

#endif

