/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/options_save.h>
#include <drivers/flash.h>
#include <util/crc16.h>
#include <util/util.h>
#include "memoryState.h"

using namespace std;
using namespace miosix;

struct Header
{
    unsigned char type;        //0x00 if settings, 0x01 if image, 0xff if not type
    unsigned char written;     //0x00 if written,     0xff if not written
    unsigned char invalidated; //0x00 if invalidated, 0xff if not invalidated
    unsigned short crc;
};

void loadOptions(void *options, int optionsSize, unsigned int address)
{
    puts("loadOptions");
    auto& flash=Flash::instance();

    unsigned int size=optionsSize+sizeof(Header);
    assert(size<flash.pageSize());
    auto buffer=make_unique<unsigned char[]>(size);

    if(flash.read(address,buffer.get(),size)==false)
    {
        iprintf("Failed to read address 0x%x\n",address);
        //Read error, abort
    }
    memcpy(options,buffer.get()+sizeof(Header),optionsSize);
    iprintf("Loaded options from address 0x%x\n",address);
}

void saveOptions(MemoryState* state, void *options, int optionsSize)
{
    puts("saveOptions");
    auto& flash=Flash::instance();

    unsigned int size=optionsSize+sizeof(Header);
    assert(size<flash.pageSize());
    auto buffer=make_unique<unsigned char[]>(size);
    auto *header=reinterpret_cast<Header*>(buffer.get());

    unsigned int newAddress = state->getFreeAddress();

    if(newAddress==3840) //sector full, cleaning memory
    {
        puts("All entries full, erasing sector 0");
        flash.eraseSector(0);
        flash.eraseSector(1);
        newAddress=0;
        state->setFirstMemoryAddressFree(0);
        state->setOccupiedMemory(1);
    }

    header->type=0;
    header->written=0;
    header->invalidated=0xff;
    header->crc=crc16(options,optionsSize);
    memcpy(buffer.get()+sizeof(Header),options,optionsSize);
    iprintf("Writing options @ address 0x%x\n",newAddress);

    if(flash.write(newAddress,buffer.get(),size)==false) puts("Failed writing options");

    //necessary read, otherwise consecutive writes aren't finalized
    //TODO: understand on drivers how to remove this
    if(flash.read(newAddress,buffer.get(),size)==false)
    {
        iprintf("Failed to read address 0x%x\n", newAddress);
        return;
    }
    state->setSettingAddress(newAddress);
    state->increaseMemoryAddressFree(flash.pageSize());
}
