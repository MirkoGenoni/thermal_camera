#include "inode.h"
#include <drivers/flash.h>
#include <memory>
#include <cstring>

using namespace std;

struct Header {
    unsigned char type;
};

void Inode::writeInodeToMemory(unsigned int address){
    unsigned short pagesSize = 126;
    unsigned short totalSize = pagesSize + sizeof(Header);

    iprintf("Failed to write address %u\n",totalSize);

    auto& flash=Flash::instance();
    auto buffer=make_unique<unsigned char[]>(totalSize);


    auto *header=reinterpret_cast<Header*>(buffer.get());
    header->type=2;
    memcpy(buffer.get()+sizeof(Header), mapped->pages, pagesSize);

    if(flash.write(address ,buffer.get(),totalSize)==false)
    {
        iprintf("Failed to write address 0x%x\n",address);
        return;
    }
}

void Imap::writeImapToMemory(unsigned short* address){
    iprintf("Addresses of inode: %d, %d", address[0], address[1]);
}