#ifndef rootTree_h
#define rootTree_h 1

#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TH1F.h"

class rootTree
{
  public:
  
    rootTree(TString name);
   ~rootTree();

  public:
     
    void initEvent();   //call at start of event
    void printValues();
    void writeEvent();  //call at end of event
    void writeTree();`  //call at end of file

    //setters
    void setADC(int chn, int value);
    void setTDC(int chn, int value);
    void setTime(int value);
    void setExtendedTime(int value);
    void setPileup(int value);
    void setOverflow(int value);
  
  private:
     
    static const int num_chn = 16;

    TFile *rootfile;
    TTree *roottree;

    TString filename;
    
    //values from MDPP-16
    int ADC[num_chn];
    int TDC[num_chn];
    int time;
    int extendedtime;
    int pileup;
    int overflow;

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

