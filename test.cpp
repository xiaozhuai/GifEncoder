//
// Created by xiaozhuai on 2020/12/25.
//

#include "GifEncoder.h"

int main() {
    // Suppose that you have three frame to be encoded
    const int w = 40;
    const int h = 40;
    uint32_t frame0[w * h];
    uint32_t frame1[w * h];
    uint32_t frame2[w * h];
    for (int i = 0; i < w * h; ++i) {
        frame0[i] = 0xFF0000FF; // red
        frame1[i] = 0xFF00FF00; // green
        frame2[i] = 0xFFFF0000; // blue
    }

    int quality = 10;
    bool useGlobalColorMap = true;
    int loop = 0;
    int preAllocSize = useGlobalColorMap ? w * h * 3 * 3 : w * h * 3;

    GifEncoder gifEncoder;

    if (!gifEncoder.open("test.gif", w, h, quality, useGlobalColorMap, loop, preAllocSize)) {
        fprintf(stderr, "Error open gif file\n");
        return 1;
    }

    gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, (uint8_t *) frame0, w, h, 20);
    gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, (uint8_t *) frame1, w, h, 20);
    gifEncoder.push(GifEncoder::PIXEL_FORMAT_RGBA, (uint8_t *) frame2, w, h, 20);

    if (!gifEncoder.close()) {
        fprintf(stderr, "Error close gif file\n");
        return 1;
    }
}
