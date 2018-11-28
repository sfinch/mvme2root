
#include "rootTree.hh"

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

rootTree::rootTree(TString name)
{
    //create root file and tre
    filename = name;
    rootfilename = name.ReplaceAll("mvmelst","root");
    rootfile = new TFile(rootfilename, "RECREATE");
    roottree = new TTree("MDPP16", "MDPP16 data");

    roottree->Branch("ADC[16]", &ADC, "ADC[16]/I");
    roottree->Branch("TDC[16]", &TDC, "TDC[16]/I");
    roottree->Branch("time", &time);
    roottree->Branch("extendedtime", &extendedtime);
    roottree->Branch("pileup", &pileup);
    roottree->Branch("overflow", &overflow);
    roottree->Branch("seconds", &seconds);

    //initialize variables
    extendedON = 0;
    time = 0;
    extendedtime = 0;
    for (int i=0; i<num_chn; i++){
        b[i] = 0;
        m[i] = 1;
        min[i] = 0;
        max[i] = 16*4096;
    }
    start_time = TDatime();
    stop_time = TDatime();
    initEvent();

    readLog();
    readAnalysis();

    //create histograms
    for (int i=0; i<num_chn; i++){
        hADC[i] = new TH1F(Form("hADC%i", i), Form("hADC%i", i), 16*4096, 0, 16*4096);
        hTDC[i] = new TH1F(Form("hTDC%i", i), Form("hTDC%i", i), 16*4096, 0, 16*4096);
        hEn[i]  = new TH1F(Form("hEn%i", i),  Form("hEn%i", i),  16*4096, min[i], max[i]);
    }

    
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
    
    TNamed startT("start_time",start_time.AsSQLString());
    TNamed stopT("stop_time",stop_time.AsSQLString());
    startT.Write();
    stopT.Write();

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


int rootTree::readLog(){

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

int rootTree::readAnalysis(){
    
    //variables
    char line[200];
    TString sLine;

    //open analysis.analysis
    TString analysis_filename = filename;
    int index = analysis_filename.Last('/');
    analysis_filename.Remove(index+1,analysis_filename.Sizeof());
    analysis_filename.Append("analysis.analysis");
    std::ifstream infile(analysis_filename.Data());
    if (!infile.is_open())
    {
        cerr << "Error opening " << analysis_filename.Data() 
             << " for reading" << endl;
        return 1;
    }
    else{
        cout << "Found " << analysis_filename.Data() << endl;
    }
    
    //read file
    do{
        infile.getline(line,200);
        sLine = line;
        //search for amplitude calibration
        if (sLine.Contains("\"mdpp16.amplitude_raw\"")){
            //forward to calibrations 
            int maxitr = 100;
            int itr = 0;
            do{
                infile.getline(line,200);
                sLine = line;
                itr++;
            }while((itr<maxitr)&&(!(sLine.Contains("\"calibrations\":"))));
            //get calibration for each chn
            for (int i=0; i<num_chn; i++){

                for (int j=0; j<4; j++){
                    infile.getline(line,200); 
                    sLine = line;
                    sLine.ReplaceAll(" ","");
                    sLine.ReplaceAll("\"","");
                    sLine.ReplaceAll(":","");
                    sLine.ReplaceAll(",","");

                    if (sLine.Contains("unitMax")){
                        sLine.ReplaceAll("unitMax","");
                        max[i] = sLine.Atof();
                    }
                    else if (sLine.Contains("unitMin")){
                        sLine.ReplaceAll("unitMin","");
                        min[i] = sLine.Atof();
                    }
                }
            }
        }
    }while(!(infile.eof()));

    //linear calibration parameters
    for (int i=0; i<num_chn; i++){
        b[i] = min[i];
        m[i] = (max[i]-min[i])/65536.;
    }

    return 0;
}


//setters
void rootTree::setADC(int chn, int value){ 
    if (chn<num_chn){
        ADC[chn] = value; 
        hADC[chn%num_chn]->AddBinContent(value);
        hEn[chn%num_chn]->AddBinContent(value);
    }
    else if(chn<2*num_chn){
        TDC[chn%num_chn] = value;
        hTDC[chn%num_chn]->AddBinContent(value);
    }
    else{
        ADC[chn%num_chn] = value; 
        hADC[chn%num_chn]->AddBinContent(value);
        hEn[chn%num_chn]->AddBinContent(value);
    }
}

void rootTree::setTDC(int chn, int value){
    TDC[chn%num_chn] = value; 
    hTDC[chn%num_chn]->AddBinContent(value);
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

