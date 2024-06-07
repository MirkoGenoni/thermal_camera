#include <list>
#include <memory>

#include "memoryState.h"
#include "struct.h"

class DebugLogger
{
public:
    void printMemoryAddress(unsigned int address);
    void printSectorPages(Page *pages);
    void printVisualizerState(std::list<std::unique_ptr<ImagesFound>> &foundL);
    void printMemoryState();
    void printDeletionLog(std::list<unique_ptr<InodeModified>> &inode_modified,
                          std::list<unique_ptr<ImapModified>> &imap_modified, 
                          bool sectorModified,
                          Page* sector);
};