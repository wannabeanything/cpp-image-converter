#include "jpeg_image.h"

#include <array>
#include <cstdio>
#include <setjmp.h>

#include <jpeglib.h>

using namespace std;

namespace img_lib {

// структура из примера LibJPEG
struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr* my_error_ptr;

// функция из примера LibJPEG
METHODDEF(void)
my_error_exit(j_common_ptr cinfo) {
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

bool SaveJPEG(const Path& file, const Image& image) {
    jpeg_compress_struct cinfo;
    my_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        return false;
    }

    jpeg_create_compress(&cinfo);

    FILE* outfile = nullptr;

#ifdef _MSC_VER
    outfile = _wfopen(file.wstring().c_str(), L"wb");
#else
    outfile = fopen(file.string().c_str(), "wb");
#endif

    if (!outfile) {
        jpeg_destroy_compress(&cinfo);
        return false;
    }

    jpeg_stdio_dest(&cinfo, outfile);

    int width = image.GetWidth();
    int height = image.GetHeight();

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = width * 3;

    while (cinfo.next_scanline < cinfo.image_height) {
        const Color* line = image.GetLine(cinfo.next_scanline);
        JSAMPLE* jsample_row = new JSAMPLE[row_stride];
        for (int x = 0; x < width; ++x) {
            jsample_row[x * 3 + 0] = static_cast<JSAMPLE>(line[x].r);
            jsample_row[x * 3 + 1] = static_cast<JSAMPLE>(line[x].g);
            jsample_row[x * 3 + 2] = static_cast<JSAMPLE>(line[x].b);
        }

        row_pointer[0] = jsample_row;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);

        delete[] jsample_row;
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);

    return true;
}

Image LoadJPEG(const Path& file) {
    FILE* infile;
    jpeg_decompress_struct cinfo;
    my_error_mgr jerr;

#ifdef _MSC_VER
    if ((infile = _wfopen(file.wstring().c_str(), L"rb")) == nullptr) {
#else
    if ((infile = fopen(file.string().c_str(), "rb")) == nullptr) {
#endif
        return {};
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return {};
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    (void)jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB;
    cinfo.output_components = 3;

    (void)jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int row_stride = width * cinfo.output_components;

    Image image(width, height, Color::Black());
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for (int x = 0; x < width; ++x) {
            JSAMPLE* pixel = buffer[0] + x * 3;
            image.GetPixel(x, cinfo.output_scanline - 1) = Color{
                static_cast<std::byte>(pixel[0]),
                static_cast<std::byte>(pixel[1]),
                static_cast<std::byte>(pixel[2]),
                std::byte{255}
            };
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return image;
}

} // namespace img_lib