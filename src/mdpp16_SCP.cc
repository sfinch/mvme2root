
#include "mdpp16_SCP.hh"

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

mdpp16_SCP::mdpp16_SCP(TString name)
{
    //create root file and tre
    filename = name;
    roottree = new TTree("MDPP16_SCP", "MDPP16 data");

    roottree->Branch(Form("ADC[%i]", num_chn), &ADC, Form("ADC[%i]/I", num_chn));
    roottree->Branch(Form("TDC[%i]", num_chn), &TDC, Form("TDC[%i]/I", num_chn));
    roottree->Branch("time_stamp", &time_stamp);
    roottree->Branch("extendedtime", &extendedtime);
    roottree->Branch(Form("overflow[%i]", num_chn), &overflow, Form("overflow[%i]/O", num_chn));
    roottree->Branch(Form("pileup[%i]", num_chn), &pileup, Form("pileup[%i]/O", num_chn));
    roottree->Branch("seconds", &seconds);

    //initialize variables
    extendedON = 0;
    time_stamp = 0;
    extendedtime = 0;
    b.ResizeTo(num_chn);
    m.ResizeTo(num_chn);
    for (int i=0; i<num_chn; i++){
        b[i] = 0;
        m[i] = 1;
        min[i] = 0;
        max[i] = 16*4096;
    }
    start_time = TDatime();
    stop_time = TDatime();
    initEvent();

    readAnalysis();
    readLog();

    //create histograms
    for (int i=0; i<num_chn; i++){
        hADC[i] = new TH1F(Form("hADC%i", i), Form("hADC%i", i), 16*4096, 0, 16*4096);
        hTDC[i] = new TH1F(Form("hTDC_SCP%i", i), Form("hTDC%i", i), 16*4096, 0, 16*4096);
        hEn[i]  = new TH1F(Form("hEn%i", i),  Form("hEn%i", i),  16*4096, min[i], max[i]);
    }

    
}

mdpp16_SCP::~mdpp16_SCP()
{
    
}

void mdpp16_SCP::initEvent()
{
    //call at start of event
    lasttime = time_stamp;
    time_stamp = 0;
    seconds = 0;
    for (int i=0; i<num_chn; i++){
        ADC[i] = 0;
        TDC[i] = 0;
        pileup[i] = 0;
        overflow[i] = 0;
    }
}

void mdpp16_SCP::writeEvent()
{
    //call at end of event
    if ((time_stamp<lasttime)&&(extendedON==0))
        extendedtime++;
    seconds = extendedtime*67.108864 + time_stamp/16000000.;
    roottree->Fill();
}

void mdpp16_SCP::writeTree()
{
    //call at end of file
    roottree->Write();
    
    TNamed startT("start_time_SCP",start_time.AsSQLString());
    TNamed stopT("stop_time_SCP",stop_time.AsSQLString());
    startT.Write();
    stopT.Write();
    m.Write(Form("m[%i]", num_chn));
    b.Write(Form("b[%i]", num_chn));
}

void mdpp16_SCP::writeHistos()
{
    for (int i=0; i<num_chn; i++){
        hADC[i]->Write();
        hTDC[i]->Write(Form("hTDC%i", i));
        hEn[i]->Write();
    }

}


void mdpp16_SCP::printValues()
{
    
    cout << "Chn \t ADC \t TDC" << endl;
    for (int i=0; i<num_chn; i++){
        cout << "-- Channel " << i << " --" << endl;
        cout << "ADC: " << ADC[i] << "\tTDC: " << TDC[i] << endl;
        cout << "Pileup flag:\t" << pileup[i] << endl;
        cout << "Overflow/underflow flag:\t" << overflow[i] << endl;
    }
    cout << "Time:\t" <<time_stamp << endl;
    cout << "Extended time:\t" << extendedtime << endl;

}


int mdpp16_SCP::readLog(){

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

int mdpp16_SCP::readAnalysis(){
    
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
    } //read file
    bool foundCal = 0;
    do{
        infile.getline(line,200);
        sLine = line;
        //search for amplitude calibration
        if (sLine.Contains("\"analysis::CalibrationMinMax\"")){
            //forward to calibrations 
            int maxitr = 7;
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
                    else if (sLine.Contains("]")){
                        i=num_chn;
                        j=4;
                    }
                }
            }
            itr=0;
            do{
                infile.getline(line,200);
                sLine = line;
                if (sLine.Contains("amplitude")){
                    cout << "Found ADC calibration" << endl;
                    foundCal=1;
                }
                itr++;
            }while((itr<maxitr)&&(!(sLine.Contains("name"))));
        }
    }while(!(foundCal||infile.eof()));

    //linear calibration parameters
    for (int i=0; i<num_chn; i++){
        b[i] = min[i];
        m[i] = (max[i]-min[i])/65536.;
    }

    return 0;
}


//setters
void mdpp16_SCP::setADC(int chn, int value){ 
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

void mdpp16_SCP::setTDC(int chn, int value){
    TDC[chn%num_chn] = value; 
    hTDC[chn%num_chn]->AddBinContent(value);
}

void mdpp16_SCP::setTime(int value){
    time_stamp = value;
}

void mdpp16_SCP::setExtendedTime(int value){
    extendedON = 1;
    extendedtime = value; 
}

void mdpp16_SCP::setPileup(int chn, bool value){
    pileup[chn%num_chn] = value;
}

void mdpp16_SCP::setOverflow(int chn, bool value){
    overflow[chn%num_chn] = value;
}

