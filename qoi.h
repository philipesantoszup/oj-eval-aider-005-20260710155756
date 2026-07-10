#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"
#include <vector>
#include <cmath>

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;
    uint64_t px_num = (uint64_t)width * height;

    struct Pixel { uint8_t r, g, b, a; };
    std::vector<Pixel> pixels;
    pixels.reserve(px_num);

    for (uint64_t i = 0; i < px_num; ++i) {
        uint8_t r = QoiReadU8();
        uint8_t g = QoiReadU8();
        uint8_t b = QoiReadU8();
        uint8_t a = (channels == 4) ? QoiReadU8() : 255;
        pixels.push_back({r, g, b, a});
    }

    for (uint64_t i = 0; i < px_num; ++i) {
        uint8_t r = pixels[i].r;
        uint8_t g = pixels[i].g;
        uint8_t b = pixels[i].b;
        uint8_t a = pixels[i].a;

        if (i > 0 && r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            int run = 0;
            while (i + 1 < px_num && run < 127 && 
                   pixels[i+1].r == r && pixels[i+1].g == g && 
                   pixels[i+1].b == b && pixels[i+1].a == a) {
                run++;
                i++;
            }
            QoiWriteU8(QOI_OP_RUN_TAG);
            QoiWriteU8(run);
            
            uint8_t hash = QoiColorHash(r, g, b, a);
            history[hash][0] = r;
            history[hash][1] = g;
            history[hash][2] = b;
            history[hash][3] = a;
            pre_r = r; pre_g = g; pre_b = b; pre_a = a;
            continue;
        }

        int index = -1;
        for (int h = 0; h < 64; ++h) {
            if (history[h][0] == r && history[h][1] == g && history[h][2] == b && history[h][3] == a) {
                index = h;
                break;
            }
        }

        bool can_diff = (std::abs((int)r - (int)pre_r) <= 64 && 
                        std::abs((int)g - (int)pre_g) <= 64 && 
                        std::abs((int)b - (int)pre_b) <= 64 && 
                        std::abs((int)a - (int)pre_a) <= 64);
        
        uint8_t luma = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
        bool can_luma = (r == luma && g == luma && b == luma);

        if (index != -1) {
            QoiWriteU8(QOI_OP_INDEX_TAG);
            QoiWriteU8(index);
        } else if (can_diff) {
            QoiWriteU8(QOI_OP_DIFF_TAG);
            QoiWriteU8(r - pre_r);
            QoiWriteU8(g - pre_g);
            QoiWriteU8(b - pre_b);
            QoiWriteU8(a - pre_a);
        } else if (can_luma) {
            QoiWriteU8(QOI_OP_LUMA_TAG);
            QoiWriteU8(luma);
            QoiWriteU8(a - pre_a);
        } else {
            if (channels == 3) {
                QoiWriteU8(QOI_OP_RGB_TAG);
                QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            } else {
                QoiWriteU8(QOI_OP_RGBA_TAG);
                QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b); QoiWriteU8(a);
            }
        }

        uint8_t hash = QoiColorHash(r, g, b, a);
        history[hash][0] = r;
        history[hash][1] = g;
        history[hash][2] = b;
        history[hash][3] = a;

        pre_r = r; pre_g = g; pre_b = b; pre_a = a;
    }

    for (int i = 0; i < 8; ++i) QoiWriteU8(QOI_PADDING[i]);
    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    if (QoiReadU8() != 'q') return false;
    if (QoiReadU8() != 'o') return false;
    if (QoiReadU8() != 'i') return false;
    if (QoiReadU8() != 'f') return false;

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));
    uint8_t r = 0, g = 0, b = 0, a = 255;

    uint64_t px_num = (uint64_t)width * height;
    for (uint64_t i = 0; i < px_num; ++i) {
        uint8_t tag = QoiReadU8();
        if (tag == QOI_OP_RUN_TAG) {
            uint8_t run_val = QoiReadU8();
            int count = (run_val & 0x7f) + 1;
            for (int j = 0; j < count; ++j) {
                QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
                if (channels == 4) QoiWriteU8(a);
                
                uint8_t hash = QoiColorHash(r, g, b, a);
                history[hash][0] = r;
                history[hash][1] = g;
                history[hash][2] = b;
                history[hash][3] = a;
            }
            i += (count - 1);
        } else if (tag == QOI_OP_INDEX_TAG) {
            uint8_t index = QoiReadU8();
            r = history[index][0]; g = history[index][1]; b = history[index][2]; a = history[index][3];
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
        } else if (tag == QOI_OP_DIFF_TAG) {
            r += QoiReadU8(); g += QoiReadU8(); b += QoiReadU8(); a += QoiReadU8();
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
        } else if (tag == QOI_OP_LUMA_TAG) {
            uint8_t lum = QoiReadU8();
            r = g = b = lum;
            a += QoiReadU8();
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
        } else if (tag == QOI_OP_RGB_TAG) {
            r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8(); a = 255;
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
        } else if (tag == QOI_OP_RGBA_TAG) {
            r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8(); a = QoiReadU8();
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
        } else {
            return false;
        }

        if (tag != QOI_OP_RUN_TAG) {
            uint8_t hash = QoiColorHash(r, g, b, a);
            history[hash][0] = r;
            history[hash][1] = g;
            history[hash][2] = b;
            history[hash][3] = a;
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) return false;
    }
    return true;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
