#include "inode.h"
#include <drivers/flash.h>
#include <memory>
#include <cstring>

using namespace std;

struct Header
{
    unsigned char type;
};

void Inode::writeInodeToMemory(unsigned int address)
{
    iprintf("Writing INODE on address 0x%x\n", address);
    unsigned short pagesSize = sizeof(InodeStruct);
    unsigned short totalSize = pagesSize + sizeof(Header);

    auto &flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(totalSize);
    auto bufferInode = make_unique<unsigned char[]>(pagesSize);

    auto *inode = reinterpret_cast<InodeStruct *>(bufferInode.get());
    auto *header = reinterpret_cast<Header *>(buffer.get());

    header->type = 2;

    inode->id = (address - 256) / (256 * 64);

    int counter = 0;
    int counter2=0;
    for (auto data : mapped->pages)
    {
        inode->content[counter] = data.type;
        inode->content[counter + 1] = data.address >> 8 & 0xff;
        inode->content[counter + 2] = data.address & 0xff;

        //if data inserted is image and the id is not already saved
        if (data.type==1 && (counter2 == 0 || data.id != inode->image_ids[counter2 - 1]))
        {
            //save image id in the correct field of the inode
            inode->image_ids[counter2] = data.id;
            counter2++;
        }

        counter += 3;
    }

    memcpy(buffer.get() + sizeof(Header), inode, pagesSize);

    if (flash.write(address, buffer.get(), totalSize) == false)
    {
        iprintf("Failed to write address 0x%x\n", address);
        return;
    }
}