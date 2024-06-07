#include <list>
#include <set>
#include <memory>

#include "memoryState.h"
#include "struct.h"

class ImageVisualizer
{
public:
    ImageVisualizer(MemoryState *memoryState)
    {
        this->memoryState = memoryState;
        sector = memoryState->getSector();
        firstMemoryAddressFree = memoryState->getFreeAddress();
        isCurrentInode = memoryState->getInodeFound();
        currentInodeAddress = memoryState->getOldInodeAddress();
        currentInode = memoryState->getImagesOld();
    };

    void searchImage(std::list<std::unique_ptr<ImagesFound>> &foundL);
    void nextImage(std::list<std::unique_ptr<ImagesFound>> &foundL);
    void prevImage(std::list<std::unique_ptr<ImagesFound>> &foundL);

    bool deleteImage(std::list<std::unique_ptr<ImagesFound>> &foundL, unsigned short id);

private:
    shared_ptr<Sector> sector;
    unsigned int firstMemoryAddressFree;

    bool isCurrentInode = false;
    unsigned int currentInodeAddress = 0;
    MemoryState *memoryState;
    set<unsigned short> currentInode;
};