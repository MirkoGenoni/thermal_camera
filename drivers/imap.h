#include "memoryState.h" 

struct ImapStruct
{
    unsigned short id;
    unsigned short inode_addresses[2];
    unsigned short modified_imaps[68];
    
    unsigned short image_ids[24];
};

class Imap {
    public:
        void setPages(shared_ptr<Sector> sector){
            mapped=sector;
        };

        void writeImapToMemory(unsigned int *addresses, unsigned short *imageIds, unsigned int freeAddress, unsigned short *imap_modified);
        void rewriteImapToMemory(unsigned int address, std::unique_ptr<unsigned char[]> buffer);
    private:
        unsigned int clearable=0;
        shared_ptr<Sector> mapped;
};