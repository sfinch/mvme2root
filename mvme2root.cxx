/* 
 * mvme2root by Sean Finch. Modified from 
 * mvme-listfile-dumper - format mvme listfile content and print it to stdout
 *
 * Copyright (C) 2017  Florian Lüke <f.lueke@mesytec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "TString.h"
#include "rootTree.hh"
#include "TROOT.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

using std::cout;
using std::cerr;
using std::endl;

/*  ===== VERSION 0 =====
 *
 *  ------- Section (Event) Header ----------
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 * +--------------------------------+
 * |ttt         eeeessssssssssssssss|
 * +--------------------------------+
 *
 * t =  3 bit section type
 * e =  4 bit event type (== event number/index) for event sections
 * s = 16 bit size in units of 32 bit words (fillwords added to data if needed) -> 256k section max size
 *
 * Section size is the number of following 32 bit words not including the header word itself.
* Sections with SectionType_Event contain subevents with the following header:

 *  ------- Subevent (Module) Header --------
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 * +--------------------------------+
 * |              mmmmmm  ssssssssss|
 * +--------------------------------+
 *
 * m =  6 bit module type (VMEModuleType enum from globals.h)
 * s = 10 bit size in units of 32 bit words
 *
 * The last word of each event section is the EndMarker (globals.h)
 *
*/
struct listfile_v0
{
    static const int Version = 0;
    static const int FirstSectionOffset = 0;

    static const int SectionMaxWords  = 0xffff;
    static const int SectionMaxSize   = SectionMaxWords * sizeof(u32);

    static const int SectionTypeMask  = 0xe0000000; // 3 bit section type
    static const int SectionTypeShift = 29;
    static const int SectionSizeMask  = 0xffff;    // 16 bit section size in 32 bit words
    static const int SectionSizeShift = 0;
    static const int EventTypeMask  = 0xf0000;   // 4 bit event type
    static const int EventTypeShift = 16;

    // Subevent containing module data
    static const int ModuleTypeMask  = 0x3f000; // 6 bit module type
    static const int ModuleTypeShift = 12;

    static const int SubEventMaxWords  = 0x3ff;
    static const int SubEventMaxSize   = SubEventMaxWords * sizeof(u32);
    static const int SubEventSizeMask  = 0x3ff; // 10 bit subevent size in 32 bit words
    static const int SubEventSizeShift = 0;
};

/*  ===== VERSION 1 =====
 *
 * Differences to version 0:
 * - Starts with the FourCC "MVME" followed by a 32 bit word containing the
 *   listfile version number.
 * - Larger section and subevent sizes: 16 -> 20 bits for sections and 10 -> 20
 *   bits for subevents.
 * - Module type is now 8 bit instead of 6.
 *
 *  ------- Section (Event) Header ----------
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 * +--------------------------------+
 * |ttteeee     ssssssssssssssssssss|
 * +--------------------------------+
 *
 * t =  3 bit section type
 * e =  4 bit event type (== event number/index) for event sections
 * s = 20 bit size in units of 32 bit words (fillwords added to data if needed) -> 256k section max size
 *
 * Section size is the number of following 32 bit words not including the header word itself.

 * Sections with SectionType_Event contain subevents with the following header:

 *  ------- Subevent (Module) Header --------
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 * +--------------------------------+
 * |mmmmmmmm    ssssssssssssssssssss|
 * +--------------------------------+
 *
 * m =  8 bit module type (VMEModuleType enum from globals.h)
 * s = 10 bit size in units of 32 bit words
 *
 * The last word of each event section is the EndMarker (globals.h)
 *
*/
struct listfile_v1
{
    static const int Version = 1;

    static const int FirstSectionOffset = 8;

    static const int SectionMaxWords  = 0xfffff;
    static const int SectionMaxSize   = SectionMaxWords * sizeof(u32);

    static const int SectionTypeMask  = 0xe0000000; // 3 bit section type
    static const int SectionTypeShift = 29;
    static const int SectionSizeMask  = 0x000fffff; // 20 bit section size in 32 bit words
    static const int SectionSizeShift = 0;
    static const int EventTypeMask    = 0x1e000000; // 4 bit event type
    static const int EventTypeShift   = 25;

    // Subevent containing module data
    static const int ModuleTypeMask  = 0xff000000;  // 8 bit module type
    static const int ModuleTypeShift = 24;

    static const int SubEventMaxWords  = 0xfffff;
    static const int SubEventMaxSize   = SubEventMaxWords * sizeof(u32);
    static const int SubEventSizeMask  = 0x000fffff; // 20 bit subevent size in 32 bit words
    static const int SubEventSizeShift = 0;
};

namespace listfile
{
    enum SectionType
    {
        /* The config section contains the mvmecfg as a json string padded with
         * spaces to the next 32 bit boundary. If the config data size exceeds
         * the maximum section size multiple config sections will be written at
         * the start of the file. */
        SectionType_Config      = 0,

        /* Readout data generated by one VME Event. Contains Subevent Headers
         * to split into VME Module data. */
        SectionType_Event       = 1,

        /* Last section written to a listfile before closing the file. Used for
         * verification purposes. */
        SectionType_End         = 2,

        /* Marker section written once at the start of a run and then once per
         * elapsed second. */
        SectionType_Timetick    = 3,

        /* Max section type possible. */
        SectionType_Max         = 7
    };

    enum VMEModuleType
    {
        Invalid         = 0,
        MADC32          = 1,
        MQDC32          = 2,
        MTDC32          = 3,
        MDPP16_SCP      = 4,
        MDPP32          = 5,
        MDI2            = 6,
        MDPP16_RCP      = 7,
        MDPP16_QDC      = 8,
        VMMR            = 9,

        MesytecCounter = 16,
        VHS4030p = 21,
    };

    static const std::map<VMEModuleType, const char *> VMEModuleTypeNames =
    {
        { VMEModuleType::MADC32,            "MADC-32" },
        { VMEModuleType::MQDC32,            "MQDC-32" },
        { VMEModuleType::MTDC32,            "MTDC-32" },
        { VMEModuleType::MDPP16_SCP,        "MDPP-16_SCP" },
        { VMEModuleType::MDPP32,            "MDPP-32" },
        { VMEModuleType::MDI2,              "MDI-2" },
        { VMEModuleType::MDPP16_RCP,        "MDPP-16_RCP" },
        { VMEModuleType::MDPP16_QDC,        "MDPP-16_QDC" },
        { VMEModuleType::VMMR,              "VMMR" },
        { VMEModuleType::VHS4030p,          "iseg VHS4030p" },
        { VMEModuleType::MesytecCounter,    "Mesytec Counter" },
    };

    const char *get_vme_module_name(VMEModuleType moduleType)
    {
        auto it = VMEModuleTypeNames.find(moduleType);
        if (it != VMEModuleTypeNames.end())
        {
            return it->second;
        }

        return "unknown";
    }

} // end namespace listfile

inline int bitExtractor(int word, int numbits, int position){
    return (((1 << numbits) - 1) & (word >> position)); 
}

template<typename LF>
void process_listfile(std::ifstream &infile, TString filename, bool optverbose)
{
    using namespace listfile;

    bool continueReading = true;
    int counter = 0;

    rootTree rootdata(filename);

    while (continueReading)
    {
        u32 sectionHeader;
        infile.read((char *)&sectionHeader, sizeof(u32));

        u32 sectionType   = (sectionHeader & LF::SectionTypeMask) >> LF::SectionTypeShift;
        u32 sectionSize   = (sectionHeader & LF::SectionSizeMask) >> LF::SectionSizeShift;

        switch (sectionType)
        {
            case SectionType_Config:
                {
                    if (optverbose)
                        cout << "Config section of size " << sectionSize << endl;
                    infile.seekg(sectionSize * sizeof(u32), std::ifstream::cur);
                } break;

            case SectionType_Event:
                {
                    if (optverbose){
                        cout << "Event " << counter << endl;
                    }
                    else{
                        if (counter%10000==0){
                            cout << '\r' << "Processing event " << counter;
                        }
                    }
                    rootdata.initEvent();
                    int pu = 0;
                    int ov = 0;
                    int chn = 0;
                    int data = 0;
                    int time = 0;
                    int extended = 0;

                    u32 eventType = (sectionHeader & LF::EventTypeMask) >> LF::EventTypeShift;
                    if (optverbose){
                        printf("Event section: eventHeader=0x%08x, eventType=%d, eventSize=%u\n",
                               sectionHeader, eventType, sectionSize);
                    }

                    u32 wordsLeft = sectionSize;

                    while (wordsLeft > 1)
                    {
                        u32 subEventHeader;
                        infile.read((char *)&subEventHeader, sizeof(u32));
                        --wordsLeft;

                        u32 moduleType = (subEventHeader & LF::ModuleTypeMask) >> LF::ModuleTypeShift;
                        u32 subEventSize = (subEventHeader & LF::SubEventSizeMask) >> LF::SubEventSizeShift;

                        if (optverbose){
                            printf("  subEventHeader=0x%08x, moduleType=%u (%s), subEventSize=%u\n",
                                   subEventHeader, moduleType, get_vme_module_name((VMEModuleType)moduleType),
                                   subEventSize);
                        }

                        for (u32 i=0; i<subEventSize; ++i)
                        {
                            u32 subEventData;
                            infile.read((char *)&subEventData, sizeof(u32));

                            if (optverbose)
                                printf("    %2u = 0x%08x\n", i, subEventData);

                            if (subEventData == 0xffffffff){
                                if (optverbose)
                                    cout << "\tFill" << endl;
                            }
                            else{
                                if ((moduleType==4)||(moduleType==7)){
                                    int sig = bitExtractor(subEventData, 4, 28);
                                    if (sig==4){ //header
                                        if (optverbose)
                                            cout << "\tHeader" << endl;
                                    }
                                    else if (sig==1){//data
                                        pu = bitExtractor(subEventData, 1, 23);
                                        ov = bitExtractor(subEventData, 1, 22);
                                        chn = bitExtractor(subEventData, 5, 16);
                                        data = bitExtractor(subEventData, 16, 0);
                                        rootdata.setPileup(pu);
                                        rootdata.setOverflow(ov);
                                        rootdata.setADC(chn, data);
                                        
                                        if (optverbose){
                                            cout << "\tData" << endl;
                                            cout << "\t" << pu << "\t" << ov << "\t" 
                                                 << chn << "\t" << data << endl;
                                        }
                                    }
                                    else if(sig==2){//extended time stamp
                                        extended = bitExtractor(subEventData, 16, 0);
                                        rootdata.setExtendedTime(extended);
                                        if (optverbose)
                                            cout << "\tExtended time stamp:\t" << extended << endl;
                                    }
                                    else if(sig>=12){//end of event
                                        time = bitExtractor(subEventData, 30, 0);
                                        rootdata.setTime(time);
                                        if (optverbose){
                                            cout << "\tEnd of event" << endl;
                                            cout << "\tTime:\t" << time << endl;
                                        }
                                    }
                                }
                            }
                        }
                        wordsLeft -= subEventSize;
                    }

                    u32 eventEndMarker;
                    infile.read((char *)&eventEndMarker, sizeof(u32));
                    if (optverbose)
                        printf("   eventEndMarker=0x%08x\n", eventEndMarker);
                    rootdata.writeEvent();
                    counter++;
                } break;

            case SectionType_Timetick:
                {
                    if (optverbose)
                        printf("Timetick\n");
                } break;

            case SectionType_End:
                {
                    printf("\nFound Listfile End section\n");
                    continueReading = false;

                    auto currentFilePos = infile.tellg();
                    infile.seekg(0, std::ifstream::end);
                    auto endFilePos = infile.tellg();

                    if (currentFilePos != endFilePos)
                    {
                        cout << "Warning: " << (endFilePos - currentFilePos)
                            << " bytes left after Listfile End Section" << endl;
                    }


                    break;
                }

            default:
                {
                    printf("Warning: Unknown section type %u of size %u, skipping...\n",
                           sectionType, sectionSize);
                    infile.seekg(sectionSize * sizeof(u32), std::ifstream::cur);
                } break;
        }
    }
    cout << counter << " events total" << endl;
    rootdata.writeTree();
}

void process_listfile(std::ifstream &infile, TString filename, bool optverbose)
{
    u32 fileVersion = 0;

    // Read the fourCC that's at the start of listfiles from version 1 and up.
    const size_t bytesToRead = 4;
    char fourCC[bytesToRead] = {};

    infile.read(fourCC, bytesToRead);
    static const char * const FourCC = "MVME";

    if (std::strncmp(fourCC, FourCC, bytesToRead) == 0)
    {
        infile.read(reinterpret_cast<char *>(&fileVersion), sizeof(fileVersion));
    }

    // Move to the start of the first section
    auto firstSectionOffset = ((fileVersion == 0)
                               ? listfile_v0::FirstSectionOffset
                               : listfile_v1::FirstSectionOffset);

    infile.seekg(firstSectionOffset, std::ifstream::beg);

    cout << "Detected listfile version " << fileVersion << endl;

    if (fileVersion == 0)
    {
        process_listfile<listfile_v0>(infile, filename, optverbose);
    }
    else
    {
        process_listfile<listfile_v1>(infile, filename, optverbose);
    }
}

int main(int argc, char *argv[])
{
    bool optverbose = 0;
    int startindex = 1;

    if (argc<2)
    {
        cerr << "Invalid number of arguments" << endl;
        cerr << "Usage: " << argv[0] << " [-v] <listfiles>" << endl;
        return 1;
    }
    
    if (!strcmp(argv[1], "-v")) //verbose option
    {
        optverbose = 1;
        startindex++;
    }

    //loop over all given files
    for (int file=startindex; file<argc; file++){
        cout << "----- Processing " << argv[file] << " -----" << endl;

        TString filename = argv[file];
        TString dir = argv[file];
        int index = dir.Last('/');
        dir.Remove(index+1,dir.Sizeof());

        //Unzip if zipfile is given
        TString zipfilename = argv[file];
        TString zipcommand = ".! unzip -o " + zipfilename;
        if (filename.EndsWith(".zip")){
            cout << "----- Unzipping file " << filename.Data() << " -----" << endl;
            if (dir.Sizeof()>1)
                zipcommand.Append(" -d " + dir);
            cout << zipcommand.Data() << endl;
            gROOT->ProcessLine(zipcommand.Data());

            //change filename to point to unzipped mvmelst file
            filename.Remove(filename.Sizeof()-4, filename.Sizeof());
            filename.Append("mvmelst");
            cout << "----- Unzip " << filename.Data() << " complete -----" << endl;
        }

        //open mvmelst file for reading
        std::ifstream infile(filename.Data(), std::ios::binary);

        if (!infile.is_open())
        {
            cerr << "Error opening " << filename.Data() << " for reading: " 
                 << std::strerror(errno) << endl;
            cout << argc-file << " files were not converted." << endl;
            return 1;
        }

        infile.exceptions(std::ifstream::badbit | std::ifstream::failbit | std::ifstream::eofbit);

        //process mvmelst file
        try
        {
            process_listfile(infile, filename, optverbose);
        }
        catch (const std::exception &e)
        {
            cerr << "Error processing listfile: " << e.what() << endl;
            cout << argc-file << " files were not converted." << endl;
            return 1;
        }

        //clean up mvme files (save space)
        std::ifstream test_lstfile(filename.Data());
        std::ifstream test_zipfile(zipfilename.Data());
        if ((test_lstfile.is_open())&&(test_zipfile.is_open())){

            cout << "Removing mvmelst file, analysis.analysis, and messages.log" << endl;

            //delete mvmelst file
            TString rmmvmelst = ".! rm -f " + filename;
            cout << rmmvmelst.Data() << endl;;
            gROOT->ProcessLine(rmmvmelst.Data());

            //delete analysis.analysis
            TString rmanalysis = ".! rm -f " + dir + "analysis.analysis";
            cout << rmanalysis.Data() << endl;;
            gROOT->ProcessLine(rmanalysis.Data());

            //delete messages.log
            TString rmlog = ".! rm -f " + dir + "messages.log";
            cout << rmlog.Data() << endl;;
            gROOT->ProcessLine(rmlog.Data());

        }

        cout << "----- " << argv[file] << " complete -----" << endl;
    }
    cout << "----- " << argc-startindex << " files converted -----" << endl;


    return 0;
}
