#include "memoryState.h" 

struct InodeStruct
{
    unsigned short id;
    unsigned short image_ids[11];
    unsigned short freeable;
    unsigned char content[189];
};

class Inode {
    public:
        void setPages(shared_ptr<Sector> sector){
            mapped=sector;
        };

        void writeInodeToMemory(unsigned int address);
    private:
        unsigned int clearable=0;
        shared_ptr<Sector> mapped;
};

class Imap {
    public:
        void setPages(shared_ptr<Sector> sector){
            mapped=sector;
        };

        void writeImapToMemory(unsigned short* address);
    private:
        unsigned int clearable=0;
        shared_ptr<Sector> mapped;
};