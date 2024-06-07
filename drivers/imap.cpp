#include "imap.h"
#include <drivers/flash.h>
#include <memory>
#include <cstring>

void Imap::writeImapToMemory(unsigned int *address, unsigned short *image_ids, unsigned int freeAddress, unsigned short *imap_modified)
{
    auto &flash = Flash::instance();
    unsigned short pagesSize = sizeof(ImapStruct);
    unsigned short totalSize = pagesSize + sizeof(ShortHeader);

    // auto& flash=Flash::instance();
    auto buffer = make_unique<unsigned char[]>(totalSize);
    auto bufferImap = make_unique<unsigned char[]>(pagesSize);

    auto *imap = reinterpret_cast<ImapStruct *>(bufferImap.get());
    auto *header = reinterpret_cast<ShortHeader *>(buffer.get());

    header->type = 3;

    imap->id = ((freeAddress - 256) / (256 * 64)) / 2;
    imap->inode_addresses[0] = address[0] >> 8;
    imap->inode_addresses[1] = address[1] >> 8;

    for (int i = 0; i < 24; i++)
    {
        imap->image_ids[i] = image_ids[i];
    }
    
    for (int i = 0; i < 68; i++)
    {
        imap->modified_imaps[i] = imap_modified[i];
    }

    memcpy(buffer.get() + sizeof(ShortHeader), imap, pagesSize);

    if (flash.write(freeAddress, buffer.get(), totalSize) == false)
    {
        iprintf("Failed to write address 0x%x\n", freeAddress);
        return;
    }
}

void Imap::rewriteImapToMemory(unsigned int freeAddress, std::unique_ptr<unsigned char[]> buffer)
{
    Flash& flash = Flash::instance();
    unsigned short pagesSize = sizeof(ImapStruct);
    unsigned short totalSize = pagesSize + sizeof(ShortHeader);

    if(flash.write(freeAddress, buffer.get(), totalSize) == false)
    // if(flash.write(address ,buffer.get(),totalSize)==false)
    {
        iprintf("Failed to write address 0x%x\n", freeAddress);
        return;
    }
}