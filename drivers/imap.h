#include "memoryState.h" 

struct ImapStruct
{
    unsigned short id;
    unsigned short inode_addresses[2];
    unsigned short modified_imaps[34];
    
    unsigned short image_ids[22];
};

class Imap {
    public:
        void setPages(shared_ptr<Sector> sector){
            mapped=sector;
        };

        void writeImapToMemory(unsigned short imapId, unsigned int* addresses, unsigned short* imageIds, unsigned int freeAddress);
    private:
        unsigned int clearable=0;
        shared_ptr<Sector> mapped;
};