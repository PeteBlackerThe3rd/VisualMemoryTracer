#ifndef __VIS_MEM_ANALYSER_PAYLOAD_H__
#define __VIS_MEM_ANALYSER_PAYLOAD_H__

#include <iostream>
#include <fstream>
#include <string>

namespace VisMemoryTrace {

    class Event {
    public:
        Event() {
            name = "Not_Set";
            address = nullptr;
        }

        Event(std::string name,
              int *addr,
              std::ofstream *nullFile) {
            this->name = name;
            this->address = addr;
            this->nullFile = nullFile;
        }

        void start()  {
            if (address != nullptr)
                *(address) = rand();
        }

        void stop() {
            if (address != nullptr)
                *(nullFile) << *(address) << std::endl;
        }

    private:
        std::string name;
        volatile int *address;
        std::ofstream *nullFile;
    };


    class Tracer {
    public:
        Tracer(int argc,
               char **argv,
               int eventLimit = 100) {

            std::string fifoName = "/tmp/vis_memory_tracer_fifo";

            for (int a=0; a<argc; ++a)
                if (std::string(argv[a]).substr(0,14) == "--region_file=")
                    fifoName = std::string(argv[a]).substr(14, std::string::npos);

            try{
                fifo.open(fifoName, std::ios::out | std::ios::in);
            }
            catch (std::fstream::failure &writeErr) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Failed to open VisMemoryTracer fifo [" << fifoName;
                std::cerr << "] (" << writeErr.what() << ")" << std::endl;
            }

            try{
                nullFile.open("/dev/null");
            }
            catch (std::ofstream::failure &writeErr) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Failed to open /dev/null. Not on linux?" << std::endl;
            }

            std::cout << "[\033[93mVMT Payload:\033[0m] Opened region fifo [" << fifoName << "]\n";

            this->eventLimit = eventLimit;
            eventSpace = new int[eventLimit];
            eventsUsed = 0;

            // setup recording event used to start and stop trace recording.
            recordingEvent = addEvent("Recording");
        }

        ~Tracer() {
            if (fifo.is_open())
                fifo.close();
            delete[] eventSpace;
        }

        Event getRecordingEvent() {
            return recordingEvent;
        }

        void setTitle(std::string title) {
            if (!fifo.is_open()) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Error: Cannot ";
                std::cerr << "set title \"" << title << "\", fifo not open.";
                std::cerr << std::endl;
            } else {
                fifo << "\"" << title << "\"\n";
            }
        }

        void addRegion(std::string name,
                       void *addr,
                       unsigned long sizeBytes,
                       unsigned long resolution = 1500) {
            if (!fifo.is_open()) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Error: Cannot add ";
                std::cerr << "memory region \"" << name << "\", fifo not open.";
                std::cerr << std::endl;
            } else {
                unsigned long start = (unsigned long)addr;
                unsigned long end = start + sizeBytes;
                fifo << ":" << name << "(" << start << "," << end;
                fifo << "," << resolution << "\n";
                fifo.flush();
            }
        }

        Event addEvent(std::string name,
                        volatile int **eventAddr = nullptr) {
            if (!fifo.is_open()) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Error: Cannot add event flag \"" << name << "\", fifo not open.";
                std::cerr << std::endl;
            } else {
                if (eventsUsed == eventLimit) {
                    std::cerr << "[\033[93mVMT Payload:\033[0m] Error: Cannot add event flag \"" << name << "\", event Space full.";
                    std::cerr << std::endl;
                } else {
                    int *addr = &eventSpace[eventsUsed++];
                    if (eventAddr)
                        *eventAddr = addr;

                    fifo << "#" << name << "(" << (unsigned long)(addr) << "\n";
                    fifo.flush();

                    return Event(name, addr, &nullFile);
                }
            }
            return Event("Error, not Created!", eventSpace, &nullFile);
        }

        void addArea(unsigned long startAddr, unsigned long endAddr,
                     unsigned long startEventAddr, unsigned long endEventAddr) {
            if (!fifo.is_open()) {
                std::cerr << "[\033[93mVMT Payload:\033[0m] Error:";
                std::cerr << " Cannot add Area, fifo not open.";
                std::cerr << std::endl;
            } else {
                fifo << "&" << startAddr << "," << endAddr;
                fifo << "," << startEventAddr << "," << endEventAddr << "\n";
                fifo.flush();
            }
        }

        //void waitReady();

        std::fstream fifo;
    private:
        std::ofstream nullFile;
        int *eventSpace;
        int eventLimit;
        int eventsUsed;
        Event recordingEvent;
    };
};

/*void VisMemoryTrace::Tracer::waitReady() {
    fifo.clear();
    std::cout << "fifo EOF is " << (fifo.eof() ? "true" : "false") << std::endl;
    fifo << "@ready?\n";
    bool ready = false;
    while (!ready) {
        std::string line;

        std::cout << "Pre getline.\n";

        std::cout << "fifo EOF is " << (fifo.eof() ? "true" : "false") << std::endl;

        std::getline(fifo, line);

        std::cout << "wait loop.\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (line.length() > 0) {
            std::cout << "Waiting recieved [" << line << "]\n";
        }

        ready = (line.substr(0,11) == "@yes_ready.");
    }
}*/

#endif  // __VIS_MEM_ANALYSER_PAYLOAD_H__