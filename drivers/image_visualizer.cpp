#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <algorithm>

#include <drivers/flash.h>

#include "inode.h"
#include "imap.h"
#include "image_visualizer.h"
#include "imap_navigation.h"
#include "debugLogger.h"

using namespace std;

void ImageVisualizer::searchFrameAddresses(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    // Cycle through inode addresses and remove duplicates in order to reduce memory reading
    std::set<unsigned int> unique_inode_addresses;
    std::set<unsigned int> localIds;

    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        for (auto inodeAddress : (*it).get()->inodeSet)
        {
            if (inodeAddress != 0xffff)
                unique_inode_addresses.insert(inodeAddress << 8);
            else
                localIds.insert(it->get()->id);
        }
    }

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    auto bufferImage = make_unique<unsigned char[]>(256);
    auto image = reinterpret_cast<Image *>(bufferImage.get());

    // Search addresses of frames of first five elements and adds them to data structure
    for (auto inodeAddress : unique_inode_addresses)
    {
        if (flash.read(inodeAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x", inodeAddress);
        }
        for (int i = 0; i < 189; i += 3)
        {
            if (inode->content[i] == 1)
            {
                unsigned int imageAddress = (inode->content[i + 1] << 8 | inode->content[i + 2]) << 8;
                if (flash.read(imageAddress, bufferImage.get(), 256) == false)
                {
                    iprintf("Error Reading 0x%x", imageAddress);
                }

                for (it = foundL.begin(); it != foundL.end(); ++it)
                {
                    if ((*it).get()->id == image->id)
                    {
                        (*it).get()->framesAddr[image->position] = imageAddress;
                    }
                }
            }
        }
    }

    for(auto page: memoryState->getSector().get()->pages){
        if(page.type==1 && localIds.find(page.id)!=localIds.end()){
            for (it = foundL.begin(); it != foundL.end(); ++it)
                {
                    if ((*it).get()->id == page.id)
                    {
                        (*it).get()->framesAddr[page.position] = page.address <<8;
                    }
                }
        } 
    }

    debug.get()->printVisualizerState(foundL);
}

void insertElement(std::list<std::unique_ptr<ImagesFound>> &found, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    if (id == 0)
        return;

    // ID FOUND
    //  image already inserted in first five images data structure, update only the data structure
    for (it = found.begin(); it != found.end(); ++it)
    {
        if ((*it).get()->id == id)
        {            
            (*it).get()->inodeSet.insert(inodeAddress);
            (*it).get()->imapSet.insert(imapAddress);
            return;
        }
    }

    // ID NOT FOUND
    // creation of field of object to add to the data structure
    auto image = make_unique<ImagesFound>();
    image->id = id;
    image->inodeSet.insert(inodeAddress);
    image->imapSet.insert(imapAddress);

    // data structure not yet full, simple insertion
    if (found.size() == 0)
    {
        iprintf("Not full\n");
        found.push_front(std::move(image));
    }
    else
    {
        bool inserted = false;
        for (it = found.begin(); it != found.end(); ++it)
        {
            if (it->get()->id > id)
            {
                found.insert(it, std::move(image));
                inserted = true;
                break;
            }
        }
        if (!inserted)
            found.push_back(std::move(image));
    }

    if (found.size() > 5)
    {
        found.pop_back();
    }
}

void ImageVisualizer::searchImage(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    foundL.clear();

    unsigned int imapAddress = memoryState->getFreeAddress() & 0xffff8000;
    unique_ptr<ImapNavigation> imapNavigator = make_unique<ImapNavigation>(imapAddress);

    list<ImapModifiedCache *>::iterator it;
    list<ImapModifiedCache *> imap_modified = memoryState->getImapsModified();

    Flash& flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    if (imap_modified.size() > 0)
        iprintf("IMAP modified not written in any imap\n");

    // ## IMAP MODIFIED NOT WRITTEN ##
    int counter = 0;
    for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
    {
        counter = 0;
        iprintf("Id: %d ", (*it)->id);
        iprintf("Address: 0x%x\n", ((*it)->address));
        if (imapNavigator->setLooked((*it)->id))
        {
            if (flash.read((*it)->address, buffer.get(), 256) == false)
            {
                iprintf("Error Reading 0x%x\n", ((*it)->address << 8));
            }
            for (auto id : imap->image_ids)
            {
                if (counter < 12)
                {
                    insertElement(foundL, id, imap->inode_addresses[0], (*it)->address);
                }
                else
                {
                    insertElement(foundL, id, imap->inode_addresses[1], (*it)->address);
                }
                counter++;
            }
        };
    }

    // ## IMAGE FROM CURRENT SECTOR NOT WRITTEN ##
    for (auto data : memoryState->getSector().get()->pages)
    {
        if (data.type == 1 && data.used == true)
        {
            insertElement(foundL, data.id, 0xffff, 0xffff);
        }
    }

    // ## IMAGES FROM PREVIOUS INODE ##
    counter = 0;
    if (memoryState->getInodeFound() == true)
    {
        for (auto ids : memoryState->getImagesOld())
        {
            insertElement(foundL, ids, memoryState->getOldInodeAddress() >> 8, 0xffff);
        }
    }

    // ## NAVIGATION THROUGH ALL IMAPS ##
    // Address of the last imap
    imapAddress = imapNavigator->nextAddress();

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x", imapAddress);
        }

        counter = 0;
        for (auto id : imap->image_ids)
        {
            if (counter < 12)
            {
                insertElement(foundL, id, imap->inode_addresses[0], imapAddress);
            }
            else
            {
                insertElement(foundL, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        imapAddress = imapNavigator->nextAddress();
    }

    searchFrameAddresses(foundL);
}

void insertNext(std::list<std::unique_ptr<ImagesFound>> &found, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
{
    if (id == 0)
        return;

    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    for (it = found.begin(); it != found.end(); ++it)
    {
        if ((*it).get()->id == id)
        {
            (*it).get()->inodeSet.insert(inodeAddress);
            (*it).get()->imapSet.insert(imapAddress);
            return;
        }
    }

    auto image = make_unique<ImagesFound>();
    image->id = id;
    image->inodeSet.insert(inodeAddress);
    image->imapSet.insert(imapAddress);

    auto last = std::prev(found.end(), 1);
    if (found.size() == 5 && id > last->get()->id)
    {
        found.push_back(std::move(image));
    }
    else
    {
        auto secondToLast = std::prev(found.end(), 2);

        if (id > secondToLast->get()->id && id < last->get()->id)
        {
            found.insert(last, std::move(image));
            found.pop_back();
        }
    }
}

void ImageVisualizer::nextImage(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();
    puts("\n\nNEXT IMAGES\n");
    Flash& flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    unsigned int imapAddress = memoryState->getFreeAddress() & 0xffff8000;
    unique_ptr<ImapNavigation> imapNavigator = make_unique<ImapNavigation>(imapAddress);

    list<ImapModifiedCache *>::iterator it;
    list<ImapModifiedCache *> imap_modified = memoryState->getImapsModified();

    // ## IMAP MODIFIED NOT WRITTEN ##
    int counter = 0;
    for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
    {
        counter = 0;
        if (imapNavigator->setLooked((*it)->id))
        {
            if (flash.read((*it)->address, buffer.get(), 256) == false)
            {
                iprintf("Error Reading 0x%x\n", ((*it)->address << 8));
            }
            for (auto id : imap->image_ids)
            {
                if (counter < 12)
                {
                    insertNext(foundL, id, imap->inode_addresses[0], (*it)->address);
                }
                else
                {
                    insertNext(foundL, id, imap->inode_addresses[1], (*it)->address);
                }
                counter++;
            }
        };
    }

    // ## IMAGE FROM CURRENT SECTOR NOT WRITTEN ##
    for (auto data : memoryState->getSector().get()->pages)
    {
        // IMAGE IN MEMORY NOT COVERED BY INODE OR IMAP
        if (data.type == 1 && data.used == true)
        {
            insertNext(foundL, data.id, 0xffff, 0xffff);
        }
    }

    counter = 0;

    // ## IMAGES FROM PREVIOUS INODE ##
    // CHECK IF IMAGE OR IMAP IN MEMORY COVERED BY PREVIOUS INODE
    if (memoryState->getInodeFound() == true)
    {
        // IF IMAGE FOUND, ADDS IMAGE WITH THE ADDRESS OF INODE AND IMAP ADDRESS 0xffff
        for (auto ids : memoryState->getImagesOld())
        {
            insertNext(foundL, ids, memoryState->getOldInodeAddress() >> 8, 0xffff);
        }
    }

    // ## NAVIGATION THROUGH ALL IMAPS ##
    // Address of the last imap
    imapAddress = imapNavigator->nextAddress();

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x\n", imapAddress);
        }

        counter = 0;
        for (auto id : imap->image_ids)
        {
            if (counter < 12)
            {
                insertNext(foundL, id, imap->inode_addresses[0], imapAddress);
            }
            else
            {
                insertNext(foundL, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        imapAddress = imapNavigator->nextAddress();
    }

    if (foundL.size() == 6)
    {
        foundL.pop_front();
        searchFrameAddresses(foundL);
    }
    else
    {
        puts("NO NEED TO UPDATE");
        debug.get()->printVisualizerState(foundL);
    }
}

void insertPrev(std::list<std::unique_ptr<ImagesFound>> &found, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
{
    if (id == 0)
        return;

    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    for (it = found.begin(); it != found.end(); ++it)
    {
        if ((*it).get()->id == id)
        {
            (*it).get()->inodeSet.insert(inodeAddress);
            (*it).get()->imapSet.insert(imapAddress);

            return;
        }
    }

    auto image = make_unique<ImagesFound>();
    image->id = id;
    image->inodeSet.insert(inodeAddress);
    image->imapSet.insert(imapAddress);

    if (found.size() == 5 && id < found.begin()->get()->id)
    {
        found.push_front(std::move(image));
    }
    else
    {
        auto secondElement = std::next(found.begin(), 1);
        auto thirdElement = std::next(found.begin(), 2);
        if (id < secondElement->get()->id && id > found.begin()->get()->id)
        {
            found.insert(secondElement, std::move(image));
            found.pop_front();
        }
    }
}

void ImageVisualizer::prevImage(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();
    puts("\n\nPREV IMAGES\n");
    Flash& flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    unsigned int imapAddress = memoryState->getFreeAddress() & 0xffff8000;
    unique_ptr<ImapNavigation> imapNavigator = make_unique<ImapNavigation>(imapAddress);

    list<ImapModifiedCache *>::iterator it;
    list<ImapModifiedCache *> imap_modified = memoryState->getImapsModified();

    // ## IMAP MODIFIED NOT WRITTEN ##
    int counter = 0;
    for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
    {
        counter = 0;
        puts("\nNEW IMAP NOT WRITTEN");
        iprintf("Id: %d ", (*it)->id);
        iprintf("Address: 0x%x\n", ((*it)->address));
        if (imapNavigator->setLooked((*it)->id))
        {
            if (flash.read((*it)->address, buffer.get(), 256) == false)
            {
                iprintf("Error Reading 0x%x\n", ((*it)->address << 8));
            }
            for (auto id : imap->image_ids)
            {
                if (counter < 12)
                {
                    insertPrev(foundL, id, imap->inode_addresses[0], (*it)->address);
                }
                else
                {
                    insertPrev(foundL, id, imap->inode_addresses[1], (*it)->address);
                }
                counter++;
            }
        };
    }

    // ## IMAGE FROM CURRENT SECTOR NOT WRITTEN ##
    for (auto data : memoryState->getSector().get()->pages)
    {
        // IMAGE IN MEMORY NOT COVERED BY INODE OR IMAP
        if (data.type == 1)
        {
            insertPrev(foundL, data.id, 0xffff, 0xffff);
        }
    }

    // ## IMAGES FROM PREVIOUS INODE ##
    // CHECK IF IMAGE OR IMAP IN MEMORY COVERED BY PREVIOUS INODE
    if (memoryState->getInodeFound() == true)
    {
        // IF IMAGE FOUND, ADDS IMAGE WITH THE ADDRESS OF INODE AND IMAP ADDRESS 0xffff
        for (auto ids : memoryState->getImagesOld())
        {
            insertPrev(foundL, ids, memoryState->getOldInodeAddress() >> 8, 0xffff);
        }
    }

    // ## NAVIGATION THROUGH ALL IMAPS ##
    // Address of the last imap
    imapAddress = imapNavigator->nextAddress();

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x\n", imapAddress);
        }

        counter = 0;
        for (auto id : imap->image_ids)
        {
            if (counter < 12)
            {

                insertPrev(foundL, id, imap->inode_addresses[0], imapAddress);
            }
            else
            {
                insertPrev(foundL, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        imapAddress = imapNavigator->nextAddress();
    }

    if (foundL.size() == 6)
    {
        foundL.pop_back();
        searchFrameAddresses(foundL);
    }
    else
    {
        puts("\nNO NEED TO UPDATE");
        debug.get()->printVisualizerState(foundL);
    }
}

void updateCurrentView(std::list<std::unique_ptr<ImagesFound>> &foundL, std::list<unique_ptr<InodeModified>> &inode_modified, std::list<unique_ptr<ImapModified>> &imap_modified, unsigned short id)
{

    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    std::list<std::unique_ptr<InodeModified>>::iterator it2;
    std::list<std::unique_ptr<ImapModified>>::iterator it3;

    std::set<unsigned int>::iterator it4;

    for (it2 = inode_modified.begin(); it2 != inode_modified.end(); it2++)
    {
        puts("\n\nMODIFIED: ");
        iprintf("INODEADDRESS: %x\n", it2->get()->inodeAddress);
        iprintf("OLDINODEADDRESS: %x\n", it2->get()->oldInodeAddress);

        for (it = foundL.begin(); it != foundL.end(); ++it)
        {
            auto inodeSet = it->get()->inodeSet;
            if (inodeSet.find(it2->get()->oldInodeAddress >> 8) != inodeSet.end())
            {
                it->get()->inodeSet.erase(it->get()->inodeSet.find(it2->get()->oldInodeAddress >> 8));
                it->get()->inodeSet.insert(it2->get()->inodeAddress >> 8);
            }
        }
    }

    for (it3 = imap_modified.begin(); it3 != imap_modified.end(); it3++)
    {
        for (it = foundL.begin(); it != foundL.end(); ++it)
        {
            auto imapSet = it->get()->imapSet;
            if (imapSet.find(it3->get()->oldImapAddress) != imapSet.end())
            {
                it->get()->imapSet.erase(it->get()->imapSet.find(it3->get()->oldImapAddress));
                it->get()->imapSet.insert(it3->get()->imapAddress);
            }
        }
    }

    for (it = foundL.begin(); it != foundL.end(); ++it)
    {
        if (it->get()->id == id)
        {
            foundL.erase(it);
            break;
        }
    }

    auto image = make_unique<ImagesFound>();
    image->id = 0;
    foundL.push_front(std::move(image));
}

void addNewImapToCurrentView(std::list<std::unique_ptr<ImagesFound>> &foundL, unsigned int newImapAddress)
{
    iprintf("IMAP WRITTEN ON: 0x%x\n", newImapAddress);
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        auto remove = it->get()->imapSet.find(0xffff);
        if (remove != it->get()->imapSet.end())
        {
            puts("FOUND EMPTY IMAP TO BE FILLED");
            it->get()->imapSet.erase(it->get()->imapSet.find(0xffff));
            it->get()->imapSet.insert(newImapAddress);
        }
    }
}

bool ImageVisualizer::deleteImage(std::list<std::unique_ptr<ImagesFound>> &foundL, unsigned short id)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    unique_ptr<DebugLogger> debug = make_unique<DebugLogger>();

    std::list<unique_ptr<InodeModified>> inode_modified;
    std::list<unique_ptr<ImapModified>> imap_modified;

    bool sectorModified = false;
    bool additionalUpdate = false;
    unsigned int newImapAddress = 0;

    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        auto element = it->get();
        if (element->id == id)
        {
            for (auto inode : element->inodeSet)
            {
                if (inode == 0xffff)
                {
                    memoryState->updateCurrentSector(id);
                    memoryState->writeMockInode();
                    sectorModified = true;
                }
                else
                {
                    unsigned int newInodeAddress = memoryState->rewriteInode(inode << 8, id);
                    if ((newInodeAddress + 512) % 16384 == 0)
                    {
                        additionalUpdate = true;
                        newImapAddress = newInodeAddress + 512;
                    }
                    unique_ptr<InodeModified> modified = make_unique<InodeModified>();
                    modified->inodeAddress = newInodeAddress;
                    modified->oldInodeAddress = inode << 8;
                    inode_modified.push_front(std::move(modified));
                }
            }
        }
    }

    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        auto element = it->get();
        if (element->id == id)
        {
            for (auto imap : element->imapSet)
            {
                if (imap != 0xffff)
                {
                    unsigned int newImapAddress = memoryState->rewriteImap(imap, id, inode_modified);
                    unique_ptr<ImapModified> modified = make_unique<ImapModified>();
                    modified->imapAddress = newImapAddress;
                    modified->oldImapAddress = imap;
                    imap_modified.push_front(std::move(modified));
                }
                else
                {
                    memoryState->updateOldInodeAddress(inode_modified, id);
                }
            }
        }
    }

    if (additionalUpdate == true)
    {
        iprintf("IMAP WRITTEN ON: 0x%x\n", newImapAddress);
        addNewImapToCurrentView(foundL, newImapAddress);
    }

    updateCurrentView(foundL, inode_modified, imap_modified, id);
    debug.get()->printDeletionLog(inode_modified, imap_modified,sectorModified, memoryState->getSector()->pages);
    nextImage(foundL);
    return true;
}

unsigned short ImageVisualizer::findMaxId(){
    unsigned int imapAddress = memoryState->getFreeAddress() & 0xffff8000;
    unique_ptr<ImapNavigation> imapNavigator = make_unique<ImapNavigation>(imapAddress);

    list<ImapModifiedCache *>::iterator it;
    list<ImapModifiedCache *> imap_modified = memoryState->getImapsModified();

    Flash& flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

    if (imap_modified.size() > 0)
        iprintf("IMAP modified not written in any imap\n");

    unsigned short maxId=0;

    // ## IMAP MODIFIED NOT WRITTEN ##
    int counter = 0;
    for (it = imap_modified.begin(); it != imap_modified.end(); ++it)
    {
        if (imapNavigator->setLooked((*it)->id))
        {

            if (flash.read((*it)->address, buffer.get(), 256) == false)
            {
                iprintf("Error Reading 0x%x\n", ((*it)->address << 8));
            }            
            for(int i=0; i<22; i++){
                if(imap->image_ids[i]>maxId) maxId=imap->image_ids[i];
            }
        };
    }

    // ## IMAGE FROM CURRENT SECTOR NOT WRITTEN ##
    for (auto data : memoryState->getSector().get()->pages)
    {
        if (data.type == 1 && data.used == true)
        {
            if(data.id > maxId) maxId = data.id;
        }
    }

    // ## IMAGES FROM PREVIOUS INODE ##
    counter = 0;
    if (memoryState->getInodeFound() == true)
    {
        for (auto ids : memoryState->getImagesOld())
        {
            if(ids > maxId) maxId = ids;
        }
    }

    // ## NAVIGATION THROUGH ALL IMAPS ##
    // Address of the last imap
    imapAddress = imapNavigator->nextAddress();

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x", imapAddress);
        }

        for (auto id : imap->image_ids)
        {
            if(id > maxId) maxId = id;
        }
        imapAddress = imapNavigator->nextAddress();
    }
    return maxId+1;
}