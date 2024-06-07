#include <memory>
#include <list>
#include <set>

#include "struct.h"

using namespace std;

#ifndef MEMORYSTATE
#define MEMORYSTATE

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

    shared_ptr<Sector> getSector()
    {
        return sector;
    };

    bool getInodeFound()
    {
        return inodeFound;
    };

    unsigned int getOldInodeAddress()
    {
        return oldInodeAddress;
    };

    set<unsigned short> getImagesOld()
    {
        return imageIds_0;
    }

    list<ImapModifiedCache *> getImapsModified()
    {
        list<unique_ptr<ImapModifiedCache>>::iterator it;
        list<ImapModifiedCache *> test;
        for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
        {
            test.push_back(it->get());
        }
        return test;
    }

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

    unsigned int rewriteInode(unsigned int address, unsigned short imageId);
    unsigned int rewriteImap(unsigned int address, unsigned short imageId, std::list<std::unique_ptr<InodeModified>> &foundL);
    void updateOldInodeAddress(std::list<std::unique_ptr<InodeModified>> &foundL, unsigned short id);
    void updateCurrentSector(unsigned short id);
    void writeMockInode();

private:
    unsigned int firstMemoryAddressFree = 0;
    unsigned int settingsAddress = 0;
    unsigned int totalMemory = 16;    // number of 256 bytes pages free
    unsigned int occupiedMemory = 64; // number of pages occupied
    unsigned short currentImageId = -1;

    bool inodeFound = false;
    unsigned int oldInodeAddress = 0;
    unsigned short oldInodeId = 0;

    shared_ptr<Sector> sector = make_shared<Sector>();
    set<unsigned short> imageIds_0;
    set<unsigned short> imageIds_1;

    std::list<unique_ptr<ImapModifiedCache>> imap_modified;

    unsigned short initialRemaining = 63;
};
#endif