#include "memoryState.h" 

struct InodeStruct
{
    unsigned short id;
    unsigned short image_ids[12];
    unsigned char content[189];
};

class Inode {
    public:
        void setPages(shared_ptr<Sector> sector){
            mapped=sector;
        };

        void writeInodeToMemory(unsigned int address);
        void rewriteInodeToMemory(unsigned int address, std::unique_ptr<unsigned char[]> buffer);
    private:
        unsigned int clearable=0;
        shared_ptr<Sector> mapped;
};