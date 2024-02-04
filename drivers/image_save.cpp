#include "memoryState.h"
#include <drivers/mlx90640.h>
#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/flash.h>
#include <drivers/mlx90640.h>
#include <util/crc16.h>
#include <util/util.h>

using namespace std;

struct Image
{
    unsigned char type;
    unsigned short id;
    unsigned char position;
};

void saveImage(MemoryState* state, std::unique_ptr<MLX90640MemoryFrame> image, int imageSize){
    auto& flash=Flash::instance();
    std::unique_ptr<MLX90640MemoryFrame> memoryFrame = std::move(image);
    int byteForImage = 248; //page bytes reserved for the actual image
    int increment = byteForImage / 2; //increase in image pixel
    
    unsigned int size = sizeof(Image) + byteForImage;
    assert(size<=flash.pageSize()); // ensures that overhead + image can be contained in a page

    if(state->increaseOccupiedMemory(6)==false){ //check for enough free memory
        iprintf("not enough memory\n");
        return;
    }

    // iprintf("Size: %d\n", size);

    // int i=0;
    // for(auto num: memoryFrame->memoryImage){
    //     iprintf("%d: %u\n", i, num);
    //     i++;
    // }
    for(int j=0, y=0; j<imageSize; j+=increment, y++){    
        auto buffer=make_unique<unsigned char[]>(size);

        auto *ImageHeader=reinterpret_cast<Image*>(buffer.get());
        ImageHeader->id=1;
        ImageHeader->type=1;
        ImageHeader->position=y;

        memcpy(buffer.get()+sizeof(Image), (memoryFrame->memoryImage)+j, byteForImage);

        //DEBUG correct memcopy to FLASH memory pages
        // auto *ImageHeaderAfter=reinterpret_cast<Image*>(buffer.get());
        // unsigned short* test = reinterpret_cast<unsigned short*>(buffer.get()+sizeof(Image));
        // for(int i=0; i<increment && i+j<imageSize; i++){
        //     if(ImageHeaderAfter->id!=ImageHeader->id) iprintf("WRONG ID | Old: %u New:%u", ImageHeader->id, ImageHeaderAfter->id);
        //     if(ImageHeaderAfter->type!=ImageHeader->type) iprintf("WRONG TYPE | Old: %u New: %u", ImageHeader->type, ImageHeaderAfter->type);
        //     if(ImageHeader->position!=ImageHeaderAfter->position) iprintf("WRONG PAGE NUMBER | Old:%u New:%u", ImageHeader->position, ImageHeaderAfter->position);
        //     if(memoryFrame->memoryImage[i+j]!=test[i]) iprintf("WRONG PIXEL | Page: %u    original: %d: %u \t new: %d: %u\n", ImageHeaderAfter->position, i+j ,memoryFrame->memoryImage[i+j], i, test[i]);
            
        //     iprintf("Page: %u    original: %d: %u \t new: %d: %u\n", ImageHeaderAfter->position, i+j ,memoryFrame->memoryImage[i+j], i, test[i]);
        // }
        
        int startAddress = state->getFreeAddress();
        iprintf("Writing on address 0x%x\n",startAddress);

        if(flash.write(startAddress,buffer.get(),size)==false)
        {
            iprintf("Failed to write address 0x%x\n",startAddress);
            return;
        }

        //necessary read, otherwise consecutive writes aren't finalized
        //TODO: understand on drivers how to remove this
        if(flash.read(state->getFreeAddress(),buffer.get(),size)==false)
        {
            iprintf("Failed to read address 0x%x\n",state->getFreeAddress());
            return;
        }

        state->increaseMemoryAddressFree(flash.pageSize());

        buffer.reset(nullptr);
    }

    memoryFrame.reset();
    
    iprintf("saving\n");
}