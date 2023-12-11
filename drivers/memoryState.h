#pragma once
#include <drivers/mlx90640.h>

class MemoryState {
    public:
        unsigned int getFreeAddress();
        unsigned int getSettingAddress();
        void setFirstMemoryAddressFree(unsigned int address);
        void setSettingAddress(unsigned int address);
        void setOccupiedMemory(unsigned int value);
        void increaseMemoryAddressFree(unsigned int increment);
        bool increaseOccupiedMemory(unsigned int dimension);
        void scanMemory(int optionsSize);
        unsigned int getTotalMemory();
        unsigned int getOccupiedMemory();
    private:
        unsigned int firstMemoryAddressFree=0;
        unsigned int settingsAddress=0;
        unsigned int totalMemory=16; //number of 256 bytes pages free
        unsigned int occupiedMemory=1; // number of pages occupied
};