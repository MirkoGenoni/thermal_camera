#include <drivers/flash.h>
#include <memory>
#include <cstring>

#include "inode.h"
#include "struct.h"

using namespace std;

void Inode::writeInodeToMemory(unsigned int address)
{
    iprintf("Writing INODE on address 0x%x\n", address);
    unsigned short pagesSize = sizeof(InodeStruct);
    unsigned short totalSize = pagesSize + sizeof(ShortHeader);

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(totalSize);
    auto bufferInode = make_unique<unsigned char[]>(pagesSize);

    auto *inode = reinterpret_cast<InodeStruct *>(bufferInode.get());
    auto *header = reinterpret_cast<ShortHeader *>(buffer.get());

    header->type = 2;

    int counter = 0;
    inode->id = (address - 256) / (256 * 64);

    for (auto data : mapped->pages)
    {
        inode->content[counter] = data.type;
        inode->content[counter + 1] = data.address >> 8 & 0xff;
        inode->content[counter + 2] = data.address & 0xff;
        counter += 3;
    }
    
    int counter2=0;
    set<unsigned short> imageIds;
    for (auto data : mapped->pages)
    {
        if (data.type == 1)
            imageIds.insert(data.id);
    }
    for (auto data : imageIds)
    {
        inode->image_ids[counter2] = data;
        counter2++;
    }

    memcpy(buffer.get() + sizeof(ShortHeader), inode, pagesSize);

    if (flash.write(address, buffer.get(), totalSize) == false)
    {
        iprintf("Failed to write address 0x%x\n", address);
        return;
    }
}

void Inode::rewriteInodeToMemory(unsigned int address, std::unique_ptr<unsigned char[]> buffer)
{
    Flash& flash = Flash::instance();
    unsigned short pagesSize = sizeof(InodeStruct);
    unsigned short totalSize = pagesSize + sizeof(ShortHeader);

    if (flash.write(address, buffer.get(), totalSize) == false)
    {
        iprintf("Failed to write address 0x%x\n", address);
        return;
    }
}