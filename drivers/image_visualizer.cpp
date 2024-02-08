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

void insertElement(std::list<std::unique_ptr<ImagesFound>> &found, std::array<unsigned short, 6> &ids, int *occupied, unsigned short id, unsigned int inodeAddress, unsigned int imapAddress)
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
    if (*occupied < 5)
    {
        found.push_back(std::move(image));
        ids[*occupied] = id;
        *occupied = (*occupied) + 1;
    }
    // data structure full
    else
    {
        // id in last position
        ids[5] = id;
        // sort the array of ids
        std::sort(ids.begin(), ids.end());
        // id in last position needs to be removed, not part of the first five
        for (it = found.begin(); it != found.end(); ++it)
        {
            if ((*it).get()->id == ids[5])
            {
                swap(*it, image);
            }
        }
    }
}

void searchImage(MemoryState *state)
{
    std::list<std::unique_ptr<ImagesFound>> foundL;
    std::list<std::unique_ptr<ImagesFound>>::iterator it;

    std::array<unsigned short, 6> ids;
    ids.fill(-1);
    int occupied = 0;

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(InodeRead));
    auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(InodeRead));

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
                insertElement(foundL, ids, &occupied, id, imap->inode_addresses[0], imapAddress);
            }
            // id part of the second inode
            else
            {
                insertElement(foundL, ids, &occupied, id, imap->inode_addresses[1], imapAddress);
            }
            counter++;
        }
        //TODO: replace this with actual navigation through imaps
        imapAddress -= 32768;
    }

    // DEBUG ON IMAGES AND INODES FOUND
    // for (it = foundL.begin(); it != foundL.end(); ++it)
    // {
    //     iprintf("Id: %u", (*it).get()->id);
    //     iprintf("Address inode 0: %x", ((*it).get()->inode[0] << 8));
    // }

    auto bufferImage = make_unique<unsigned char[]>(256);
    auto image = reinterpret_cast<Image *>(bufferImage.get());
    auto effectiveImage = reinterpret_cast<unsigned short *>(buffer.get() + sizeof(Image));

    // Cycle through inode addresses and remove duplicates in order to reduce memory reading
    std::set<unsigned int> unique_inode_addresses;
    for (it = foundL.begin(); it != foundL.end(); ++it)
    {
        unique_inode_addresses.insert((*it).get()->inode[0] << 8);
    }

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

    // DEBUG print image frame addresses
    // for (it = foundL.begin(); it != foundL.end(); ++it)
    // {
    //     iprintf("Image: %u", (*it).get()->id);
    //     for (int i = 0; i < 6; i++)
    //     {
    //         iprintf("Address %D: 0x%x ", i, (*it).get()->framesAddr[i]);
    //     }
    // }
}
