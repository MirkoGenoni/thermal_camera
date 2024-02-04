#include "memoryState.h" 

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