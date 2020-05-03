/*
    Memory analyser utility.
    -------------------------

    Attaches to the given region fifo and reads a set of named regions that
    will be monitored. Stdin is assumed to be piped from the valgrind tool
    Lackey --mem-trace. load and store operations within the monitored regions
    will be recorded through time.

*/
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <cassert>
#include <opencv2/opencv.hpp>
#include "trace_image.h"
#include "activity.h"
#include "tensor_block.h"
#include "memory_region.h"
#include "trace_session.h"

bool getline_async(std::istream& is, std::string& str, char delim = '\n') {

    static std::string lineSoFar;
    char inChar;
    int charsRead = 0;
    bool lineRead = false;
    str = "";

    do {
        charsRead = is.readsome(&inChar, 1);
        if (charsRead == 1) {
            // if the delimiter is read then return the string so far
            if (inChar == delim) {
                str = lineSoFar;
                lineSoFar = "";
                lineRead = true;
            } else {  // otherwise add it to the string so far
                lineSoFar.append(1, inChar);
            }
        }
    } while (charsRead != 0 && !lineRead);

    return lineRead;
}

void readMemoryRegions(int argc, char **argv)
{
    std::string fifoName = "/tmp/vis_memory_tracer_fifo";

    for (int a=0; a<argc; ++a)
        if (std::string(argv[a]).substr(0,14) == "--region_file=")
            fifoName = std::string(argv[a]).substr(14, std::string::npos);

    std::fstream fifo;
    try {
        fifo.open(fifoName.c_str(), std::ios::out | std::ios::in);
    }
    catch (const std::exception& ex) {
        std::cerr << "[\033[92mVMT\033[0m] Exception opening fifo : " << ex.what();
        exit(1);
    }

    std::cout << "[\033[92mVMT\033[0m] Opened region file [" << fifoName << "]\n";

    while (!fifo.eof())
    {
        std::string line;
        getline_async(fifo, line);

        if (line.length() == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::stringstream lineStream(line);
        char dump;

        // if this line sets the title of the analysis
        if (line[0] == '"') {
            lineStream >> dump;
            std::string analysisTitle;
            std::getline(lineStream, analysisTitle, '"');
            std::cout << "[\033[92mVMT\033[0m] Set title [" << analysisTitle;
            std::cout << "]\n";
            TraceSession::title = analysisTitle;
        }
        // if this line defined a memory region,
        else if (line[0] == ':') {
            std::string name;
            unsigned long start, end, resolution = 0;

            lineStream >> dump;
            std::getline(lineStream, name, '(');

            lineStream >> start;
            lineStream >> dump >> end;
            lineStream >> dump >> resolution;

            // use default resolution of 1000 if none specified
            if (resolution == 0)
                resolution = 1000;

            if (name == "")
                break;

            MemoryRegion region(name, start, end, resolution);

            std::cout << "[\033[92mVMT\033[0m] Added region [" << region.name;
            std::cout << "] from [" << region.startAddr << "] to [";
            std::cout << region.endAddr << "] with resolution [";
            std::cout << resolution << "]\n";

            TraceSession::memRegionsMutex.lock();
            TraceSession::memoryRegions.push_back(region);
            TraceSession::memRegionsMutex.unlock();
        }
        // if this line defines an event
        else if (line[0] == '#') {
            std::string name;
            unsigned long addr;

            lineStream >> dump;
            std:getline(lineStream, name, '(');
            lineStream >> addr;

            std::cout << "[\033[92mVMT\033[0m] Added activity [" << name;
            std::cout << "] at address " << addr << std::endl;

            TraceSession::activitiesMutex.lock();
            TraceSession::activities.push_back(Activity(name, addr));
            TraceSession::activitiesMutex.unlock();
        }
        // if this line defines an area of time-memory space
        else if (line[0] == '&') {
            unsigned long startAddr, endAddr;
            unsigned long startEventAddr, endEventAddr;

            lineStream >> dump >> startAddr;
            lineStream >> dump >> endAddr;
            lineStream >> dump >> startEventAddr;
            lineStream >> dump >> endEventAddr;

            std::cout << "[\033[92mVMT\033[0m] Added time-memory area.\n";

            TimeMemoryArea area(startAddr, endAddr,
                                startEventAddr,
                                endEventAddr);
            TraceSession::timeMemoryAreas.push_back(area);
        }
        // if this line is a read query request
        else if (line.substr(0,7) == "@ready?") {
            std::cout << "Responding with [@yes_ready.]\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            fifo << "@yes_ready.\n";
        }

        // break out of this loop if shutdown requested.
        TraceSession::shutdownMutex.lock();
        if (TraceSession::readShutdown)
        {
            TraceSession::shutdownMutex.unlock();
            break;
        }
        TraceSession::shutdownMutex.unlock();
    }

    fifo.close();
}

int main(int argc, char **argv)
{
    std::cout << "[\033[92mVisual Memory Tracer\033[0m] Starting up.\n";

    for (int a=0; a<argc; ++a)
    {
        if (std::string(argv[a]).substr(0,14) == "--ins_per_row=") {
            TraceSession::instructionsPerRow = std::atoi(std::string(argv[a]).substr(14, std::string::npos).c_str());
            std::cout << "[\033[92mVMT\033[0m] Setting instruction per row to : " << TraceSession::instructionsPerRow << std::endl;
        }
        else if (std::string(argv[a]).substr(0,12) == "--box_alpha=") {
            TraceSession::boxAlpha = std::atof(std::string(argv[a]).substr(12, std::string::npos).c_str());
            std::cout << "[\033[92mVMT\033[0m] Setting time-memory area alpha to : " << (TraceSession::boxAlpha*100) << "%" << std::endl;
        }
    }

    std::cout << "Finishing loading mem map or not." << std::endl;

    std::thread region_fifo_thread(readMemoryRegions, argc, argv);

    // Now we look reading lines from stdin and process them as output from valgrind --tool=lackey
    // we terminate when a line contains "Exit code" indicating the end of the valgrind process.
    unsigned long instructionCount = 0;
    bool end = false;
    bool anythingRecorded = false;
    bool recording = false;

    int loopCount=0;

    do {
        std::string line;
        std::getline(std::cin, line);

        if (line.length() >= 1) {
            if (recording && line[0] == 'I')
            {
                ++instructionCount;

                if (anythingRecorded && (instructionCount % TraceSession::instructionsPerRow) == 0)
                {
                    if (TraceSession::memoryRegions.size() > 0 && TraceSession::memoryRegions[0].trace.size() == 1)
                    {
                        TraceSession::traceStartInstruction = instructionCount - TraceSession::instructionsPerRow;
                    }

                    for (int r=0; r<TraceSession::memoryRegions.size(); ++r)
                        TraceSession::memoryRegions[r].storeRow();

                    // if the recording limit has been reached then stop.
                    if (TraceSession::memoryRegions.size() > 0 && TraceSession::memoryRegions[0].trace.size() > TraceSession::maxTraceRows)
                    {
                        break;
                        std::cout << "[\033[92mVMT\033[0m] Read limit of " << TraceSession::maxTraceRows << " rows reached." << std::endl;
                    }
                }
            }

            char type = line[1];
            if (type == 'L' || type == 'S' || type == 'M')
            {
                anythingRecorded = true;
                std::stringstream lineStream(line.substr(3, std::string::npos));

                std::string hexStr;
                std::getline(lineStream, hexStr, ',');
                unsigned long addr = std::stol(hexStr, nullptr, 16);

                unsigned int size;
                lineStream >> size;

                // check for recording start stop events
                if (TraceSession::activities.size() > 0 &&
                    TraceSession::activities[0].addr == addr) {
                  if (type == 'S') {
                    recording = true;
                    std::cout << "[\033[92mVMT\033[0m] Started Recording.\n";
                  } else if (type == 'L') {
                    recording = false;
                    std::cout << "[\033[92mVMT\033[0m] Stopping Recording.\n";
                  }
                }

                // check for memeory access in inspected regions
                bool update = false;
                if (recording) {
                  TraceSession::memRegionsMutex.lock();
                  for (int r=0; r<TraceSession::memoryRegions.size(); ++r)
                    if (addr >= TraceSession::memoryRegions[r].startAddr && addr < TraceSession::memoryRegions[r].endAddr) {
                      if (type == 'L') {
                        ++TraceSession::memoryRegions[r].loadCount;
                        TraceSession::memoryRegions[r].addLoad(addr, size);
                      } else if (type == 'S') {
                        ++TraceSession::memoryRegions[r].storeCount;
                        TraceSession::memoryRegions[r].addStore(addr, size);
                      } else if (type == 'M') {
                        TraceSession::memoryRegions[r].addMod(addr, size);
                      }
                      update = true;
                    }
                  TraceSession::memRegionsMutex.unlock();

                  // check for activity start stop events
                  TraceSession::activitiesMutex.lock();
                  for (int a=1; a<TraceSession::activities.size(); ++a) {
                    if (TraceSession::activities[a].addr == addr) {
                      if (type == 'S')
                        TraceSession::activities[a].startEvent(instructionCount);
                      else if (type == 'L')
                        TraceSession::activities[a].stopEvent(instructionCount);
                      update = true;
                    }
                  }
                  TraceSession::activitiesMutex.unlock();
                }

                if (update)
                    {
                    std::cout << "\r";
                    for (int r=0; r<TraceSession::memoryRegions.size(); ++r)
                        std::cout << "[" << TraceSession::memoryRegions[r].name << "] l:" << TraceSession::memoryRegions[r].loadCount << " s:" << TraceSession::memoryRegions[r].storeCount << "  ";
                    std::cout << "Instruction " << instructionCount;
                    }
            }
        }

        // detect the end of Valgrind output by searching for 'Exit code'
        end = line.find("Exit code") != std::string::npos;
    } while (!end);

    // shutdown the read memory regions thread
    TraceSession::shutdownMutex.lock();
    TraceSession::readShutdown = true;
    TraceSession::shutdownMutex.unlock();
    region_fifo_thread.join();

    // debug activity monitoring
    /*std::cout << "\nEvents\n";
    for (int a=0; a<TraceSession::activities.size(); ++a)
    {
        std::cout << "[" << TraceSession::activities[a].name << ":" << TraceSession::activities[a].addr << "] " << TraceSession::activities[a].occurrences.size() << " occurrences" << std::endl;
        for (int e=0; e<TraceSession::activities[a].occurrences.size(); ++e)
        {
            std::cout << "start " << TraceSession::activities[a].occurrences[e].start << " end " << TraceSession::activities[a].occurrences[e].stop;
            std::cout << "start " << TraceSession::activities[a].occurrences[e].start << " end " << TraceSession::activities[a].occurrences[e].stop;
            std::cout << " duration " << (TraceSession::activities[a].occurrences[e].stop - TraceSession::activities[a].occurrences[e].start) << std::endl;
        }
    }*/

    // save trace data
    std::ofstream traceDataFile("model.trace");
    if (traceDataFile.is_open()) {
        TraceSession::toStream(traceDataFile);
        traceDataFile.close();
    } else
        std::cerr << "Could not open \"model.trace\" to save trace data" << std::endl;

    // save trace image files
    TraceImage traceSaver;
    bool showOperations = TraceSession::activities.size() != 0;
    TraceSession::boxAlpha = 0.0;
    TraceSession::boxOutlineAlpha = 0.0;
    traceSaver.saveTraceImage(TraceSession::memoryRegions,
                              "model_trace.png",
                              TraceSession::title,
                              true,
                              showOperations);

    TraceSession::boxAlpha = 1.0;
    TraceSession::boxOutlineAlpha = 0.0;
    traceSaver.saveTraceImage(TraceSession::memoryRegions,
                              "model_blocks.png",
                              TraceSession::title,
                              true,
                              showOperations);

    TraceSession::boxAlpha = 0.0;
    TraceSession::boxOutlineAlpha = 1.0;
    TraceSession::showTrace = false;
    traceSaver.saveTraceImage(TraceSession::memoryRegions,
                              "model_block_outlines.png",
                              TraceSession::title,
                              true,
                              showOperations);

    std::cout << "[\033[92mVisual Memory Tracer\033[0m] Shutdown successfully.\n";

    return 0;
}