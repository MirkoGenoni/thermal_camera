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

void MemoryState::increaseMemoryAddressFree(unsigned int address, unsigned char type, unsigned short id, unsigned char position, unsigned int increment){
    firstMemoryAddressFree+=increment;
    
    sector->pages[occupiedMemory].address=address>>8;
    sector->pages[occupiedMemory].type=type;
    sector->pages[occupiedMemory].id=id;
    sector->pages[occupiedMemory].position=position;
    sector->pages[occupiedMemory].used=true;

    occupiedMemory++;

    //write inode
    if(occupiedMemory==63){
        puts("Need to write inode");

        unique_ptr<Inode> currentInode = make_unique<Inode>();
        currentInode->setPages(sector);
        
        currentInode->writeInodeToMemory(firstMemoryAddressFree);
        sector.reset(new Sector());
        setOccupiedMemory(0);

        if(inodeFound==true){
            //create map with previous inodes addresses inside first address of new data block
            unsigned short inodeAddresses[2] = {oldInodeAddress, (unsigned short) firstMemoryAddressFree};

            firstMemoryAddressFree += 256;
            //generate imap and write to memory
            unique_ptr<Imap> test2 = make_unique<Imap>();

            //ONLY TEST PRINT, TODO: add imap
            test2->writeImapToMemory(inodeAddresses);
            inodeFound = false;
        }
        else
        {
            inodeFound = true;
            oldInodeAddress = firstMemoryAddressFree;
        }
        firstMemoryAddressFree+=increment;
    }
};

void MemoryState::addPages(unsigned int address, unsigned char type, unsigned short id, unsigned char position)
{
    sector->pages[remaining].address=address >> 8;
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

    bool optionsFound=false;

    unsigned int unmarkedAddress = 0;

    //FIND FIRST FREE SECTOR
    for(unsigned int i=(4*flash.sectorSize())-flash.pageSize();i<flash.size(); i+=4*flash.sectorSize()){
        if(flash.read(i,buffer.get(), flash.pageSize())==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            break; //Read error, abort
        }
        if (header->type == 2)
        {
            inodeFound = !inodeFound;
            oldInodeAddress = i;
        }
        if(header->type==0xff && headerImage->type==0xff){
            unmarkedAddress=i;
            iprintf("first sector free: 0x%d\n", i);
            break;
        }
    }

    //cycle through all the occupied sector if not already covered by inode
    for(int j=64; j>0; j--){
        if(flash.read(unmarkedAddress,buffer.get(), 256)==false)
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
            if (currentImageId == -1)
                currentImageId = headerImage->id + 1;
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
            addPages(unmarkedAddress, (unsigned char)2, 0, 0);
        }
        
        if(unmarkedAddress==0) break;
        unmarkedAddress=unmarkedAddress-flash.pageSize();
    }

    if ((short)currentImageId == -1)
        currentImageId = 0;

    //PRINT MEMORY COVERED BY CURRENT INODE
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
            } else {
                iprintf("type: Imap\n");
            }
        } else {
            iprintf("empty\n");
        }
    }
    iprintf("memory occupied: %u\n", occupiedMemory);
    
    //Reads and prints out first inode for DEBUG 
    auto bufferT=make_unique<unsigned char[]>(256);
    auto headerT = reinterpret_cast<InodeRead*>(bufferT.get());
    auto inodeT = reinterpret_cast<InodeStruct*>(bufferT.get()+sizeof(InodeRead));

    if(flash.read(oldInodeAddress, bufferT.get(), 255)==false)
    {
        iprintf("Failed to read address 0x%x\n",oldInodeAddress);
    }

    if(headerT->type==2){
        iprintf("TYPE READ: %u \n" , headerT->type);

        puts("WRITTEN INODE: \n");
        iprintf("inode id: %u " ,(unsigned short)inodeT->id);
        for (int counter = 0; counter < 189; counter += 3)
        {
            iprintf("type: %u   ", (unsigned short)inodeT->content[counter]);
            iprintf("address: %x\n", (((unsigned short)inodeT->content[counter + 1] << 8 | (unsigned short)inodeT->content[counter + 2]) << 8));
        }
        
        puts("image_ids:");
        for (int counter2 = 0; counter2 < 11; counter2++)
        {
            iprintf("%d: %u \n", counter2, *(inodeT->image_ids + counter2));
        }
    }
};

void MemoryState::clearMemory(){
    auto& flash=Flash::instance();
    flash.eraseBlock(0);
    flash.eraseBlock(flash.blockSize());
    flash.eraseBlock(2*flash.blockSize());
    this->setFirstMemoryAddressFree(0);
    this->setOccupiedMemory(0);
}