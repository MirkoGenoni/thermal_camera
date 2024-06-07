#include <list>
#include <memory>
#include "struct.h"

using namespace std;

class ImapNavigation
{
public:
    ImapNavigation(unsigned int initialAddress)
    {
        this->initialAddress = initialAddress;
        this->intialImapId = calculateId(initialAddress);

        for (int i = 0; i < 8; i++)
        {
            imapLooked[i] = 0;
        }
    }

    unsigned short calculateId(unsigned int address)
    {
        return ((address - 256) / (256 * 64)) / 2;
    }

    bool setLooked(unsigned int position);

    unsigned int nextAddress();

    unsigned int *getImapLooked()
    {
        return imapLooked;
    }

private:
    unsigned int imapLooked[8];
    unsigned int initialAddress;
    unsigned short intialImapId;

    bool mainChecked = false;
    int count = 0;

    std::list<unique_ptr<ImapModifiedCache>> imap_modified;
};