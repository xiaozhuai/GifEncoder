#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <vector>
#include "stb_image.h"
#include "gif/GifEncoder.h"

class Bitmap {
public:
    Bitmap() = default;

    Bitmap(uint8_t *_data, GifEncoder::PixelFormat _format, int _width, int _height)
            : data(_data),
              format(_format),
              width(_width),
              height(_height) {}

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

        data = pixels;
        format = _format;
        width = w;
        height = h;
        return true;
    }

    void release() {
        stbi_image_free(data);
        format = GifEncoder::PIXEL_FORMAT_UNKNOWN;
        width = 0;
        height = 0;
    }

public:
    uint8_t *data = nullptr;
    GifEncoder::PixelFormat format = GifEncoder::PIXEL_FORMAT_UNKNOWN;
    int width = 0;
    int height = 0;
};

std::vector<Bitmap> loadImages(const char *fmt, int count) {
    char file[1024] = {0};
    std::vector<Bitmap> frames;
    for (int i = 0; i < count; ++i) {
        snprintf(file, 1024, fmt, i);
        Bitmap frame;
        frame.load(file, GifEncoder::PIXEL_FORMAT_BGR);
        frames.emplace_back(frame);
        printf("Loaded frame %s, size: (%d, %d), format: %d\n", file, frame.width, frame.height, frame.format);
    }
    return frames;
}

void releaseImages(std::vector<Bitmap> &frames) {
    for (auto &frame : frames) {
        frame.release();
    }
    frames.clear();
}

int main() {
    auto frames = loadImages("../frames/frame_%d.jpg", 4);

    int delay = 20;  // 20 * 0.01s
    int quality = 10; // 1..30, Lower values (minimum = 1) produce better colors, but slow processing significantly.

    GifEncoder gifEncoder;

    if (!gifEncoder.open("out.gif", 600, 392)) {
        fprintf(stderr, "Error open gif file\n");
        return 1;
    }

    for (int i = 0; i < frames.size(); ++i) {
        auto &frame = frames[i];
        if (gifEncoder.push(frame.format, frame.data, frame.width, frame.height, delay, quality)) {
            printf("Encoded frame %d/%lu\n", i + 1, frames.size());
        } else {
            fprintf(stderr, "Error add a frame\n");
        }
    }

    if (!gifEncoder.close()) {
        fprintf(stderr, "Error close gif file\n");
        return 1;
    }

    releaseImages(frames);

    return 0;
}
