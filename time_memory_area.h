#ifndef __TIME_MEMORY_AREA_H__
#define __TIME_MEMORY_AREA_H__

#include <string>

class TimeMemoryArea {
public:
    TimeMemoryArea(unsigned long startMem,
                   unsigned long endMem,
                   unsigned long startEventAddr,
                   unsigned long endEventAddr) {
        this->startMem = startMem;
        this->endMem = endMem;
        this->startEventAddr = startEventAddr;
        this->endEventAddr = endEventAddr;
        startInstruction = 0;
        endInstruction = 0;
    }

    TimeMemoryArea(std::istream &in) {
        in.read((char*)&this->startMem, sizeof(unsigned long));
        in.read((char*)&this->endMem, sizeof(unsigned long));
        in.read((char*)&this->startEventAddr, sizeof(unsigned long));
        in.read((char*)&this->endEventAddr, sizeof(unsigned long));
        in.read((char*)&this->startInstruction, sizeof(unsigned long));
        in.read((char*)&this->endInstruction, sizeof(unsigned long));
    }

    friend std::ostream& operator<< (std::ostream& stream,
                                     const TimeMemoryArea& area) {
        stream.write((char*)&area.startMem, sizeof(unsigned long));
        stream.write((char*)&area.endMem, sizeof(unsigned long));
        stream.write((char*)&area.startEventAddr, sizeof(unsigned long));
        stream.write((char*)&area.endEventAddr, sizeof(unsigned long));
        stream.write((char*)&area.startInstruction, sizeof(unsigned long));
        stream.write((char*)&area.endInstruction, sizeof(unsigned long));
    }

    unsigned long startMem;
    unsigned long endMem;
    unsigned long startEventAddr;
    unsigned long endEventAddr;
    unsigned long startInstruction;
    unsigned long endInstruction;
};

#endif  // __TIME_MEMORY_AREA_H__