
#include "activity.h"

Activity::Activity(std::string name, unsigned long addr) {
    this->name = name;
    this->addr = addr;
}

Activity::Activity(std::ifstream& in) {
    std::getline(in, this->name, '\0');
    in.read((char*)&(this->addr), sizeof (unsigned long));
    size_t size;
    in.read((char*)&size, sizeof (size_t));
    for (size_t i = 0; i < size; ++i) {
        Activity::Occurrence newOcc(0);
        in.read((char*)&(newOcc.start), sizeof (unsigned long));
        in.read((char*)&(newOcc.stop), sizeof (unsigned long));
        this->occurrences.push_back(newOcc);
    }
}

Activity::Occurrence::Occurrence(unsigned long start) {
    this->start = start;
    this->stop = 0;
}

void Activity::startEvent(unsigned long instructionCount) {
    // assert that the activity is currently inactive
    //assert((occurrences.size() == 0 || occurrences.back().stop != 0) && "Error. Cannot start an event that is already active.");

    occurrences.push_back(Occurrence(instructionCount));
}

void Activity::stopEvent(unsigned long instructionCount) {
    // assert that the activithy is currently active
    //assert(occurrences.size() > 0 && occurrences.back().stop == 0 && "Error. Cannot stop and event that isn't active.");

    occurrences.back().stop = instructionCount;
}