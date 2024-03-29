#ifndef mdpp16_SCP_h
#define mdpp16_SCP_h 1

#include "TTree.h"
#include "TString.h"
#include "TH1F.h"
#include "TDatime.h"
#include "TVectorD.h"

class mdpp16_SCP
{
  public:
  
    mdpp16_SCP(TString name);
   ~mdpp16_SCP();

  public:
     
    void initEvent();   //call at start of event
    void printValues();
    void writeEvent();  //call at end of event
    void writeTree();   //call at end of file
    void writeHistos();   //call at end of file

    int readAnalysis();

    //setters
    void setADC(int chn, int value);
    void setTDC(int chn, int value);
    void setTrigger(int chn, int value);
    void setTime(int value);
    void setExtendedTime(int value);
    void setPileup(int chn, bool value);
    void setOverflow(int chn, bool value);
  
  private:
     
    static const int num_chn = 16;
    static const int num_trigger = 2;

    TTree *roottree;

    TString filename;
    
    //values from MDPP-16
    int ADC[num_chn];
    int TDC[num_chn];
    int Trigger[num_trigger];
    int time_stamp;
    int extendedtime;
    bool pileup[num_chn];
    bool overflow[num_chn];

    //values from analysis.analysis (energy calibration)
    TVectorD m;
    TVectorD b;
    double min[num_chn], max[num_chn];

    //calculated  values
    int lasttime;       //time stamp of last event
    bool extendedON;    //0 extended time stamp off,
                        //1 extended time stamp on
    double seconds;     //seconds since start of run

    //Projected histograms
    TH1F *hADC[num_chn];
    TH1F *hTDC[num_chn];
    TH1F *hEn[num_chn];
    
};

#endif

