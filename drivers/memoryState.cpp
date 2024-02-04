#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/flash.h>
#include <drivers/mlx90640.h>
#include <util/crc16.h>
#include <util/util.h>
#include "memoryState.h"
#include "inode.h"

using namespace std;
using namespace miosix;

unsigned short remaining = 63;
bool inodeFound = false;
unsigned short oldInodeAddress=0;

struct Header
{
    unsigned char type;        //0x00 if settings, 0x01 if image, 0xff if not type
    unsigned char written;     //0x00 if written,     0xff if not written
    unsigned char invalidated; //0x00 if invalidated, 0xff if not invalidated
    unsigned short crc;
};

struct InodeRead
{
    unsigned char type;
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

bool MemoryState::increaseOccupiedMemory(unsigned int dimension){
    if(occupiedMemory+dimension>totalMemory) return false;
    occupiedMemory+=dimension;
    return true;
}

void MemoryState::increaseMemoryAddressFree(unsigned short address, unsigned char type, unsigned short id, unsigned char position, unsigned int increment){
    firstMemoryAddressFree+=increment;
    
    sector->pages[occupiedMemory].address=address;
    sector->pages[occupiedMemory].type=type;
    sector->pages[occupiedMemory].id=id;
    sector->pages[occupiedMemory].position=position;
    sector->pages[occupiedMemory].used=true;

    occupiedMemory++;

    //write inode
    if(occupiedMemory==63){
        puts("Need to write inode");

        unique_ptr<Inode> test = make_unique<Inode>();
        test->setPages(sector);
        test->writeInodeToMemory(firstMemoryAddressFree);
        sector.reset(new Sector());
        setOccupiedMemory(0);
        if(inodeFound==true){
            //create map with previous inodes addresses inside first address of new data block
            unsigned short inodeAddresses[2] = {oldInodeAddress, (unsigned short) firstMemoryAddressFree};

            //generate imap and write to memory
            unique_ptr<Imap> test2 = make_unique<Imap>();
            test2->writeImapToMemory(inodeAddresses);

            //add imap to current inode
            sector->pages[0].address=firstMemoryAddressFree;
            sector->pages[0].type=(unsigned short) 3;
            sector->pages[0].used=true;

            //increase memory address free
            firstMemoryAddressFree+=increment;
        }
        firstMemoryAddressFree+=increment;
    }
};

void MemoryState::addPages(unsigned short address, unsigned char type, unsigned short id, unsigned char position)
{
    sector->pages[remaining].address=address;
    sector->pages[remaining].type=type;
    sector->pages[remaining].id=id;
    sector->pages[remaining].position=position;
    sector->pages[remaining].used=true;

    remaining--;
}

void MemoryState::scanMemory(int optionsSize){
    puts("Scanning memory");

    auto& flash=Flash::instance();
    auto buffer=make_unique<unsigned char[]>(256);
    auto header=reinterpret_cast<Header*>(buffer.get());
    auto headerImage=reinterpret_cast<Image*>(buffer.get());
    //int current_options = 0;
    iprintf("Failed to read address 0x%u\n",sizeof(Test));

    bool optionsFound=false;

    unsigned short unmarkedAddress = 0;

    //FIND FIRST FREE SECTOR
    for(unsigned int i=0;i<flash.blockSize();i+=4*flash.sectorSize()){
        if(flash.read(i,buffer.get(), optionsSize+sizeof(Header))==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            break; //Read error, abort
        }
        if(header->type==0xff && headerImage->type==0xff){
            unmarkedAddress=i-flash.pageSize();
            iprintf("first sector free: 0x%d\n", i);
            break;
        }
    }

    //cycle through all the occupied sector if not already covered by inode
    for(int j=64; j>0; j--){
        if(flash.read(unmarkedAddress,buffer.get(), optionsSize+sizeof(Header))==false)
        {
            iprintf("Failed to read address 0x%x\n",unmarkedAddress);
        }

        // if(j==16 && header->type==2){ 
        //     occupiedMemory=0; 
        //     break;
        // }

        if(header->type==0xff && headerImage->type==0xff){
            iprintf("Free address found @ address 0x%x\n",unmarkedAddress);
            this->setFirstMemoryAddressFree(unmarkedAddress);
            occupiedMemory--;
            remaining--;
        }

        if(headerImage->type==1){
            iprintf("Image found @ address 0x%x\n",unmarkedAddress);
            // this->increaseOccupiedMemory(1);
            addPages(unmarkedAddress, headerImage->type, headerImage->id, headerImage->position);
        }

        if(header->type==0){
            addPages(unmarkedAddress, header->type, 0, 0);
            if(header->invalidated==0)
            {
                iprintf("Invalidated option @ address 0x%x\n",unmarkedAddress);
            }
            if(header->crc!=crc16(buffer.get()+sizeof(Header),optionsSize))
            {
                iprintf("Corrupted option @ address 0x%x\n",unmarkedAddress);
            }
            iprintf("Found valid options @ address 0x%x\n",unmarkedAddress);
            if(!optionsFound){ 
                setSettingAddress(unmarkedAddress);
                optionsFound=true;
            }
        }

        if(header->type==3){
            puts("imap found");
            addPages(unmarkedAddress, (unsigned char)3, 0, 0);
        }

        if(header->type==2){
            puts("inode found");
            inodeFound=true;
            oldInodeAddress=unmarkedAddress;
            addPages(unmarkedAddress, (unsigned char)2, 0, 0);
        }
        
        if(unmarkedAddress==0) break;
        unmarkedAddress=unmarkedAddress-flash.pageSize();
    }

    puts("MemoryState:\n");

    for(auto page: sector->pages){
        if(page.used){
            iprintf("address: 0x%x   ", page.address);
            if(page.type==1){
                iprintf("type: Image  ");
                iprintf("position: %u\n", page.position);
            } else if(page.type==0){
                iprintf("type: Settings\n");
            } else if(page.type==2){
                iprintf("type: Inode\n");
                iprintf("type: Inode containing: \n");
                int size = sizeof(InodeRead) + 15*sizeof(Page);
                auto buffer=make_unique<unsigned char[]>(size);
                auto *pages = reinterpret_cast<Page*>(buffer.get()+sizeof(InodeRead));
                if(flash.read(oldInodeAddress,buffer.get(), size)==false)
                {
                    iprintf("Failed to read address 0x%x\n",oldInodeAddress);
                    break; //Read error, abort
                }

                for(unsigned int i=0; i<63; i++){
                    auto currElement = *(pages+i);
                    iprintf("address: 0x%x   ", currElement.address);
                    if(currElement.type==1){
                        iprintf("type: Image  ");
                        iprintf("position: %u\n", currElement.position);
                    } else if(currElement.type==0){
                        iprintf("type: Settings\n");
                    }
                }
            } else {
                iprintf("type: Imap\n");
            }
        } else {
            iprintf("empty\n");
        }
    }
    iprintf("sector pages remaining: %u\n", occupiedMemory);
};

void MemoryState::clearMemory(){
    auto& flash=Flash::instance();
    flash.eraseBlock(0);
    this->setFirstMemoryAddressFree(0);
    this->setOccupiedMemory(0);
}