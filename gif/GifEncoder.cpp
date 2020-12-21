//
// Created by xiaozhuai on 2020/12/20.
//

#include <string>
#include <vector>
#include <cstdlib>
#include<cstring>
#include "GifEncoder.h"
#include "giflib/gif_lib.h"
#include "algorithm/NeuQuant.h"

#define m_gifFile ((GifFileType *) m_gifFileHandler)
#define GifAddExtensionBlockFor(a, func, len, data) GifAddExtensionBlock(       \
        &((a)->ExtensionBlockCount),                                            \
        &((a)->ExtensionBlocks),                                                \
        func,                                                                   \
        len,                                                                    \
        data                                                                    \
    )

bool GifEncoder::open(const std::string &file, int width, int height, int16_t loop) {
    if (m_gifFile != nullptr) {
        return false;
    }

    int error;
    m_gifFileHandler = EGifOpenFileName(file.c_str(), false, &error);
    if (!m_gifFile) {
        return false;
    }

    m_gifFile->SWidth = width;
    m_gifFile->SHeight = height;
    m_gifFile->SColorResolution = 8;
    m_gifFile->SBackGroundColor = 0;
    m_gifFile->SColorMap = nullptr;

    uint8_t appExt[11] = {
            'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
            '2', '.', '0'
    };
    uint8_t appExtSubBlock[3] = {
            0x01,       // hex 0x01
            0x00, 0x00  // little-endian short. The number of times the loop should be executed.
    };
    memcpy(appExtSubBlock + 1, &loop, sizeof(loop));

    GifAddExtensionBlockFor(m_gifFile, APPLICATION_EXT_FUNC_CODE, sizeof(appExt), appExt);
    GifAddExtensionBlockFor(m_gifFile, CONTINUE_EXT_FUNC_CODE, sizeof(appExtSubBlock), appExtSubBlock);

    return true;
}

bool GifEncoder::push(PixelFormat format, const uint8_t *frame, int width, int height, int delay, int quality) {
    if (m_gifFile == nullptr) {
        return false;
    }

    if (frame == nullptr) {
        return false;
    }

    const uint8_t *pixels = nullptr;
    uint8_t *frameCopy = nullptr;
    switch (format) {
        case PIXEL_FORMAT_BGR:
            pixels = frame;
            break;
        case PIXEL_FORMAT_RGB:
            frameCopy = (uint8_t *) malloc(width * height * 3);
            for (int i = 0; i < width * height; ++i) {
                frameCopy[i * 3] = frame[i * 3 + 2];
                frameCopy[i * 3 + 1] = frame[i * 3 + 1];
                frameCopy[i * 3 + 2] = frame[i * 3];
            }
            pixels = frameCopy;
            break;
        case PIXEL_FORMAT_BGRA:
            frameCopy = (uint8_t *) malloc(width * height * 3);
            for (int i = 0; i < width * height; ++i) {
                frameCopy[i * 3] = frame[i * 4];
                frameCopy[i * 3 + 1] = frame[i * 4 + 1];
                frameCopy[i * 3 + 2] = frame[i * 4 + 2];
            }
            pixels = frameCopy;
            break;
        case PIXEL_FORMAT_RGBA:
            frameCopy = (uint8_t *) malloc(width * height * 3);
            for (int i = 0; i < width * height; ++i) {
                frameCopy[i * 3] = frame[i * 4 + 2];
                frameCopy[i * 3 + 1] = frame[i * 4 + 1];
                frameCopy[i * 3 + 2] = frame[i * 4];
            }
            pixels = frameCopy;
            break;
        default:
            return false;
    }

    std::vector<uint8_t> colorMap(256 * 3);
    auto *indices = (uint8_t *) malloc(width * height);

    initnet(pixels, width * height * 3, quality);
    learn();
    unbiasnet();
    getcolourmap(colorMap.data());

    inxbuild();

    for (int i = 0; i < width * height; ++i) {
        indices[i] = inxsearch(
                pixels[i * 3],
                pixels[i * 3 + 1],
                pixels[i * 3 + 2]
        );
    }

    SavedImage gifImage;
    gifImage.ImageDesc.Left = 0;
    gifImage.ImageDesc.Top = 0;
    gifImage.ImageDesc.Width = width;
    gifImage.ImageDesc.Height = height;
    gifImage.ImageDesc.Interlace = false;
    gifImage.ImageDesc.ColorMap = GifMakeMapObject(256, (const GifColorType *) colorMap.data());
    gifImage.RasterBits = (GifByteType *) indices;
    gifImage.ExtensionBlockCount = 0;
    gifImage.ExtensionBlocks = nullptr;

    GraphicsControlBlock gcb;
    gcb.DisposalMode = DISPOSE_DO_NOT;
    gcb.UserInputFlag = false;
    gcb.DelayTime = delay;
    gcb.TransparentColor = NO_TRANSPARENT_COLOR;
    uint8_t gcbBytes[4];
    EGifGCBToExtension(&gcb, gcbBytes);
    GifAddExtensionBlockFor(&gifImage, GRAPHICS_EXT_FUNC_CODE, sizeof(gcbBytes), gcbBytes);

    GifMakeSavedImage(m_gifFile, &gifImage);

    if (frameCopy != nullptr) {
        free(frameCopy);
    }

    return true;
}

bool GifEncoder::close() {
    if (m_gifFile == nullptr) {
        return false;
    }

    int error;
    if (EGifSpew(m_gifFile) == GIF_ERROR) {
        EGifCloseFile(m_gifFile, &error);
        m_gifFileHandler = nullptr;
        return false;
    }

    GifFreeExtensions(&m_gifFile->ExtensionBlockCount, &m_gifFile->ExtensionBlocks);
    GifFreeSavedImages(m_gifFile);

    m_gifFileHandler = nullptr;
    return true;
}
