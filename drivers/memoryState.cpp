#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <algorithm>

#include <drivers/flash.h>
#include <drivers/mlx90640.h>
#include <util/crc16.h>
#include <util/util.h>

#include "memoryState.h"
#include "inode.h"
#include "imap.h"
#include "struct.h"
#include "debugLogger.h"

using namespace std;
using namespace miosix;

bool MemoryState::increaseOccupiedMemory(unsigned int dimension)
{
    if (occupiedMemory + dimension > totalMemory)
        return false;
    occupiedMemory += dimension;
    return true;
}

void MemoryState::increaseMemoryAddressFree(unsigned int address, unsigned char type, unsigned short id, unsigned char position, unsigned int increment){
    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();
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

        // SAVE CURRENT IMAGE IDs INSIDE INODE INSIDE LOCAL VARIABLE
        if (inodeFound == false)
        {
            imageIds_0.clear();
            imageIds_1.clear();
        }
        for (auto data : sector->pages)
        {
            if (data.type == 1)
            {
                if (inodeFound == true)
                {
                    imageIds_1.insert(data.id);
                }
                else
                {
                    imageIds_0.insert(data.id);
                }
            }
        }

        currentInode->writeInodeToMemory(firstMemoryAddressFree);
        debug->printMemoryAddress(firstMemoryAddressFree);
        sector.reset(new Sector());
        setOccupiedMemory(0);

        if (inodeFound == true)
        {
            // create map with previous inodes addresses inside first address of new data block
            unsigned int inodeAddresses[2] = {oldInodeAddress, (unsigned int)firstMemoryAddressFree};

            firstMemoryAddressFree += 256;
            // generate imap and write to memory
            unique_ptr<Imap> imap = make_unique<Imap>();

            unsigned short imageIds[24];
            // ARRAY INITIALIZATION
            for (int i = 0; i < 24; i++)
            {
                imageIds[i] = 0;
            }

            int counter = 0;
            for (auto ids : imageIds_0)
            {
                imageIds[counter] = ids;
                counter++;
            }
            counter = 12;
            for (auto ids : imageIds_1)
            {
                imageIds[counter] = ids;
                counter++;
            }

            unsigned short imapModified[68];
            // ARRAY INITIALIZATION
            for (int i = 0; i < 68; i++)
            {
                imapModified[i] = 0xff;
            }

            counter = 0;

            std::list<std::unique_ptr<ImapModifiedCache>>::iterator it;

            for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
            {
                imapModified[counter] = it->get()->address >> 8;
                counter++;
            }

            imap->writeImapToMemory(inodeAddresses, imageIds, firstMemoryAddressFree, imapModified);

            // add imap to current inode
            sector->pages[0].address = firstMemoryAddressFree >> 8;
            sector->pages[0].type = (unsigned short)3;
            sector->pages[0].id = ((firstMemoryAddressFree - 256) / (256 * 64)) / 2;
            sector->pages[0].used = true;

            occupiedMemory++;

            imageIds_0.clear();
            imageIds_1.clear();
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
    sector->pages[initialRemaining].address=address >> 8;
    sector->pages[initialRemaining].type=type;
    sector->pages[initialRemaining].id=id;
    sector->pages[initialRemaining].position=position;
    sector->pages[initialRemaining].used=true;

    initialRemaining--;
}

void MemoryState::scanMemory(int optionsSize){
    puts("Scanning memory");

    auto& flash=Flash::instance();
    auto buffer=make_unique<unsigned char[]>(256);
    auto header=reinterpret_cast<Header*>(buffer.get());
    auto headerImage=reinterpret_cast<Image*>(buffer.get());
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    bool optionsFound=false;

    unsigned int unmarkedAddress = 0;

    bool previousInodeModified = false;

    /*
        ## SCAN THROUGH ALL THE LAST ADDRESSES OF SECTOR TO FIND FREE PAGE OR INODE ##
    */
    for(unsigned int i=63*flash.pageSize();i<flash.size(); i+=4*flash.sectorSize()){
        if(flash.read(i,buffer.get(), flash.pageSize())==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            break; //Read error, abort
        }

        if (header->type == 0xff && headerImage->type == 0xff)
        {
            unmarkedAddress = i;
            iprintf("first sector free: 0x%x\n", i);
            break;
        }
        if (header->type == 2)
        {
            auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));
            inodeFound = !inodeFound;
            oldInodeAddress = i;
            oldInodeId = inode->id;
        }
    }

    /*
        ## SCAN ALL PAGES INSIDE CURRENT INODE
    */
    for(int j=64; j>0; j--){
        if(flash.read(unmarkedAddress,buffer.get(), 256)==false)
        {
            iprintf("Failed to read address 0x%x\n",unmarkedAddress);
        }

        if (header->type == 0xff && headerImage->type == 0xff)
        {
            iprintf("Free address found @ address 0x%x\n",unmarkedAddress);
            this->setFirstMemoryAddressFree(unmarkedAddress);
            occupiedMemory--;
            initialRemaining--;
        }

        if (headerImage->type == 1)
        {
            iprintf("Image found @ address 0x%x\n", unmarkedAddress);
            // this->increaseOccupiedMemory(1);
            addPages(unmarkedAddress, headerImage->type, headerImage->id, headerImage->position);
        }

        if (header->type == 0)
        {
            iprintf("Found valid options @ address 0x%x\n", unmarkedAddress);
            addPages(unmarkedAddress, header->type, 0, 0);
            if (!optionsFound)
            {
                setSettingAddress(unmarkedAddress);
                optionsFound = true;
            }
        }

        if (header->type == 3)
        {
            puts("imap found");

            // Imap in current pages and not in first position means different imap modified
            if (unmarkedAddress % 32768 != 0)
            {
                unique_ptr<ImapModifiedCache> ImapModified = make_unique<ImapModifiedCache>();
                ImapModified->id = imap->id;
                ImapModified->address = unmarkedAddress;
                imap_modified.push_back(std::move(ImapModified));
            }

            addPages(unmarkedAddress, (unsigned char)3, imap->id, 0);
        }

        if (header->type == 2)
        {
            puts("inode found");
            if (inode->id == (unmarkedAddress - 256) / (256 * 64))
            {
                iprintf("\nMock INODE found at 0x%x\n\n", unmarkedAddress);
                int counter2 = 0;
                for (int counter = 0; counter < 189; counter += 3)
                {
                    unsigned int containedAddress = ((unsigned short)inode->content[counter + 1] << 8 | (unsigned short)inode->content[counter + 2]);
                    unsigned char type = inode->content[counter];

                    iprintf("Here address 0x%x\n", containedAddress);

                    // TOFIX: fix behaviour from actual flash memory
                    if ((unsigned short)type == 0 && containedAddress == 0 && counter != 0)
                    {
                        puts("Address free in mock inode");
                        break;
                    }

                    sector->pages[counter2].type = type;
                    sector->pages[counter2].address = containedAddress;

                    auto bufferInodeRead = make_unique<unsigned char[]>(256);
                    auto headerImage = reinterpret_cast<Image *>(bufferInodeRead.get());
                    auto headerInode = reinterpret_cast<InodeStruct *>(bufferInodeRead.get() + sizeof(ShortHeader));
                    auto headerImap = reinterpret_cast<ImapStruct *>(bufferInodeRead.get() + sizeof(ShortHeader));

                    if (flash.read(containedAddress << 8, bufferInodeRead.get(), sizeof(Image)) == false)
                    {
                        iprintf("Failed to read address 0x%x\n", unmarkedAddress);
                    }

                    // CASE: The mock inode already contained the previous inode modified
                    if ((unsigned short)type == 2 && oldInodeId == headerInode->id && !previousInodeModified)
                    {
                        puts("Old inode found");
                        oldInodeAddress = containedAddress << 8;
                    }

                    // CASE: The mock inode already contained a previous imap modified
                    if ((unsigned short)type == 3 && containedAddress % 128 != 0)
                    {
                        unique_ptr<ImapModifiedCache> ImapModified = make_unique<ImapModifiedCache>();
                        ImapModified->id = headerImap->id;
                        ImapModified->address = containedAddress << 8;
                        imap_modified.push_front(std::move(ImapModified));
                    }
                    sector->pages[counter2].id = headerImage->id;
                    sector->pages[counter2].position = headerImage->position;
                    sector->pages[counter2].used=true;
                    counter2++;
                }
                addPages(unmarkedAddress, (unsigned char)2, inode->id, 0);
                break;
            }

            if (oldInodeId == inode->id && !previousInodeModified)
            {
                puts("Modified previous inode");
                oldInodeAddress = unmarkedAddress;
                previousInodeModified = true;
            }
            addPages(unmarkedAddress, (unsigned char)2, inode->id, 0);
        }

        if (unmarkedAddress == 0)
            break;
        unmarkedAddress = unmarkedAddress - 256;
    }

    /*
        ## RETRIEVE INFORMATION ON OLD INODE ##
    */
    if (inodeFound)
    {
        if (flash.read(oldInodeAddress, buffer.get(), 256) == false)
        {
            iprintf("Failed to read address 0x%x\n", unmarkedAddress);
        }

        // IMAGES CONTAINED
        imageIds_0.clear();
        imageIds_1.clear();
        for (int i = 0; i < 12; i++)
        {
            imageIds_0.insert(inode->image_ids[i]);
        }
            
        // IMAP MODIFIED CONTAINED
        for (int i = 0; i < 189; i += 3)
        {
            unsigned int imapModifiedAddress = (inode->content[i + 1] << 8 | inode->content[i + 2]) << 8;
            if (inode->content[i] == 3 && imapModifiedAddress % 32768 != 0)
            {
                puts("\n\nSCANNING MODIFIED IMAPS IN OLD INODE\n");
                auto bufferImap = make_unique<unsigned char[]>(256);
                auto imapModifiedPointer = reinterpret_cast<ImapStruct *>(bufferImap.get() + sizeof(ShortHeader));

                if (flash.read(imapModifiedAddress, bufferImap.get(), 256) == false)
                {
                    iprintf("Failed to read address 0x%x\n", unmarkedAddress);
                }

                unique_ptr<ImapModifiedCache> ImapModified = make_unique<ImapModifiedCache>();
                ImapModified->id = imapModifiedPointer->id;
                ImapModified->address = imapModifiedAddress;
                imap_modified.push_back(std::move(ImapModified));
            }
        }

        std::list<std::unique_ptr<ImapModifiedCache>>::iterator it;

        for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
        {
            iprintf("%d: %x\n", it->get()->id, it->get()->address);
        }
    }

    //TODO: add handling of no images in current imap, search for last id in previous imaps 
    if (currentImageId == (unsigned short)-1)
        currentImageId = 1;

    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();
    debug.get()->printMemoryState();
    debug.get()->printSectorPages(sector->pages);

    iprintf("sector pages occupied: %d\n\n", occupiedMemory);
};

unsigned int MemoryState::rewriteInode(unsigned int address, unsigned short imageId)
{
    auto& flash=Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto header = reinterpret_cast<ShortHeader *>(buffer.get());
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    auto bufferImage = make_unique<unsigned char[]>(256);
    auto image_header = reinterpret_cast<Image *>(bufferImage.get());
    if (flash.read(address, buffer.get(), 256) == false)
    {
        iprintf("Failed to read address 0x%x\n", address);
    }

    int counter2 = 0;
    for (auto data : inode->image_ids)
    {
        if (data == imageId)
        {
            inode->image_ids[counter2] = 0;
        }
        counter2++;
    }

    for (int counter = 0; counter < 189; counter += 3)
    {
        unsigned short contentType = (unsigned short)inode->content[counter];
        if (contentType == 1)
        {
            unsigned int image = (((unsigned short)inode->content[counter + 1] << 8 | (unsigned short)inode->content[counter + 2]) << 8);
            if (flash.read(image, bufferImage.get(), 256) == false)
            {
                iprintf("Failed to read address 0x%x\n", image);
            }
            if (image_header->type == 1 && image_header->id == imageId)
            {
                inode->content[counter] = 255;
            }
        }
    }

    unique_ptr<Inode> currentInode = make_unique<Inode>();
    currentInode->rewriteInodeToMemory(firstMemoryAddressFree, std::move(buffer));
    if (occupiedMemory == 62)
    {
        if (imageIds_0.find(imageId) != imageIds_0.end())
            imageIds_0.erase(imageIds_0.find(imageId));
        if (address == this->oldInodeAddress)
        {
            this->oldInodeAddress = firstMemoryAddressFree;
            iprintf("MODIFIED OLD INODE ADDRESS FROM: %x TO: %x\n", address, firstMemoryAddressFree);
        }
    }
    unsigned int currentAddress = firstMemoryAddressFree;
    increaseMemoryAddressFree(firstMemoryAddressFree, 2, inode->id, 0, 256);
    return currentAddress;
};

unsigned int MemoryState::rewriteImap(unsigned int address, unsigned short imageId, std::list<std::unique_ptr<InodeModified>> &foundL)
{
    std::list<std::unique_ptr<InodeModified>>::iterator it;
    auto& flash=Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto header = reinterpret_cast<ShortHeader *>(buffer.get());
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));

    if (flash.read(address, buffer.get(), 256) == false)
    {
        iprintf("Failed to read address 0x%x\n", address);
    }

    int counter = 0;
    for (auto id : imap->image_ids)
    {
        if (id == imageId)
        {
            imap->image_ids[counter] = 0;
        }
        counter++;
    }

    set<list<std::unique_ptr<InodeModified>>::iterator> toRemove;

    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        int counter2 = 0;
        for (auto inodeAdresses : imap->inode_addresses)
        {
            if (inodeAdresses == it->get()->oldInodeAddress >> 8)
            {
                imap->inode_addresses[counter2] = it->get()->inodeAddress >> 8;
                break;
            }
            counter2++;
        }
    }

    unique_ptr<ImapModifiedCache> imapFS = make_unique<ImapModifiedCache>();
    imapFS.get()->id = imap->id;
    imapFS.get()->address = firstMemoryAddressFree;
    iprintf("\n\nWRITING MODIFIED: %x\n", imapFS.get()->address);
    imap_modified.push_front(std::move(imapFS));

    unique_ptr<Imap> currentImap = make_unique<Imap>();
    currentImap->rewriteImapToMemory(firstMemoryAddressFree, std::move(buffer));

    increaseMemoryAddressFree(firstMemoryAddressFree, 3, imap->id, 0, 256);

    return firstMemoryAddressFree - 256;
};

void MemoryState::updateOldInodeAddress(std::list<std::unique_ptr<InodeModified>> &foundL, unsigned short id)
{
    if (imageIds_0.find(id) == imageIds_0.end())
        return;
    imageIds_0.erase(imageIds_0.find(id));

    std::list<std::unique_ptr<InodeModified>>::iterator it;
    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        if (it->get()->oldInodeAddress == this->oldInodeAddress)
        {
            this->oldInodeAddress = it->get()->inodeAddress;
            iprintf("MODIFIED OLD INODE ADDRESS FROM: %x TO : %x\n", it->get()->oldInodeAddress, this->oldInodeAddress);
        }
    }
};

void MemoryState::updateCurrentSector(unsigned short id)
{
    int counter = 0;
    for (auto page : sector->pages)
    {
        if (page.type == 1 && page.id == id)
        {
            sector->pages[counter].type = 255;
        }
        counter++;
    }
};

void MemoryState::writeMockInode()
{
    unique_ptr<Inode> currentInode = make_unique<Inode>();
    currentInode->setPages(sector);
    currentInode->writeInodeToMemory(firstMemoryAddressFree);
    increaseMemoryAddressFree(firstMemoryAddressFree, 2, 0xffff, 0, 256);
};

void MemoryState::clearMemory(){
    auto& flash=Flash::instance();
    flash.eraseBlock(0);
    flash.eraseBlock(flash.blockSize());
    flash.eraseBlock(2*flash.blockSize());
    this->setFirstMemoryAddressFree(0);
    this->setOccupiedMemory(0);    
    for(int i=0; i<63; i++){
        sector.get()->pages[i].address=0;
        sector.get()->pages[i].id=0;
        sector.get()->pages[i].position=0;
        sector.get()->pages[i].type=0;
        sector.get()->pages[i].used=false;
    }
    currentImageId=1;
}