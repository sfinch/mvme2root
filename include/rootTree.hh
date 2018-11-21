#ifndef rootTree_h
#define rootTree_h 1

#include "TFile.h"
#include "TTree.h"
#include "TString.h"

class rootTree
{
  public:
  
    rootTree(TString name);
   ~rootTree();

  public:
     
    void initEvent();
    void printValues();
    void writeEvent();
    void writeTree();

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
    
    int ADC[num_chn];
    int TDC[num_chn];
    int time;
    int extendedtime;
    int pileup;
    int overflow;
      
  private:
    
};



#endif

