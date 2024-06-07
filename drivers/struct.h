#ifndef STRUCT_H
#define STRUCT_H

#include <list>
#include <set>
#include <memory>

struct Header
{
    unsigned char type;        // 0x00 if settings, 0x01 if image, 0xff if not type
    unsigned char written;     // 0x00 if written,     0xff if not written
    unsigned char invalidated; // 0x00 if invalidated, 0xff if not invalidated
    unsigned short crc;
};

struct ShortHeader
{
    unsigned char type; // 0x00 if settings, 0x01 if image, 0xff if not type
};

struct Image
{
    unsigned char type;
    unsigned short id;
    unsigned char position;
};

struct ImapModifiedCache
{
    unsigned short id;
    unsigned int address;
};

struct Page
{
    unsigned short address;
    unsigned char type;
    unsigned short id;
    unsigned char position;
    bool used;
};

struct Sector
{
    Page pages[63];
};

struct InodeModified
{
    unsigned int oldInodeAddress;
    unsigned int inodeAddress;
};

struct ImapModified
{
    unsigned int oldImapAddress;
    unsigned int imapAddress;
};

struct ImagesFound
{
    unsigned short id;

    std::set<unsigned int> inodeSet;
    std::set<unsigned int> imapSet;

    unsigned int framesAddr[6];
};


#endif