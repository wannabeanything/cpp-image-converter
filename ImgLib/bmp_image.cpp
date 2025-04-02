#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>

using namespace std;

namespace img_lib {

PACKED_STRUCT_BEGIN BitmapFileHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} PACKED_STRUCT_END

PACKED_STRUCT_BEGIN BitmapInfoHeader {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} PACKED_STRUCT_END

static int GetBMPStride(int w) {
    return 4 * ((w * 3 + 3) / 4);
}

bool SaveBMP(const Path& file, const Image& image) {
    const int width = image.GetWidth();
    const int height = image.GetHeight();
    const int stride = GetBMPStride(width);

    BitmapFileHeader fileHeader = {};
    BitmapInfoHeader infoHeader = {};

    fileHeader.bfType = 0x4D42; 
    fileHeader.bfSize = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + stride * height;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    infoHeader.biSize = sizeof(BitmapInfoHeader);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = stride * height;
    infoHeader.biXPelsPerMeter = 11811;
    infoHeader.biYPelsPerMeter = 11811; 
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0x1000000;

    ofstream ofs(file, ios::binary);
    if (!ofs) {
        return false;
    }


    if (!ofs.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader))) {
        return false;
    }
    if (!ofs.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader))) {
        return false;
    }


    vector<char> rowBuffer(stride, 0);
    for (int y = height - 1; y >= 0; --y) {
        const Color* line = image.GetLine(y); 
        for (int x = 0; x < width; ++x) {
            rowBuffer[x * 3 + 0] = static_cast<char>(line[x].b);
            rowBuffer[x * 3 + 1] = static_cast<char>(line[x].g); 
            rowBuffer[x * 3 + 2] = static_cast<char>(line[x].r);
        }
        if (!ofs.write(rowBuffer.data(), rowBuffer.size())) {
            return false;
        }
    }

    return true;
}

Image LoadBMP(const Path& file) {
    ifstream ifs(file, ios::binary);
    if (!ifs) {
        return {};
    }

    BitmapFileHeader fileHeader = {};
    BitmapInfoHeader infoHeader = {};

    ifs.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    ifs.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));

    // Validate headers
    if (fileHeader.bfType != 0x4D42 || infoHeader.biBitCount != 24 || infoHeader.biCompression != 0) {
        return {};
    }

    const int width = infoHeader.biWidth;
    const int height = abs(infoHeader.biHeight); // Handle negative height
    const int stride = GetBMPStride(width);

    // Create image
    Image image(width, height, Color::Black());

    // Read pixel data
    vector<char> rowBuffer(stride);
    for (int y = 0; y < height; ++y) {
        ifs.read(rowBuffer.data(), rowBuffer.size());

        Color* line = image.GetLine(height - 1 - y); // Reverse row order
        for (int x = 0; x < width; ++x) {
            line[x].b = static_cast<std::byte>(rowBuffer[x * 3 + 0]); // Blue
            line[x].g = static_cast<std::byte>(rowBuffer[x * 3 + 1]); // Green
            line[x].r = static_cast<std::byte>(rowBuffer[x * 3 + 2]); // Red
            line[x].a = std::byte{255}; // Fully opaque alpha channel
        }
    }

    return image;
}

} // namespace img_lib