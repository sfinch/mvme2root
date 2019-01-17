
#include "mdpp16_QDC.hh"

#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TDatime.h"

#include <stdlib.h>
#include <cerrno>
#include <iostream>
#include <fstream>
using std::cout;
using std::cerr;
using std::endl;

mdpp16_QDC::mdpp16_QDC(TString name)
{
    //create root file and tre
    filename = name;
    rootfilename = name.ReplaceAll("mvmelst","root");
    rootfilename = name.ReplaceAll("listfiles","data_root");
    cout << "Root file name: " << rootfilename << endl;
    rootfile = new TFile(rootfilename, "RECREATE");
    roottree = new TTree("MDPP16", "MDPP16 data");

    roottree->Branch(Form("ADC_short[%i]", num_chn), &ADC_short, Form("ADC_short[%i]/I", num_chn));
    roottree->Branch(Form("ADC_long[%i]", num_chn), &ADC_long, Form("ADC_long[%i]/I", num_chn));
    roottree->Branch(Form("TDC[%i]", num_chn), &TDC, Form("TDC[%i]/I", num_chn));
    //roottree->Branch(Form("PSD[%i]", num_chn), &PSD, Form("PSD[%i]/I", num_chn));
    roottree->Branch("time_stamp", &time_stamp);
    roottree->Branch("extendedtime", &extendedtime);
    roottree->Branch("overflow", &overflow);
    roottree->Branch("seconds", &seconds);

    //initialize variables
    extendedON = 0;
    time_stamp = 0;
    extendedtime = 0;
    start_time = TDatime();
    stop_time = TDatime();
    initEvent();

    readLog();

    //create histograms
    for (int i=0; i<num_chn; i++){
        hADC_long[i] = new TH1F(Form("hADC_long%i", i), Form("hADC_long%i", i), 4096, 0, 4096);
        hADC_short[i] = new TH1F(Form("hADC_short%i", i), Form("hADC_short%i", i), 4096, 0, 4096);
        hTDC[i] = new TH1F(Form("hTDC%i", i), Form("hTDC%i", i), 16*4096, 0, 16*4096);
        hPSD[i]  = new TH1F(Form("hPSD%i", i), Form("hPSD%i", i), 4096, -4.096, 4.096);
    }

}

mdpp16_QDC::~mdpp16_QDC()
{
    
}

void mdpp16_QDC::initEvent()
{
    //call at start of event
    lasttime = time_stamp;
    time_stamp = 0;
    for (int i=0; i<num_chn; i++){
        ADC_long[i] = 0;
        ADC_short[i] = 0;
        TDC[i] = 0;
        PSD[i] = 0;
    }
    overflow = 0;
    seconds = 0;
}

void mdpp16_QDC::writeEvent()
{
    //call at end of event
    if ((time_stamp<lasttime)&&(extendedON==0))
        extendedtime++;

    //Calculated values
    seconds = extendedtime*67.108864 + time_stamp/16000000.;
    for (int i=0; i<num_chn; i++){
        PSD[i] = (ADC_long[i]-ADC_short[i])*1./(1.*ADC_long[i]);
        hPSD[i]->Fill(PSD[i]);
    }

    //fill tree
    roottree->Fill();
}

void mdpp16_QDC::writeTree()
{
    //call at end of file
    rootfile->cd();
    roottree->Write();
    
    TNamed startT("start_time",start_time.AsSQLString());
    TNamed stopT("stop_time",stop_time.AsSQLString());
    startT.Write();
    stopT.Write();

    rootfile->mkdir("histos");
    rootfile->cd("histos");
    for (int i=0; i<num_chn; i++){
        hADC_short[i]->Write();
        hADC_long[i]->Write();
        hTDC[i]->Write();
        hPSD[i]->Write();
    }

    rootfile->Close();
}


void mdpp16_QDC::printValues()
{
    
    cout << "Chn \t ADC \t TDC" << endl;
    for (int i=0; i<num_chn; i++){
        cout << i << "\t" << ADC_short[i] << "\t" << ADC_long[i] << "\t" << TDC[i] << endl;
    }
    cout << "Time:\t" <<time_stamp << endl;
    cout << "Extended time:\t" << extendedtime << endl;
    cout << "Overflow/underflow flag:\t" << overflow << endl;

}


int mdpp16_QDC::readLog(){

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

    return 0;
}

//setters
void mdpp16_QDC::setADC(int chn, int value){ 
    if (chn<num_chn){
        ADC_long[chn%num_chn] = value; 
        hADC_long[chn%num_chn]->AddBinContent(value);
    }
    else if(chn<2*num_chn){
        TDC[chn%num_chn] = value;
        hTDC[chn%num_chn]->AddBinContent(value);
    }
    else{
        ADC_short[chn%num_chn] = value; 
        hADC_short[chn%num_chn]->AddBinContent(value);
    }
}

void mdpp16_QDC::setADC_short(int chn, int value){ 
    ADC_short[chn%num_chn] = value; 
    hADC_short[chn%num_chn]->AddBinContent(value);
}

void mdpp16_QDC::setADC_long(int chn, int value){ 
    ADC_long[chn%num_chn] = value; 
    hADC_long[chn%num_chn]->AddBinContent(value);
}

void mdpp16_QDC::setTDC(int chn, int value){
    TDC[chn%num_chn] = value; 
    hTDC[chn%num_chn]->AddBinContent(value);
}

void mdpp16_QDC::setPileup(int value){
}

void mdpp16_QDC::setTime(int value){
    time_stamp = value;
}

void mdpp16_QDC::setExtendedTime(int value){
    extendedON = 1;
    extendedtime = value; 
}

void mdpp16_QDC::setOverflow(int value){
    overflow = value;
}

