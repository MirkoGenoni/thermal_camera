#include "imap.h"
#include <drivers/flash.h>
#include <memory>
#include <cstring>

struct Header
{
    unsigned char type;
};

void Imap::writeImapToMemory(unsigned short imapId, unsigned int *address, unsigned short *image_ids, unsigned int freeAddress)
{
    auto &flash = Flash::instance();
    unsigned short pagesSize = sizeof(ImapStruct);
    unsigned short totalSize = pagesSize + sizeof(Header);

    // auto& flash=Flash::instance();
    auto buffer = make_unique<unsigned char[]>(totalSize);
    auto bufferImap = make_unique<unsigned char[]>(pagesSize);

    auto *imap = reinterpret_cast<ImapStruct *>(bufferImap.get());
    auto *header = reinterpret_cast<Header *>(buffer.get());

    header->type = 3;

    imap->id = imapId;
    imap->inode_addresses[0] = address[0] >> 8;
    imap->inode_addresses[1] = address[1] >> 8;

    for (int i = 0; i < 22; i++)
    {
        imap->image_ids[i] = image_ids[i];
    }
    
    memcpy(buffer.get() + sizeof(Header), imap, pagesSize);

    if (flash.write(freeAddress, buffer.get(), totalSize) == false)
    // if(flash.write(address ,buffer.get(),totalSize)==false)
    {
        iprintf("Failed to write address 0x%x\n", freeAddress);
        return;
    }
} 