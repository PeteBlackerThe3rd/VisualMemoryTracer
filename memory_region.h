#ifndef __MEMORY_REGION_H__
#define __MEMORY_REGION_H__

#include <iostream>

class MemoryRegion
{
public:

    enum AccessType : char { Load, Store, Modify, None };

    enum CompBlockType : char { Data, Repeat, End };

    class MemoryReading
    {
    public:
        MemoryReading() : loadCount(0),
                          storeCount(0),
                          modCount(0),
                          firstOp(None),
                          lastOp(None) {};

        MemoryReading(std::istream &in) {
            in.read((char*)&(this->loadCount), sizeof (unsigned short));
            in.read((char*)&(this->storeCount), sizeof (unsigned short));
            in.read((char*)&(this->modCount), sizeof (unsigned short));
            in.read((char*)&(this->firstOp), sizeof (AccessType));
            in.read((char*)&(this->lastOp), sizeof (AccessType));
        };

        friend std::ostream& operator<< (std::ostream& stream,
                                         const MemoryReading& r) {
            stream.write((char*)&r.loadCount, sizeof (unsigned short));
            stream.write((char*)&r.storeCount, sizeof (unsigned short));
            stream.write((char*)&r.modCount, sizeof (unsigned short));
            stream.write((char*)&r.firstOp, sizeof (AccessType));
            stream.write((char*)&r.lastOp, sizeof (AccessType));
        }

        bool operator==(const MemoryReading& b) {
            return (this->loadCount == b.loadCount &&
                    this->storeCount == b.storeCount &&
                    this->modCount == b.modCount &&
                    this->firstOp == b.firstOp &&
                    this->lastOp == b.lastOp);
        }

        bool operator!=(const MemoryReading& b) {
            return !(*this == b);
        }

        unsigned short loadCount, storeCount, modCount;
        AccessType firstOp, lastOp;
    };

    MemoryRegion(unsigned int resolution = 1000)
    {
        //std::cout << "MemoryRegion default contstructor.....\n";
        loadCount = 0;
        storeCount = 0;
        this->resolution = resolution;
        trace.push_back(std::vector<MemoryReading>(resolution));
    }
    MemoryRegion(std::string name, unsigned long start, unsigned long end, unsigned int resolution = 1000)
    {
        this->name = name;
        startAddr = start;
        endAddr = end;

        unsigned long size = end - start;
        if (size < resolution)
            resolution = size;

        loadCount = 0;
        storeCount = 0;
        this->resolution = resolution;
        trace.push_back(std::vector<MemoryReading>(resolution));
    }

    MemoryRegion(std::ifstream &in) {

        in.read((char*)&(this->startAddr), sizeof (unsigned long));
        in.read((char*)&(this->endAddr), sizeof (unsigned long));
        std::getline(in, this->name, '\0');
        in.read((char*)&(this->resolution), sizeof (unsigned int));
        in.read((char*)&(this->loadCount), sizeof (unsigned long));
        in.read((char*)&(this->storeCount), sizeof (unsigned long));

        std::cout << "Reading memory region [" << this->name << "]\n";

        size_t size;
        in.read((char*)&size, sizeof (size_t));
        std::cout << "Reading " << size << "lines of memory region.\n";
        for (size_t i = 0; i < size; ++i)
            this->trace.push_back(MemoryRegion::lineFromStream(in));
    }

    friend std::ostream& operator<< (std::ofstream& stream,
                                     const MemoryRegion& r) {

        stream.write((char*)&r.startAddr, sizeof (unsigned long));
        stream.write((char*)&r.endAddr, sizeof (unsigned long));
        stream << r.name;
        char term = 0;
        stream.write((char*)&term, sizeof (char));
        stream.write((char*)&r.resolution, sizeof (unsigned int));
        stream.write((char*)&r.loadCount, sizeof (unsigned long));
        stream.write((char*)&r.storeCount, sizeof (unsigned long));

        std::cout << "mem_region wrote atomics.\n";

        std::cout << "About to write mem region of size [ ";
        std::cout << r.trace.size() << " x " << r.trace[0].size() << " ]\n";
        std::cout << "Should take about " << (r.trace.size() * r.trace[0].size() * sizeof(MemoryReading)) << " bytes\n";

        size_t size =r.trace.size();
        stream.write((char*)&size, sizeof(size_t));
        int c=0;
        for (auto &line : r.trace) {
            MemoryRegion::lineToStream(line, stream);
            //std::cout << "Wrote trace (file size " << stream.tellp() << " bytes) line " << c++ << std::endl;
        }
    }

    static std::vector<MemoryReading> lineFromStream(std::ifstream &in) {

        CompBlockType type;
        size_t size;
        std::vector<MemoryReading> line;

        do {
            in.read((char*)&type, sizeof (CompBlockType));

            if (type == Data) {
                in.read((char*)&size, sizeof (size_t));
                std::cout << "Reading data block [" << size << "]\n";
                for (size_t i = 0; i < size; ++i)
                    line.push_back(MemoryReading(in));
            } else if (type == Repeat) {
                in.read((char*)&size, sizeof (size_t));
                std::cout << "Reading repeat block [" << size << "]\n";
                MemoryReading reading(in);
                for (size_t i = 0; i < size; ++i)
                    line.push_back(reading);
            }
        } while (type != End);

        std::cout << "Decompressed a line of length [" << line.size() << "]\n";

        return line;
    }

    static void lineToStream(std::vector<MemoryReading> line, std::ofstream &out) {

        size_t pos = 0;
        size_t blockSize;
        CompBlockType type;

        //std::cout << "Write compressed line.\n";

        do {
            if (line.size() - pos == 1 || line[pos] != line[pos + 1]) {
                type = Data;
                if (line.size() - pos == 1)
                    blockSize = 1;
                else
                    blockSize = 2;

                while (pos + blockSize < line.size()) {
                    if (line[pos + blockSize] != line[pos + blockSize + 1])
                        ++blockSize;
                    else
                        break;
                }

                //std::cout << "Data [" << blockSize << "]\n";

                out.write((char*)&type, sizeof (CompBlockType));
                out.write((char*)&blockSize, sizeof (size_t));
                for (size_t i = 0; i < blockSize; ++i)
                    out << line[pos + i];
            } else {
                type = Repeat;
                blockSize = 2;
                while (pos + blockSize < line.size()) {
                    if (line[pos + blockSize] == line[pos + blockSize + 1])
                        ++blockSize;
                    else
                        break;
                }

                //std::cout << "Repeat [" << blockSize << "]\n";

                out.write((char*)&type, sizeof (CompBlockType));
                out.write((char*)&blockSize, sizeof (size_t));
                out << line[pos];
            }
            pos += blockSize;
        } while (pos < line.size());

        if (pos != line.size()) {
            std::cout << "Error! " << pos << "element written of line size" << line.size() << "\n";
        }

        //std::cout << "Completed " << pos << "element written of line size" << line.size() << "\n";

        type = End;
        out.write((char*)&type, sizeof (CompBlockType));
    }

    inline int memAddrToPix(long addr) {
        int pixel = ((addr - startAddr) * resolution) / (endAddr - startAddr);
        return pixel;
    }

    void addLoad(unsigned long address, unsigned int size)
    {
        int index = ((address - startAddr) * resolution) / (endAddr - startAddr);
        int indexEnd = ((address + size - startAddr) * resolution) / (endAddr - startAddr);

        for (int a=index; a<indexEnd; ++a) {
            ++trace.back()[a].loadCount;
            if (trace.back()[a].firstOp == None)
                trace.back()[a].firstOp = Load;
            trace.back()[a].lastOp = Load;
        }
    }

    void addStore(unsigned long address, unsigned int size)
    {

        int index = ((address - startAddr) * resolution) / (endAddr - startAddr);
        int indexEnd = ((address + size - startAddr) * resolution) / (endAddr - startAddr);

        for (int a=index; a<indexEnd; ++a) {
            ++trace.back()[a].storeCount;
            if (trace.back()[a].firstOp == None)
                trace.back()[a].firstOp = Store;
            trace.back()[a].lastOp = Store;
        }
    }

    void addMod(unsigned long address, unsigned int size)
    {

        int index = ((address - startAddr) * resolution) / (endAddr - startAddr);
        int indexEnd = ((address + size - startAddr) * resolution) / (endAddr - startAddr);

        for (int a=index; a<indexEnd; ++a) {
            ++trace.back()[a].modCount;
            if (trace.back()[a].firstOp == None)
                trace.back()[a].firstOp = Modify;
            trace.back()[a].lastOp = Modify;
        }
    }

    void storeRow()
    {
        trace.push_back(std::vector<MemoryReading>(resolution));
    }

    std::vector<std::vector<MemoryReading> > trace;

    unsigned long startAddr, endAddr;
    std::string name;

    unsigned int resolution;

    unsigned long loadCount;
    unsigned long storeCount;
};

#endif  // __MEMORY_REGION_H__