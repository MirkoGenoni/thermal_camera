#include <list>
#include <set>
#include <memory>

#include "memoryState.h"
#include "struct.h"

class ImageVisualizer
{
public:
    ImageVisualizer(MemoryState *memoryStateIN)
    {
        memoryState = memoryStateIN;
    };

    void searchImage(std::list<std::unique_ptr<ImagesFound>> &foundL);
    void nextImage(std::list<std::unique_ptr<ImagesFound>> &foundL);
    void prevImage(std::list<std::unique_ptr<ImagesFound>> &foundL);

    bool deleteImage(std::list<std::unique_ptr<ImagesFound>> &foundL, unsigned short id);

private:
    MemoryState *memoryState;
    void searchFrameAddresses(std::list<std::unique_ptr<ImagesFound>> &foundL);
};