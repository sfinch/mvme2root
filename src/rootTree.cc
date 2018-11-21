
#include "rootTree.hh"

#include "TFile.h"
#include "TTree.h"
#include "TString.h"

#include <stdlib.h>
#include <iostream>
#include <fstream>
using std::cout;
using std::endl;

rootTree::rootTree(TString name)
{
    filename = name;
    rootfile = new TFile(filename, "RECREATE");
    roottree = new TTree("MDPP16", "MDPP16 data");

    roottree->Branch("ADC", &ADC, "ADC[16]/I");
    roottree->Branch("TDC", &TDC, "TDC[16]/I");
    roottree->Branch("time", &time);
    roottree->Branch("extendedtime", &extendedtime);
    roottree->Branch("pileup", &pileup);
    roottree->Branch("overflow", &overflow);

}

rootTree::~rootTree()
{
    //delete rootfile;
    //delete roottree;
}

void rootTree::initEvent()
{
    for (int i=0; i<num_chn; i++){
        ADC[i] = 0;
        TDC[i] = 0;
    }
    time = 0;
    extendedtime = 0;
    pileup = 0;
    overflow = 0;
      
}

void rootTree::writeEvent()
{
    roottree->Fill();
}

void rootTree::writeTree()
{
    roottree->Write();
    rootfile->Close();
}


void rootTree::printValues()
{

    cout << "Chn \t ADC \t TDC" << endl;
    for (int i=0; i<num_chn; i++){
        cout << i << "\t" << ADC[i] << "\t" << TDC[i] << endl;
    }
    cout << "Time:\t" <<time << endl;
    cout << "Extended time:\t" << extendedtime << endl;
    cout << "Pileup flag:\t" << pileup << endl;
    cout << "Overflow/underflow flag:\t" << overflow << endl;

}

void rootTree::setADC(int chn, int value){ 
    if (chn<num_chn){
        ADC[chn] = value; 
    }
    else if(chn<2*num_chn){
        TDC[chn%num_chn] = value;
    }
    else{
        ADC[chn%num_chn] = value; 
    }
}

void rootTree::setTDC(int chn, int value){
    TDC[chn%num_chn] = value; 
}

void rootTree::setTime(int value){
    time = value;
}

void rootTree::setExtendedTime(int value){
    extendedtime = value;
}

void rootTree::setPileup(int value){
    pileup = value;
}

void rootTree::setOverflow(int value){
    overflow = value;
}
