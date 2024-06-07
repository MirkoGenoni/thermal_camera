#include <cstring>
#include <cassert>
#include <memory>
#include <iostream>
#include <list>
#include <array>
#include <set>
#include <algorithm>

#include <drivers/flash.h>

#include "debugLogger.h"
#include "inode.h"
#include "memoryState.h"
#include "imap.h"

void DebugLogger::printMemoryAddress(unsigned int address)
{
    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto header = reinterpret_cast<Image *>(buffer.get());

    if (flash.read(address, buffer.get(), 256) == false)
    {
        iprintf("Failed to read address 0x%x\n", address);
    }
    if ((unsigned short)header->type == 1)
    {
        auto image = reinterpret_cast<Image *>(buffer.get());
        iprintf("Image id: %d\n", header->id);
        iprintf("Image position: %d\n\n", (unsigned short)header->position);
    }
    if ((unsigned short)header->type == 2)
    {
        auto inode = reinterpret_cast<InodeStruct *>(buffer.get() + sizeof(ShortHeader));

        iprintf("inode id %d\n", (unsigned short)inode->id);
        for (int counter = 0; counter < 189; counter += 3)
        {
            iprintf("type: %d\n", (unsigned short)inode->content[counter]);
            iprintf("address: %x\n", (((unsigned short)inode->content[counter + 1] << 8 | (unsigned short)inode->content[counter + 2]) << 8));
        }

        puts("image_ids:");
        for (int counter2 = 0; counter2 < 12; counter2++)
        {
            iprintf("%d: %d\n", counter2, *(inode->image_ids + counter2));
        }
        puts("");
    }

    if ((unsigned short)header->type == 3)
    {
        auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));

        if (flash.read(address, buffer.get(), 256) == false)
        {
            iprintf("Failed to read address 0x%x\n", address);
        }

        iprintf("imap id %d", (unsigned short)imap->id);

        puts("Inode locations: ");

        for (auto data : imap->inode_addresses)
        {
            iprintf("address: %x\n", (unsigned short)data);
        }

        int counter = 0;
        for (auto id : imap->image_ids)
        {
            if (counter == 0)
                iprintf("inode 0 ids: ");
            if (counter == 12)
            {
                puts("");
                iprintf("inode 1 ids: ");
            }
            iprintf("%d", id);
            counter++;
        }

        counter = 0;

        puts("\nMODIFIED IMAPS:");
        for (auto imapModified : imap->modified_imaps)
        {
            iprintf("address: %x", imapModified);
        }
        iprintf("\n\n");
    }
}

void DebugLogger::printSectorPages(Page *pages)
{
    iprintf("\n\nSECTOR STATE: \n");
    for (int i = 0; i < 63; i++)
    {
        Page data = pages[i];
        iprintf("address: 0x%x  ", (data.address << 8));
        iprintf("type: %d  ", (unsigned short)data.type);
        iprintf("id: %d  ", (unsigned short)data.id);
        iprintf("position: %d\n", (unsigned short)data.position);
    }
}

void DebugLogger::printVisualizerState(std::list<std::unique_ptr<ImagesFound>> &foundL)
{
    std::list<std::unique_ptr<ImagesFound>>::iterator it;
    for (it = foundL.begin(); it != foundL.end(); ++it)
    {
        iprintf("Image: %d\n", (*it).get()->id);
        int count = 0;
        for (auto data : (*it).get()->inodeSet)
        {
            iprintf("Inode %d: 0x%x", count, data);
            count++;
        }
        count = 0;
        for (auto data : (*it).get()->imapSet)
        {
            iprintf("Imap %d: 0x%x\n", count, data);
            count++;
        }
        for (int i = 0; i < 6; i++)
        {
            iprintf("Address %d: 0x%x\n", i, (*it).get()->framesAddr[i]);
        }
        puts("");
    }
}

void DebugLogger::printMemoryState()
{
    Flash &flash = Flash::instance();

    int address = 0;
    auto buffer = make_unique<unsigned char[]>(256);
    auto header = reinterpret_cast<ShortHeader *>(buffer.get());

    bool freeFound = false;
    int counter = 1;

    while (!freeFound)
    {
        if (flash.read(address, buffer.get(), 256) == false)
            iprintf("Failed to read address: 0x%X\n", address);

        iprintf("Address: 0x%x   ", address);

        if (header->type == 1)
        {
            iprintf("Image found");
        }
        else if (header->type == 0)
        {
            iprintf("Settings found");
        }
        else if (header->type == 2)
        {
            iprintf("Inode found");
        }
        else if (header->type == 3)
        {
            iprintf("Imap found");
        }
        else if (header->type == 0xff)
        {
            iprintf("empty");
            freeFound = true;
        };

        if (counter % 5 == 0)
        {
            iprintf("\n");
        }
        else if (counter == 64)
        {
            iprintf("\n\n");
            counter = 0;
        }
        else if (counter != 0)
        {
            iprintf("        ");
        }
        counter++;
        address += 256;
    }
    puts("\n");
}

void DebugLogger::printDeletionLog(std::list<unique_ptr<InodeModified>> &inode_modified,
                      std::list<unique_ptr<ImapModified>> &imap_modified,
                      bool sectorModified,
                      Page *sector)
{

    std::list<std::unique_ptr<InodeModified>>::iterator it2;
    std::list<std::unique_ptr<ImapModified>>::iterator it3;

    if (inode_modified.size() > 0)
    {
        puts("INODE MODIFIED: ");
        for (it2 = inode_modified.begin(); it2 != inode_modified.end(); it2++)
        {
            iprintf("old inode address: %x new inode address: %x\n\n", it2->get()->oldInodeAddress, it2->get()->inodeAddress);
        }
    }
    if (imap_modified.size() > 0)
    {
        puts("IMAP MODIFIED: ");
        for (it3 = imap_modified.begin(); it3 != imap_modified.end(); it3++)
        {
            iprintf("old imap address: %x new imap address: %x\n\n", it3->get()->oldImapAddress, it3->get()->imapAddress);
        }
    }

    if (sectorModified)
    {
        for (int i=0; i<64; i++)
        {
            auto data = sector[i];
            iprintf("Type: %d\n", (unsigned short)data.type);
            if (data.type == 1)
            {
                iprintf("Id: %d\n", data.id);
                iprintf("Position: %d\n", (unsigned short)data.position);
            }
            if (data.type == 2)
                puts("inode found");

            if (data.type == 3)
                puts("imap found");

            if (data.type == 255)
                puts("free space");
        }
        puts("\n");
    }
};