
#include "trace_session.h"

std::string TraceSession::title = "Title not set";

std::vector<MemoryRegion> TraceSession::memoryRegions;
std::mutex TraceSession::memRegionsMutex;

std::vector<Activity> TraceSession::activities;
std::mutex TraceSession::activitiesMutex;

std::vector<TimeMemoryArea> TraceSession::timeMemoryAreas;
float TraceSession::boxAlpha = 0.15;
float TraceSession::boxOutlineAlpha = 1.0;
bool TraceSession::showTrace = true;

//std::vector<TensorBlock> TraceSession::tensors;
unsigned long TraceSession::instructionsPerRow = 1 * 1000;
unsigned long TraceSession::maxTraceRows = 30 * 1000;
unsigned long TraceSession::traceStartInstruction = 0;

bool TraceSession::readShutdown = false;
std::mutex TraceSession::shutdownMutex;


void TraceSession::toStream(std::ofstream &out) {

    // write single values
    out.write((char*)&TraceSession::boxAlpha, sizeof (TraceSession::boxAlpha));
    out.write((char*)&TraceSession::instructionsPerRow, sizeof (TraceSession::instructionsPerRow));
    out.write((char*)&TraceSession::maxTraceRows, sizeof (TraceSession::maxTraceRows));
    out.write((char*)&TraceSession::traceStartInstruction, sizeof (TraceSession::traceStartInstruction));

    std::cout << "Wrote atomic values.\n";

    // write vectors
    size_t size = TraceSession::memoryRegions.size();
    out.write((char*)&size, sizeof (size_t));
    for (auto &memoryRegion : TraceSession::memoryRegions)
        out << memoryRegion;

    std::cout << "Wrote " << TraceSession::memoryRegions.size() << " memory regions.\n";

    size = TraceSession::activities.size();
    out.write((char*)&size, sizeof (size_t));
    for (auto &activity : TraceSession::activities)
        out << activity;

    std::cout << "Wrote " << TraceSession::activities.size() << " activities.\n";

    size = TraceSession::timeMemoryAreas.size();
    out.write((char*)&size, sizeof (size_t));
    for (auto &timeMemoryArea : TraceSession::timeMemoryAreas)
        out << timeMemoryArea;

    std::cout << "Wrote " << TraceSession::timeMemoryAreas.size() << " memory areas.\n";
}

void TraceSession::fromStream(std::ifstream &in) {

    // read single values
    in.read((char*)&TraceSession::boxAlpha, sizeof (TraceSession::boxAlpha));
    in.read((char*)&TraceSession::instructionsPerRow, sizeof (TraceSession::instructionsPerRow));
    in.read((char*)&TraceSession::maxTraceRows, sizeof (TraceSession::maxTraceRows));
    in.read((char*)&TraceSession::traceStartInstruction, sizeof (TraceSession::traceStartInstruction));

    std::cout << "read single values. unsigned long is "<< sizeof (unsigned long) << " bytes long \n";

    // read vectors
    size_t size;
    in.read((char*)&size, sizeof(size_t));
    for (size_t i = 0; i < size; ++i)
        TraceSession::memoryRegions.push_back(MemoryRegion(in));

    std::cout << "read " << size << " memory regions.\n";

    in.read((char*)&size, sizeof(size_t));
    for (size_t i = 0; i < size; ++i)
        TraceSession::activities.push_back(Activity(in));

    std::cout << "read " << size << " activites.\n";

    in.read((char*)&size, sizeof(size_t));
    for (size_t i = 0; i < size; ++i)
        TraceSession::timeMemoryAreas.push_back(TimeMemoryArea(in));

    std::cout << "read " << size << " memory areas.\n";
}
