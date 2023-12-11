#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/flash.h>
#include <drivers/mlx90640.h>
#include <util/crc16.h>
#include <util/util.h>
#include "memoryState.h"

using namespace std;
using namespace miosix;

struct Header
{
    unsigned char type;        //0x00 if settings, 0x01 if image, 0xff if not type
    unsigned char written;     //0x00 if written,     0xff if not written
    unsigned char invalidated; //0x00 if invalidated, 0xff if not invalidated
    unsigned short crc;
};

struct Image
{
    unsigned char type;
    unsigned short id;
    unsigned char position;
};

struct ApplicationOptions
{
    int frameRate; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
    float emissivity;
    int brightness;
};


unsigned int MemoryState::getFreeAddress(){
    return firstMemoryAddressFree;
};
unsigned int MemoryState::getSettingAddress(){
    return settingsAddress;
};

unsigned int MemoryState::getTotalMemory(){
    return totalMemory;
};
unsigned int MemoryState::getOccupiedMemory(){
    return occupiedMemory;
};

void MemoryState::setFirstMemoryAddressFree(unsigned int address){
    firstMemoryAddressFree = address;
};

void MemoryState::setOccupiedMemory(unsigned int value){
    occupiedMemory=value;
};

void MemoryState::increaseMemoryAddressFree(unsigned int increment){
    firstMemoryAddressFree+=increment;
};
bool MemoryState::increaseOccupiedMemory(unsigned int dimension){
    if(occupiedMemory+dimension>totalMemory) return false;
    occupiedMemory+=dimension;
    return true;
}
void MemoryState::setSettingAddress(unsigned int address){
    settingsAddress= address;
};

void MemoryState::scanMemory(int optionsSize){
    puts("Scanning memory");

    auto& flash=Flash::instance();
    auto buffer=make_unique<unsigned char[]>(256);
    auto header=reinterpret_cast<Header*>(buffer.get());
    auto headerImage=reinterpret_cast<Image*>(buffer.get());
    //int current_options = 0;


    for(unsigned int i=0;i<flash.sectorSize();i+=flash.pageSize())
    {
        if(flash.read(i,buffer.get(), optionsSize+sizeof(Header))==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            break; //Read error, abort
        }
        // iprintf("Pagina: %d, Type: %x\n", i/256, buffer.get()[0]);
        if(header->type==0xff && headerImage->type==0xff){
            iprintf("First free address @ address 0x%x\n",i);
            this->setFirstMemoryAddressFree(i);
            break;
        }
        if(headerImage->type==1){
            iprintf("Image found @ address 0x%x\n",i);
            this->increaseOccupiedMemory(1);
            continue;
        }
        if(header->type==0){
            if(header->invalidated==0)
            {
                iprintf("Invalidated option @ address 0x%x\n",i);
                continue; //Skip invalidated entry
            }
            if(header->crc!=crc16(buffer.get()+sizeof(Header),optionsSize))
            {
                iprintf("Corrupted option @ address 0x%x\n",i);
                continue; //Corrupted
            }
            iprintf("Found valid options @ address 0x%x\n",i);
            setSettingAddress(i);
            continue;
        }
    }
    // flash.read(1024, buffer.get(), sizeof(Image)+248);
    // iprintf("0x400 address Type: %x, 129: %x\n", buffer.get()[0], buffer.get()[178]);
};