#ifndef __TRACE_SESSION_H__
#define __TRACE_SESSION_H__

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <fstream>

#include "tensor_block.h"
#include "memory_region.h"
#include "activity.h"
#include "time_memory_area.h"

class TraceSession {
public:
    static void toStream(std::ofstream &out);
    static void fromStream(std::ifstream &in);

    static std::string title;

    static std::vector<MemoryRegion> memoryRegions;
    static std::mutex memRegionsMutex;

    static std::vector<Activity> activities;
    static std::mutex activitiesMutex;

    static std::vector<TimeMemoryArea> timeMemoryAreas;
    static float boxAlpha;
    static float boxOutlineAlpha;
    static bool showTrace;

    static unsigned long instructionsPerRow;
    static unsigned long maxTraceRows;
    //static std::vector<TensorBlock> tensors;
    static unsigned long traceStartInstruction;

    static bool readShutdown;
    static std::mutex shutdownMutex;
};

#endif // __TRACE_SESSION_H__