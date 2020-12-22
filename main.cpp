#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <vector>
#include <chrono>
#include "stb_image.h"
#include "GifEncoder.h"

#define NOW (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

class Bitmap {
public:
    Bitmap() = default;

    Bitmap(uint8_t *_data, GifEncoder::PixelFormat _format, int _width, int _height)
            : data(_data),
              format(_format),
              width(_width),
              height(_height),
              needFree(false) {}

    bool load(const std::string &file, GifEncoder::PixelFormat _format) {
        int w = 0;
        int h = 0;
        int c = 0;
        uint8_t *pixels = nullptr;

        switch (_format) {
            case GifEncoder::PIXEL_FORMAT_BGR:
                pixels = stbi_load(file.c_str(), &w, &h, &c, 3);
                if (pixels == nullptr) return false;

                // RGB2BGR
                for (int i = 0; i < w * h; ++i) {
                    std::swap(pixels[3 * i], pixels[3 * i + 2]);
                }
                break;
            case GifEncoder::PIXEL_FORMAT_RGB:
                pixels = stbi_load(file.c_str(), &w, &h, &c, 3);
                if (pixels == nullptr) return false;
                break;
            case GifEncoder::PIXEL_FORMAT_BGRA:
                pixels = stbi_load(file.c_str(), &w, &h, &c, 4);
                if (pixels == nullptr) return false;

                // RGBA2BGRA
                for (int i = 0; i < w * h; ++i) {
                    std::swap(pixels[4 * i], pixels[4 * i + 2]);
                }
                break;
            case GifEncoder::PIXEL_FORMAT_RGBA:
                pixels = stbi_load(file.c_str(), &w, &h, &c, 4);
                if (pixels == nullptr) return false;
                break;
            default:
                return false;
        }

        needFree = true;
        data = pixels;
        format = _format;
        width = w;
        height = h;
        return true;
    }

    void release() {
        if (needFree) {
            stbi_image_free(data);
            needFree = false;
        }
        data = nullptr;
        format = GifEncoder::PIXEL_FORMAT_UNKNOWN;
        width = 0;
        height = 0;
    }

public:
    uint8_t *data = nullptr;
    GifEncoder::PixelFormat format = GifEncoder::PIXEL_FORMAT_UNKNOWN;
    int width = 0;
    int height = 0;

private:
    bool needFree = false;
};

std::vector<Bitmap> loadImages(const char *fmt, int count) {
    char file[1024] = {0};
    std::vector<Bitmap> frames;
    for (int i = 0; i < count; ++i) {
        snprintf(file, 1024, fmt, i);
        Bitmap frame;
        frame.load(file, GifEncoder::PIXEL_FORMAT_BGR);
        if (!frame.data) {
            fprintf(stderr, "Error load frame %s, size: (%d, %d), format: %d\n", file, frame.width, frame.height,
                    frame.format);
        }
        frames.emplace_back(frame);
    }
    return frames;
}

void releaseImages(std::vector<Bitmap> &frames) {
    for (auto &frame : frames) {
        frame.release();
    }
    frames.clear();
}


void encodeGif(const char *fmt, int count, const char *output,
               int width, int height,
               int quality, int delay,
               bool useGlobalColorMap, int preAllocSize = 0) {
    int64_t lastTime = NOW;

    auto frames = loadImages(fmt, count);

    GifEncoder gifEncoder;

    if (!gifEncoder.open(output, width, height, quality, useGlobalColorMap, 0, preAllocSize)) {
        fprintf(stderr, "Error open gif file\n");
        return;
    }

    for (auto &frame : frames) {
        if (!gifEncoder.push(frame.format, frame.data, frame.width, frame.height, delay)) {
            fprintf(stderr, "Error add a frame\n");
        }
    }

    if (!gifEncoder.close()) {
        fprintf(stderr, "Error close gif file\n");
        return;
    }

    releaseImages(frames);

    printf("Encoded %s, spend %lldμs\n", output, NOW - lastTime);
}

int main() {
    /**
     * For better performance, it's suggested to set preAllocSize. If you can't determine it, set to 0.
     * If use global color map, all frames size must be same, and preAllocSize = width * height * 3 * nFrame
     * If use local color map, preAllocSize = MAX(width * height) * 3
     *
     * quality: 1..30, Lower values (minimum = 1) produce better colors, but slow processing significantly.
     * delay: delay * 0.01s
     */

    encodeGif("../frames/frame_%d.jpg", 4, "out_lcm.gif",
              600, 392,
              10, 20,
              false, 600 * 392 * 3);

    encodeGif("../frames/frame_%d.jpg", 4, "out_gcm.gif",
              600, 392,
              10, 20,
              true, 600 * 392 * 3 * 4);

//    encodeGif("../frames2/frame_%d.jpg", 40, "test_lcm.gif",
//              360, 360,
//              10, 10,
//              false, 360 * 360 * 3);
//
//    encodeGif("../frames2/frame_%d.jpg", 40, "test_gcm.gif",
//              360, 360,
//              10, 10,
//              true, 360 * 360 * 3 * 40);

    return 0;
}
