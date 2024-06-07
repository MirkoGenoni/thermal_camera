#include <stdio.h>
#include <iostream>

#include <drivers/flash.h>

#include "imap_navigation.h"
#include "inode.h"
#include "imap.h"

bool ImapNavigation::setLooked(unsigned int position)
{
    if (position > 255)
        return false;

    unsigned int currentPosition = 7 - position / 32;
    unsigned int localShift = position / 32 * 32;
    if ((imapLooked[currentPosition] >> (position - localShift) & 0x1) == 1)
    {
        return false;
    }
    else
    {
        unsigned int check = 0x1 << (position - localShift);
        imapLooked[currentPosition] = imapLooked[currentPosition] | check;
        return true;
    }
}

void setLocalModified(unsigned int initialAddress, int *count, list<std::unique_ptr<ImapModifiedCache>> &imap_modified)
{
    Flash& flash = Flash::instance();
    auto buffer = make_unique<unsigned char[]>(256);
    auto header = reinterpret_cast<ShortHeader *>(buffer.get());
    auto imap = reinterpret_cast<ImapStruct *>(buffer.get() + sizeof(ShortHeader));
    
    auto bufferModified = make_unique<unsigned char[]>(256);
    auto imapModified = reinterpret_cast<ImapStruct *>(bufferModified.get() + sizeof(ShortHeader));

    if (flash.read(initialAddress, buffer.get(), 256) == false)
    {
        iprintf("Failed to read address 0x%x\n", initialAddress);
    }

    for (int i = 0; i < 68; i ++)
    {   
        if (flash.read(imap->modified_imaps[i] << 8, bufferModified.get(), 256) == false)
        {
            iprintf("Failed to read address 0x%x\n", initialAddress);
        }

        if (imap->modified_imaps[i] == 0xff)
            break;

        unique_ptr<ImapModifiedCache> imapModified = make_unique<ImapModifiedCache>();
        imapModified.get()->id = imapModified->id;
        imapModified.get()->address = imap->modified_imaps[i];
        imap_modified.push_back(std::move(imapModified));
        *count = *count + 1;
    }
}

unsigned int ImapNavigation::nextAddress()
{
    while (initialAddress != 0)
    {
        if (!mainChecked)
        {
            setLocalModified(initialAddress, &count, imap_modified);
            if (setLooked(intialImapId))
            {
                mainChecked = true;
                return initialAddress;
            }
        }

        while (count != 0)
        {
            unsigned int addressReturn = imap_modified.front().get()->address << 8;
            intialImapId = imap_modified.front().get()->id;
            imap_modified.pop_front();
            count--;

            if (setLooked(intialImapId))
            {
                return addressReturn;
            }
        }

        mainChecked = false;
        initialAddress -= 32768;
        intialImapId = calculateId(initialAddress);
        count = 0;
    }

    return 0;
}