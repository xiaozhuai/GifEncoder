//
// Created by xiaozhuai on 2020/12/20.
//

#ifndef GIF_GIFENCODER_H
#define GIF_GIFENCODER_H

#include <string>

class GifEncoder {
public:
    enum PixelFormat {
        PIXEL_FORMAT_UNKNOWN = 0,
        PIXEL_FORMAT_BGR = 1,
        PIXEL_FORMAT_RGB = 2,
        PIXEL_FORMAT_BGRA = 3,
        PIXEL_FORMAT_RGBA = 4,
    };
public:
    GifEncoder() = default;

    /**
     * create gif file
     *
     * @param file file path
     * @param width gif width
     * @param height gif height
     * @param loop loop count, 0 for endless
     * @return
     */
    bool open(const std::string &file, int width, int height, int16_t loop = 0);

    /**
     * add frame
     *
     * @param format pixel format
     * @param frame frame data
     * @param width frame width
     * @param height frame height
     * @param delay delay time 0.01s
     * @param quality 1..30, 1 for best
     * @return
     */
    bool push(PixelFormat format, const uint8_t *frame, int width, int height, int delay, int quality);

    /**
     * close gif file
     *
     * @return
     */
    bool close();

private:
    void *m_gifFileHandler = nullptr;
};


#endif //GIF_GIFENCODER_H
