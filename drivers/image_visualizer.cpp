#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <algorithm>

#include "inode.h"
#include "imap.h"
#include <drivers/flash.h>
#include "memoryState.h"

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

struct ImagesFound
{
    unsigned short id;

    unsigned int imap;

    unsigned int inode[2];
    int inodesNum = 0;

    unsigned int framesAddr[6];
};

void searchFrameAddresses(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    // Cycle through inode addresses and remove duplicates in order to reduce memory reading
    std::set<unsigned int> unique_inode_addresses;

    for (it = foundL.begin(); it != foundL.end(); it++)
    {
        if (it->get()->inode[0] != 0)
            unique_inode_addresses.insert((*it).get()->inode[0] << 8);
        if (it->get()->inode[1] != 0)
            unique_inode_addresses.insert((*it).get()->inode[1] << 8);
    }

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(InodeStruct));

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

    for (it = foundL.begin(); it != foundL.end(); ++it)
    {
        std::cout << "Image: " << dec << (*it).get()->id << std::endl;
        for (int i = 0; i < 6; i++)
        {
            std::cout << "Address " << i << ": 0x" << hex << (*it).get()->framesAddr[i] << std::endl;
        }
    }
}

void insertElement(std::list<std::unique_ptr<ImagesFound>> &found, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    // ID FOUND
    //  image already inserted in first five images data structure, update only the data structure
    for (it = found.begin(); it != found.end(); ++it)
    {
        if ((*it).get()->id == id)
        {
            (*it).get()->inode[(*it).get()->inodesNum] = inodeAddress;
            return;
        }
    }

    // ID NOT FOUND
    // creation of field of object to add to the data structure
    auto image = make_unique<ImagesFound>();
    image->id = id;
    image->inode[image->inodesNum] = inodeAddress;
    image->inodesNum += 1;

    // data structure not yet full, simple insertion
    if (found.size() == 0)
    {
        iprintf("Not full\n");
        found.push_front(std::move(image));
    }
    else
    {
        for (it = found.begin(); it != found.end(); ++it)
        {
            if (it->get()->id > id)
            {
                found.insert(it, std::move(image));
                break;
            }
        }
    }

    if (found.size() > 5)
    {
        found.pop_back();
    }
}

void searchImage(std::list<std::unique_ptr<ImagesFound>> &foundL, MemoryState *state)
{
    foundL.clear();
    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(InodeRead));

    // Address of the last imap
    unsigned int imapAddress = state->getFreeAddress() & 0xffff8000;

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            iprintf("Error Reading 0x%x", imapAddress);
        }

        int counter = 0;
        for (auto id : imap->image_ids)
        {
            // id part of the first inode
            if (counter < 11)
            {
                insertElement(foundL, id, imap->inode_addresses[0], imapAddress);
            }
            // id part of the second inode
            else
            {
                insertElement(foundL, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        // TODO: replace this with actual navigation through imaps
        imapAddress -= 32768;
    }

    searchFrameAddresses(foundL);
}

void insertNext(std::list<std::unique_ptr<ImagesFound>> &found, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    for (it = found.begin(); it != found.end(); ++it)
    {
        if ((*it).get()->id == id && (*it).get()->inode[0] != inodeAddress)
        {
            (*it).get()->inode[(*it).get()->inodesNum] = inodeAddress;
            return;
        }
    }

    auto image = make_unique<ImagesFound>();
    image->id = id;
    image->inode[image->inodesNum] = inodeAddress;
    image->inodesNum += 1;

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

void nextImage(std::list<std::unique_ptr<ImagesFound>> &foundL, MemoryState *state)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    for (auto data : state->getSectorState()->pages)
    {
        if (data.type == 1)
        {
            insertNext(foundL, data.id, 0, 0);
        }
    }

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(InodeRead));
    std::cout << "Need to search in another imap\n";

    // Address of the last imap
    unsigned int imapAddress = state->getFreeAddress() & 0xffff8000;

    // Search for inodes of first five elements
    while (imapAddress > 0)
    {
        if (flash.read(imapAddress, buffer.get(), 256) == false)
        {
            std::cout << "Error Reading 0x" << hex << imapAddress << std::endl;
        }

        int counter = 0;
        for (auto id : imap->image_ids)
        {
            if (counter < 11)
            {

                insertNext(foundL, id, imap->inode_addresses[0], imapAddress);
            }
            else
            {
                insertNext(foundL, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        imapAddress -= 32768;
    }

    if (foundL.size() == 6)
    {
        foundL.pop_front();
        searchFrameAddresses(foundL);
    }
    else
    {
        iprintf("\nNO NEED TO UPDATE\n");
        for (it = foundL.begin(); it != foundL.end(); ++it)
        {
            iprintf("Image: %d", (*it).get()->id);
            for (int i = 0; i < 6; i++)
            {
                iprintf("Address %d: 0x%x", i, (*it).get()->framesAddr[i]);
            }
        }
    }
}