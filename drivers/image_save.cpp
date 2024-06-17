#include <drivers/mlx90640.h>
#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/flash.h>
#include <drivers/mlx90640.h>
#include <util/crc16.h>
#include <util/util.h>

#include "memoryState.h"
#include "struct.h"

using namespace std;

void saveImage(MemoryState* state, std::unique_ptr<MLX90640MemoryFrame> image, int imageSize){
    auto& flash=Flash::instance();
    std::unique_ptr<MLX90640MemoryFrame> memoryFrame = std::move(image);
    int byteForImage = 248; //page bytes reserved for the actual image
    int increment = byteForImage / 2; //increase in image pixel
    
    unsigned int size = sizeof(Image) + byteForImage;
    assert(size<=flash.pageSize()); // ensures that overhead + image can be contained in a page
    iprintf("Saving image id: %d\n", state->getCurrentImageId());
    iprintf("Image dimension: %d\n", size);

    // if(state->increaseOccupiedMemory(6)==false){ //check for enough free memory
    //     iprintf("not enough memory\n");
    //     return;
    // }

    // iprintf("Size: %d\n", size);

    // int i=0;
    // for(auto num: memoryFrame->memoryImage){
    //     iprintf("%d: %u\n", i, num);
    //     i++;
    // }
    for(int j=0, y=0; j<imageSize; j+=increment, y++){    
        auto buffer=make_unique<unsigned char[]>(size);

        auto *ImageHeader=reinterpret_cast<Image*>(buffer.get());
        ImageHeader->id = state->getCurrentImageId();
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
        
        unsigned int startAddress = state->getFreeAddress();
        iprintf("Writing IMAGE on address 0x%x\n",startAddress);

        if(flash.write(startAddress,buffer.get(),size)==false)
        {
            iprintf("Failed to write address 0x%x\n",startAddress);
            return;
        }


        state->increaseMemoryAddressFree(startAddress, ImageHeader->type, ImageHeader->id, ImageHeader->position, flash.pageSize());

        buffer.reset(nullptr);
    }

    state->increaseImageId();
    memoryFrame.reset();
    
    iprintf("saving\n");
}

void loadImage(MLX90640Frame* frame, unsigned int* address){
    Flash& flash = Flash::instance();
    auto buffer= make_unique<unsigned char[]>(256);
    auto reinterpret = reinterpret_cast<unsigned short*>(buffer.get()+sizeof(Image));
    
    for(int i=0; i<5; i++){
        if(flash.read(address[i], buffer.get(), 256)==false){
            iprintf("Failed to write address 0x%x\n",address[i]);
            return;
        };
        for(int o=0; o<124; o++){
            frame->temperature[i*124+o] = reinterpret[o];
        }
    }
    if(flash.read(address[5], buffer.get(), 256)==false){
        iprintf("Failed to write address 0x%x\n",address[5]);
        return;
    };
    for(int o=0; o<100; o++){
        frame->temperature[620+o] = reinterpret[o];
    }
    puts("Image fully loaded");
}