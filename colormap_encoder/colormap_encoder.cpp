#include <iostream>
#include <iomanip>

using namespace std;

unsigned char colormap[3*256] =
{
#include "thermalcolormap"
};

unsigned short to565(unsigned short r, unsigned short g, unsigned short b) {
    return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);
}

int main()
{
    cout << "//Autogenerated by colormap_encoder.cpp" << endl;
    cout << "const unsigned short colormap[256] = " << endl;
    cout << "{" << endl << "    ";
    for(int i = 0; i < 256; i++)
    {
        unsigned short r = colormap[3*i], g = colormap[3*i+1], b = colormap[3*i+2];
        //Convert RGB888 -> RGB565
        unsigned short color = to565(r,g,b);
        cout << "0x" << hex << setfill('0') << setw(4) << color << ",";
        if(i % 8 == 7)
        {
            cout << endl;
            if(i != 255) cout << "    ";
        }
    }
    cout << "};" << endl;
}
