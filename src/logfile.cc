
#include "logfile.hh"

#include "TString.h"
#include "TDatime.h"
#include "TNamed.h"

#include <stdlib.h>
#include <cerrno>
#include <iostream>
#include <fstream>
using std::cout;
using std::cerr;
using std::endl;

logfile::logfile(TString name)
{
    //create root file and tre
    filename = name;

    //initialize variables
    start_time = TDatime();
    stop_time = TDatime();

    readLog();

}

logfile::~logfile()
{
    
}

int logfile::readLog(){

    //variables
    char line[200];
    TString sLine;

    //Open messages.log
    TString log_filename = filename;
    int index = log_filename.Last('/');
    log_filename.Remove(index+1,log_filename.Sizeof());
    log_filename.Append("messages.log");
    std::ifstream infile(log_filename.Data());
    if (!infile.is_open())
    {
        cerr << "Error opening " << log_filename.Data() 
             << " for reading" << endl;
        return 1;
    }
    else{
        cout << "Found " << log_filename.Data() << endl;
    }
    
    //read file and extract start/stop date and time
    do{
        infile.getline(line,200);
        sLine = line;
        if (sLine.Contains("readout starting on")){
            sLine.Remove(0, sLine.Sizeof()-20);
            sLine.ReplaceAll("T"," ");
            start_time = TDatime(sLine.Data());
            cout << "Run Start ";
            start_time.Print();
        }
        else if (sLine.Contains("readout stopped on")){
            sLine.Remove(0, sLine.Sizeof()-20);
            sLine.ReplaceAll("T"," ");
            stop_time = TDatime(sLine.Data());
            cout << "Run Stop  ";
            stop_time.Print();
        }
    }while(!(infile.eof()));

    TNamed startT("start_time_SCP",start_time.AsSQLString());
    TNamed stopT("stop_time_SCP",stop_time.AsSQLString());
    startT.Write();
    stopT.Write();

    return 0;
}
