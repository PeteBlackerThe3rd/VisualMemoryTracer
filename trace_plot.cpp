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

int main(int argc, char **argv)
{
    std::cout << "[\033[92mVisual Memory Trace - Plot Renderer\033[0m] Starting up.\n";

    std::string traceFilename = std::string(argv[1]);
    std::string outputImageFilename = "trace.png";

    for (int a=0; a<argc; ++a)
    {
        if (std::string(argv[a]).substr(0,6) == "--out=") {
            outputImageFilename = std::atof(std::string(argv[a]).substr(6, std::string::npos).c_str());
            std::cout << "[\033[92mVMT\033[0m] Setting output image file name to : " << outputImageFilename << std::endl;
        }
    }

    std::cout << "Loading memory trace [" << traceFilename << "]\n";

    // load trace data
    std::ifstream traceDataFile(traceFilename);
    if (traceDataFile.is_open()) {
        TraceSession::fromStream(traceDataFile);
        traceDataFile.close();
    } else
        std::cerr << "Could not open trace file \"" << traceFilename << "\"\n";

    // save trace image file
    std::cout << "Saving plot \"" << outputImageFilename << "\"\n";
    TraceImage traceSaver;
    bool showOperations = TraceSession::activities.size() != 0;
    traceSaver.saveTraceImage(TraceSession::memoryRegions,
                              outputImageFilename,
                              TraceSession::title,
                              true,
                              showOperations);

    std::cout << "Complete.\n";

    return 0;
}