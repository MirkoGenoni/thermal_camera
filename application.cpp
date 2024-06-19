/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico and Daniele Cattaneo          *
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

#include <application.h>
#include <mxgui/misc_inst.h>
#include <drivers/misc.h>
#include <drivers/options_save.h>
#include <drivers/memoryState.h>
#include <drivers/image_visualizer.h>
#include <drivers/image_save.h>
#include <images/batt100icon.h>
#include <images/batt75icon.h>
#include <images/batt50icon.h>
#include <images/batt25icon.h>
#include <images/batt0icon.h>
#include <images/miosixlogoicon.h>
#include <images/emissivityicon.h>
#include <images/smallcelsiusicon.h>
#include <images/largecelsiusicon.h>
#include <string.h>

using namespace std;
using namespace miosix;
using namespace mxgui;

/*
 * STM32F205RC
 * SPI1 speed (display) 15MHz
 * I2C1 speed (sensor)  400kHz
 */

//
// class Application
//

Application::Application(Display& display)
    : display(display), ui(*this, display, ButtonState(1^up_btn::value(),on_btn::value())),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),1000)),
      sensor(make_unique<MLX90640>(i2c.get())), usb(make_unique<USBCDC>(Priority()))
{
    memoryState = new MemoryState();
    memoryState->scanMemory(sizeof(ui.options));

    unique_ptr<ImageVisualizer> visualizer = make_unique<ImageVisualizer>(memoryState);
    memoryState->setCurrentImageId(visualizer->findMaxId());
    unsigned int settingsAddress = memoryState->getSettingAddress();
    if(!loadOptions(&ui.options,sizeof(ui.options), settingsAddress)){
        saveOptions(ui.options);
    };
    if(sensor->setRefresh(refreshFromInt(ui.options.frameRate))==false)
        puts("Error setting framerate");
    display.setBrightness(ui.options.brightness * 6);
}

void Application::run()
{
    //High priority for sensor read, prevents I2C reads from starving
    sensorThread = Thread::create(Application::sensorThreadMainTramp, 2048U, Priority(MAIN_PRIORITY+1), static_cast<void*>(this), Thread::JOINABLE);
    //Low priority for processing, prevents display writes from starving
    Thread *processThread = Thread::create(Application::processThreadMainTramp, 2048U, Priority(MAIN_PRIORITY-1), static_cast<void*>(this), Thread::JOINABLE);
    writeThread = Thread::create(Application::writeMemoryMainTramp, 3072U, Priority(MAIN_PRIORITY-1), static_cast<void*>(this), Thread::JOINABLE);

    loadThread = Thread::create(Application::loadimageMainTramp, 3072U, Priority(MAIN_PRIORITY-1), static_cast<void*>(this), Thread::JOINABLE);

    //Drop first frame before starting the render thread
    MLX90640Frame *processedFrame=nullptr;
    processedFrameQueue.get(processedFrame);
    delete processedFrame;

    Thread *renderThread = Thread::create(Application::renderThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);

    Thread *usbInteractiveThread = Thread::create(Application::usbThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);
    Thread *usbOutputThread = Thread::create(Application::usbFrameOutputThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);
    
    ui.lifecycle = UI::Ready;
    while (ui.lifecycle != UI::Quit) {
        //auto t1 = miosix::getTime();
        ui.update();
        //auto t2 = miosix::getTime();
        //iprintf("ui update = %lld\n",t2-t1);
        Thread::sleep(80);
    }

    usb->prepareShutdown();
    
    sensorThread->wakeup(); //Prevents deadlock if acquisition is paused
    sensorThread->join();
    iprintf("sensorThread joined\n");
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevents deadlock
    processThread->join();
    iprintf("processThread joined\n");
    if(processedFrameQueue.isEmpty()) processedFrameQueue.put(nullptr); //Prevents deadlock
    renderThread->join();
    iprintf("renderThread joined\n");
    if(usbOutputQueue.isEmpty()) usbOutputQueue.put(nullptr);
    usbOutputThread->join();
    iprintf("usbOutputThread joined\n");
    usbInteractiveThread->join();
    iprintf("usbInteractiveThread joined\n");

    if(frameWriteBuffer.isFull()) frameWriteBuffer.reset();
    if(frameWriteBuffer.isEmpty()) frameWriteBuffer.put(nullptr);
    ui.writeOut=true;
    writeThread->wakeup();
    writeThread->join();

    if(imageToLoad.isFull()) imageToLoad.reset();
    if(imageToLoad.isEmpty()) imageToLoad.put(nullptr);
    ui.load=true;
    loadThread->wakeup();
    loadThread->join();

    iprintf("writing thread joined\n");
}

ButtonState Application::checkButtons()
{
    // up button is inverted
    return ButtonState(1^up_btn::value(),on_btn::value());
}

BatteryLevel Application::checkBatteryLevel()
{
    prevBatteryVoltage=min(prevBatteryVoltage,getBatteryVoltage());
    return batteryLevel(prevBatteryVoltage);
}

bool Application::checkUSBConnected()
{
    return usb->connected();
}

void Application::setPause(bool pause)
{
    sensorThread->wakeup();
}

void Application::setWriteOut()
{
    writeThread->wakeup();
}

void Application::clearMemory()
{
    memoryState->clearMemory();
    ::saveOptions(memoryState, &ui.options, sizeof(ui.options));
}

void Application::retrieveImages(std::list<std::unique_ptr<ImagesFound>>& found){
    unique_ptr<ImageVisualizer> visualizer = make_unique<ImageVisualizer>(memoryState);
    visualizer->searchImage(found);

    if(found.size()==0) return;

    unsigned int* addr = found.begin()->get()->framesAddr;      
    imageToLoad.put(addr);
    ui.load = true;
    loadThread->wakeup();
    
    MLX90640Frame* frame;
    loadedFrameQueue.get(frame);

    ui.drawLoaded(frame, found.begin()->get()->id);
}


void Application::nextImage(std::list<std::unique_ptr<ImagesFound>>& found){
    if(found.size()==0) return;
    unique_ptr<ImageVisualizer> visualizer = make_unique<ImageVisualizer>(memoryState);
    unsigned short lastId = found.back()->id;
    if(ui.next==true){
        visualizer->nextImage(found);
        if(lastId == found.back()->id){
            ui.next=false;
            ui.skip++;
        }
    }
    
    list<std::unique_ptr<ImagesFound>>::iterator it = found.begin();
    std::advance(it, ui.skip);

    if(ui.skip > found.size()-1){
        ui.skip = found.size()-1;
        return;
    }

    unsigned int* addr = it->get()->framesAddr;
    imageToLoad.put(addr);
    ui.load = true;
    loadThread->wakeup();

    MLX90640Frame* frame;
    loadedFrameQueue.get(frame);

    ui.drawLoaded(frame, it->get()->id);
}


void Application::prevImage(std::list<std::unique_ptr<ImagesFound>>& found){
    if(found.size()==0) return;
    unique_ptr<ImageVisualizer> visualizer = make_unique<ImageVisualizer>(memoryState);
    unsigned short firstId = found.begin()->get()->id;
    if(ui.next==true){
        visualizer->prevImage(found);        
        if(firstId == found.begin()->get()->id){
            ui.next=false;
            ui.skip--;
        }
    }
    
    list<std::unique_ptr<ImagesFound>>::iterator it = found.begin();
    std::advance(it, ui.skip);

    if(ui.skip < 0){
        ui.skip = 0;
        return;
    }

    unsigned int* addr = it->get()->framesAddr;      
    imageToLoad.put(addr);
    ui.load = true;
    loadThread->wakeup();
    
    MLX90640Frame* frame;
    loadedFrameQueue.get(frame);

    ui.drawLoaded(frame, it->get()->id);
}

void Application::deleteImage(std::list<std::unique_ptr<ImagesFound>>& found, unsigned short id){
    unique_ptr<ImageVisualizer> visualizer = make_unique<ImageVisualizer>(memoryState);
    visualizer->deleteImage(found, id);
    iprintf("DELETING: %hu\n", id);
}

void Application::saveOptions(ApplicationOptions& options)
{
    ::saveOptions(memoryState, &options,sizeof(options));
}

void *Application::writeMemoryMainTramp(void* p)
{
    static_cast<Application *>(p)->writeMemoryThreadMain();
    return nullptr;
}

void Application::writeMemoryThreadMain()
{
    while(ui.lifecycle != UI::Quit)
    {
        while(ui.writeOut==false) Thread::wait();
        std::unique_ptr<MLX90640MemoryFrame> memoryFrame;
        {
            MLX90640MemoryFrame *test = nullptr;
            frameWriteBuffer.get(test);
            memoryFrame.reset(test);
        }

        if(!memoryFrame){
            puts("Shutting down, not saving");
            continue;
        }
        // int x=0;
        // char string[32];
        // for(auto data: memoryFrame->memoryImage){
        //     sniprintf(string, sizeof(string), "%d: %u", x, data);
        //     puts(string);
        //     x++;
        // }
        ::saveImage(memoryState, std::move(memoryFrame), 720);
        
        // puts("Written");
        
        ui.writeOut=false;
    }
}

void *Application::loadimageMainTramp(void *p){
    static_cast<Application *>(p)->loadImageThreadMain();
    return nullptr;
};

void Application::loadImageThreadMain(){
    while(ui.lifecycle != UI::Quit)
    {
        while(ui.load==false) Thread::wait();

        MLX90640Frame* memoryFrame = new MLX90640Frame;
        MLX90640Frame* frame = new MLX90640Frame;

        unsigned int* addresses;
        imageToLoad.get(addresses);

        ::loadImage(memoryFrame, addresses);
        sensor->decompressFrame(frame, memoryFrame);
        //TODO: handle with smart pointer
        delete(memoryFrame);
        loadedFrameQueue.put(frame);
        imageToLoad.reset();
        ui.load = false;
    }
};

void *Application::sensorThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->sensorThreadMain();
    return nullptr;
}

void Application::sensorThreadMain()
{
    auto previousRefreshRate=sensor->getRefresh();
    while(ui.lifecycle!=UI::Quit)
    {
        auto *rawFrame=new MLX90640RawFrame;
        bool success;
        do {
            auto currentRefreshRate=refreshFromInt(ui.options.frameRate);
            if(previousRefreshRate!=currentRefreshRate)
            {
                if(sensor->setRefresh(currentRefreshRate))
                    previousRefreshRate=currentRefreshRate;
                else puts("Error setting framerate");
            }
            success=sensor->readFrame(rawFrame);
            if(success==false) puts("Error reading frame");
        } while(success==false);
        {
            FastInterruptDisableLock dLock;
            success=rawFrameQueue.IRQput(rawFrame); //Nonblocking put
        }
        if(success==false)
        {
            puts("Dropped frame");
            delete rawFrame; //Drop frame without leaking memory
        }
        while (ui.paused && ui.lifecycle!=UI::Quit) Thread::wait();
    }
    iprintf("sensorThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::processThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->processThreadMain();
    return nullptr;
}

void Application::processThreadMain()
{
    while(ui.lifecycle != UI::Quit)
    {
        MLX90640RawFrame *rawFrame=nullptr;
        rawFrameQueue.get(rawFrame);
        if(rawFrame==nullptr) continue; //Happens on shutdown
        //auto t1=getTime();
        auto *processedFrame=new MLX90640Frame;
        sensor->processFrame(rawFrame,processedFrame,ui.options.emissivity);
        processedFrameQueue.put(processedFrame);
        usbOutputQueue.put(rawFrame);
        if(ui.writeOut==false){
            //Memory write thread is sleeping, freeing memory before allocating new one
            if(frameWriteBuffer.isFull()){ 
                std::unique_ptr<MLX90640MemoryFrame> rawFrame;
                {
                    MLX90640MemoryFrame *pointer=nullptr;
                    frameWriteBuffer.get(pointer);
                    rawFrame.reset(pointer);
                }
                rawFrame.reset();
                frameWriteBuffer.reset();
            }

            auto* memoryFrame = new MLX90640MemoryFrame;
            
            // auto t3=getTime();
            sensor->reduceFrame(processedFrame, memoryFrame);
            // auto t4=getTime();
            // iprintf("reducing time: %lld\n", t4-t3);
            frameWriteBuffer.put(memoryFrame);
        }
        //auto t2=getTime();
        //iprintf("process = %lld\n",t2-t1);
    }
    iprintf("processThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::renderThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->renderThreadMain();
    return nullptr;
}

void Application::renderThreadMain()
{
    while(ui.lifecycle != UI::Quit)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        ui.updateFrame(processedFrame);
    }
    iprintf("renderThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::usbThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->usbThreadMain();
    return nullptr;
}

char *hexDump(const uint8_t *bytes, int size, char *output)
{
    static const char *hex = "0123456789ABCDEF";
    for (int i=0; i<size; i++)
    {
        *output++ = hex[bytes[i] & 0xF];
        *output++ = hex[bytes[i] >> 4];
    }
    return output;
}

void Application::usbThreadMain()
{
    while (ui.lifecycle != UI::Quit) {
        char buf[80];
        bool success = usb->readLine(buf, 80);
        if (!success)
            continue;
        
        if (strcmp(buf, "get_eeprom") == 0) {
            const MLX90640EEPROM& eeprom = sensor->getEEPROM();
            const int hexSize = MLX90640EEPROM::eepromSize*4+2;
            char *hex = new char[hexSize];
            char *p = hexDump(reinterpret_cast<const uint8_t *>(eeprom.eeprom), MLX90640EEPROM::eepromSize*2, hex);
            *p++ = '\r'; *p++ = '\n';
            usb->write(reinterpret_cast<uint8_t *>(hex), hexSize, usbWriteTimeout);
            delete hex;
        } else if (strcmp(buf, "start_stream") == 0) {
            usbDumpRawFrames = true;
        } else if (strcmp(buf, "stop_stream") == 0) {
            usbDumpRawFrames = false;
        } else {
            usb->print("Unrecognized command\r\n", usbWriteTimeout);
        }
    }
    iprintf("usbInteractiveThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::usbFrameOutputThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->usbFrameOutputThreadMain();
    return nullptr;
}

void Application::usbFrameOutputThreadMain()
{
    const int hexSize = (2+834*sizeof(uint16_t)*2+2)*2;
    char *hex = new char[hexSize];

    while(ui.lifecycle != UI::Quit)
    {
        std::unique_ptr<MLX90640RawFrame> rawFrame;
        {
            MLX90640RawFrame *pointer=nullptr;
            usbOutputQueue.get(pointer);
            rawFrame.reset(pointer);
        }
        if (!rawFrame) continue;
        if (!usb->connected())
        {
            usbDumpRawFrames = false;
        } else if (usbDumpRawFrames && !ui.paused) {
            char *p = hex;
            *p++ = '1'; *p++ = '=';
            p = hexDump(reinterpret_cast<const uint8_t *>(rawFrame->subframe[0]), 834*2, p);
            *p++ = '\r'; *p++ = '\n';
            *p++ = '2'; *p++ = '=';
            p = hexDump(reinterpret_cast<const uint8_t *>(rawFrame->subframe[1]), 834*2, p);
            *p++ = '\r'; *p++ = '\n';
            rawFrame.reset(nullptr);
            usb->write(reinterpret_cast<uint8_t *>(hex), hexSize, usbWriteTimeout);
        }
    }
    delete hex;

    iprintf("usbOutputThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}
