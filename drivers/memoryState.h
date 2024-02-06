#pragma once
#include <drivers/mlx90640.h>
#include <memory>

using namespace std;

struct Page
{
    unsigned short address;
    unsigned char type;
    unsigned short id;
    unsigned char position;
    bool used;
};

struct Sector
{
    Page pages[63];
};

struct Test
{
    unsigned short id;
    unsigned short ids[10];
    unsigned char pages[189];
};

class MemoryState
{
public:
    unsigned int getFreeAddress()
    {
        return firstMemoryAddressFree;
    };
    unsigned int getSettingAddress()
    {
        return settingsAddress;
    };
    unsigned int getTotalMemory()
    {
        return totalMemory;
    };
    unsigned int getOccupiedMemory()
    {
        return occupiedMemory;
    };

    void setFirstMemoryAddressFree(unsigned int address)
    {
        firstMemoryAddressFree = address;
    };

    void increaseMemoryAddressFree(unsigned int address, unsigned char type, unsigned short id, unsigned char position, unsigned int increment);

    void setSettingAddress(unsigned int address)
    {
        settingsAddress = address;
    };

    void setOccupiedMemory(unsigned int value)
    {
        occupiedMemory = value;
    };

    bool increaseOccupiedMemory(unsigned int dimension);

    void scanMemory(int optionsSize);

    void addPages(unsigned int address, unsigned char type, unsigned short id, unsigned char position);

    void clearMemory();

    unsigned short getCurrentImageId()
    {
        return currentImageId;
    }
    void increaseImageId()
    {
        currentImageId++;
    }

private:
    unsigned int firstMemoryAddressFree = 0;
    unsigned int settingsAddress = 0;
    unsigned int totalMemory = 16;    // number of 256 bytes pages free
    unsigned int occupiedMemory = 64; // number of pages occupied

    unsigned short currentImageId = -1;
    shared_ptr<Sector> sector = make_shared<Sector>();
};