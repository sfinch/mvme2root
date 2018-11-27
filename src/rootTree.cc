
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
    //create root file and tre
    filename = name;
    rootfile = new TFile(filename, "RECREATE");
    roottree = new TTree("MDPP16", "MDPP16 data");

    roottree->Branch("ADC[16]", &ADC, "ADC[16]/I");
    roottree->Branch("TDC[16]", &TDC, "TDC[16]/I");
    roottree->Branch("time", &time);
    roottree->Branch("extendedtime", &extendedtime);
    roottree->Branch("pileup", &pileup);
    roottree->Branch("overflow", &overflow);
    roottree->Branch("seconds", &seconds);

    //create histograms
    for (int i=0; i<num_chn; i++){
        hADC[i] = new TH1F(Form("hADC%i", i), Form("hADC%i", i), 16*4096, 0, 16*4096);
        hTDC[i] = new TH1F(Form("hTDC%i", i), Form("hTDC%i", i), 16*4096, 0, 16*4096);
        hEn[i]  = new TH1F(Form("hEn%i", i),  Form("hEn%i", i),  16*4096, 0, 16*4096);
    }
    
    //initialize variables
    extendedON = 0;
    time = 0;
    extendedtime = 0;
    initEvent();

}

rootTree::~rootTree()
{
    
}

void rootTree::initEvent()
{
    //call at start of event
    lasttime = time;
    time = 0;
    for (int i=0; i<num_chn; i++){
        ADC[i] = 0;
        TDC[i] = 0;
    }
    pileup = 0;
    overflow = 0;
    seconds = 0;
}

void rootTree::writeEvent()
{
    //call at end of event
    if ((time<lasttime)&&(extendedON==0))
        extendedtime++;
    seconds = extendedtime*67.108864 + time/16000000.;
    roottree->Fill();
}

void rootTree::writeTree()
{
    //call at end of file
    rootfile->cd();
    roottree->Write();

    rootfile->mkdir("histos");
    rootfile->cd("histos");
    for (int i=0; i<num_chn; i++){
        hADC[i]->Write();
        hTDC[i]->Write();
        hEn[i]->Write();
    }

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

//setters
void rootTree::setADC(int chn, int value){ 
    if (chn<num_chn){
        ADC[chn] = value; 
        hADC[chn%num_chn]->Fill(value);
    }
    else if(chn<2*num_chn){
        TDC[chn%num_chn] = value;
        hTDC[chn%num_chn]->Fill(value);
    }
    else{
        ADC[chn%num_chn] = value; 
        hADC[chn%num_chn]->Fill(value);
    }
}

void rootTree::setTDC(int chn, int value){
    TDC[chn%num_chn] = value; 
    hTDC[chn%num_chn]->Fill(value);
}

void rootTree::setTime(int value){
    time = value;
}

void rootTree::setExtendedTime(int value){
    extendedON = 1;
    extendedtime = value; }

void rootTree::setPileup(int value){
    pileup = value;
}

void rootTree::setOverflow(int value){
    overflow = value;
}

