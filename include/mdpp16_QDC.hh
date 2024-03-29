#ifndef mdpp16_QDC_h
#define mdpp16_QDC_h 1

#include "TTree.h"
#include "TString.h"
#include "TH1F.h"
#include "TDatime.h"
#include "TVectorD.h"

class mdpp16_QDC
{
  public:
  
    mdpp16_QDC(TString name);
   ~mdpp16_QDC();

  public:
     
    void initEvent();   //call at start of event
    void printValues();
    void writeEvent();  //call at end of event
    void writeTree();   //call at end of file
    void writeHistos();   //call at end of file

    //setters
    void setADC(int chn, int value);
    void setADC_short(int chn, int value);
    void setADC_long(int chn, int value);
    void setTDC(int chn, int value);
    void setTime(int value);
    void setExtendedTime(int value);
    void setOverflow(int chn, bool value);
    void setTrigger(int chn, int value);
  
  private:
     
    static const int num_chn = 16;
    static const int num_trigger = 2;

    TTree *roottree;

    TString filename;
    
    //values from MDPP-16
    int ADC_long[num_chn];
    int ADC_short[num_chn];
    int TDC[num_chn];
    int Trigger[num_trigger];
    bool overflow[num_chn];
    int time_stamp;
    int extendedtime;

    //calculated  values
    double PSD[num_chn];
    int lasttime;       //time stamp of last event
    bool extendedON;    //0 extended time stamp off,
                        //1 extended time stamp on
    double seconds;     //seconds since start of run

    //Projected histograms
    TH1F *hADC_short[num_chn];
    TH1F *hADC_long[num_chn];
    TH1F *hPSD[num_chn];
    TH1F *hTDC[num_chn];
    
};

#endif

