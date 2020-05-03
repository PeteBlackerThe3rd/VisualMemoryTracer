#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__

#include <vector>
#include <string>
#include <fstream>

class Activity {
public:
    Activity(std::string name, unsigned long addr);

    Activity(std::ifstream& in);

    class Occurrence {
    public:
        Occurrence(unsigned long start);
        unsigned long start, stop;
    };

    void startEvent(unsigned long instructionCount);

    void stopEvent(unsigned long instructionCount);

    friend std::ostream& operator<< (std::ofstream& stream,
                                     const Activity& a) {


        stream << a.name;
        char term = 0;
        stream.write((char*)&term, sizeof (char));
        stream.write((char*)&a.addr, sizeof (unsigned long));
        size_t size = a.occurrences.size();
        stream.write((char*)&size, sizeof (size_t));
        for (auto &occ : a.occurrences) {
            stream.write((char*)&occ.start, sizeof (unsigned long));
            stream.write((char*)&occ.stop, sizeof (unsigned long));
        }
    }

    std::string name;
    unsigned long addr;

    std::vector<Occurrence> occurrences;
};

#endif  // __ACTIVITY_H__