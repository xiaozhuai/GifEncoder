//
// Created by xiaozhuai on 2020/12/20.
// Expanded by SkrFractals on 2025/01/31
//

#ifndef GIF_GIFENCODER_H
#define GIF_GIFENCODER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "giflib/gif_lib.h"
#include "algorithm/NeuQuant.h"

#pragma region ENUMS
/** Allowed pixel formats, BGR_NATIVE is fastest and will not allocate new mmemory, you should not unlock/free that frame data until the encoding task is finished*/
public enum GifEncoderPixelFormat : uint8_t {
    PIXEL_FORMAT_UNKNOWN = 0,
    PIXEL_FORMAT_BGR_NATIVE = 1,
    PIXEL_FORMAT_BGR = 2,
    PIXEL_FORMAT_RGB = 3,
    PIXEL_FORMAT_BGRA = 4,
    PIXEL_FORMAT_RGBA = 5,
};

/** Return of tryWrite call, informing you how it went */
public enum GifEncoderTryWriteResult : uint8_t {
    Failed = 0,				// Error has occured, and the file was aborted.
    Waiting = 1,			// Next frame was not written because it was either not supplied yet, or it's task is still running.
    FinishedFrame = 2,		// Next frame was successfully written into the stream
    FinishedAnimation = 3	// The stream was sucessfully finished
};
#pragma endregion

#pragma region TASK_STRUCT
/** SkrFractals expansion: A structture holding the data for and performing the parallel neuquant */
public struct NeuQuantTask {

    // Paralellism variables:
    ColorMapObject* colorMap;   // learned color map from NeuQuant
    GifByteType* rasterBits;    // rasterBits to write into the file
    int frameIndex;             // index of the frame in order to write into the file (needed if you push them out of order)
    int m_delay;                // delay of this frame
    bool finished;              // is NeuQuant's learn finished?
    bool failed;                // has something failed?
    int m_allocSize;            // how many bytes of thepicture are allocated
    int m_frameWidth;           // width of the image
    int m_frameHeight;          // height of the image
    int transIndex;             // index of the transparent color
    int transR, transG, transB; // transparent color
    //int factor;
    NeuQuant
        neuquant = NeuQuant();  // unstaticed instance of neuquant to allow it to run in parallel
    NeuQuantTask* nextTask;     // next pushed task (can be out of order frame)
    std::thread* thisTask;      // if the task makes it's own thread, it writes it here, and also to the reference supplied in the push
    uint8_t* thepicture;        // BGR bytes of the picture
    int lengthcount;            // = Height * Width * 3
    int samplepixels;           // = lengthcount / (3 * quality)
    int factor;                 // balanced factor of samplepixels to evenly split the learn loop
    void* cancel;               // cancellation pointer
    bool cancelType;            // false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*

    NeuQuantTask(const int width, const int height);

    void setTransparent(const int r, const int g, const int b);

    /** changes the size, return true if changed */
    bool setSize(const int width, const int height);

    /** attmepts to allocate memory for thepicture */
    bool alloc(const int needSize);

    /** copies pixels into thepicture in BGR format */
    bool convertToBGR(GifEncoderPixelFormat format, uint8_t* dst, const uint8_t* src) const;

    /** runs the NeuQuant's learn to get the color palette */
    bool getColorMap(const int quality);

    /** gets raster bits according to the colormap */
    bool getRasterBits(uint8_t* pixels, NeuQuantTask* task = nullptr);

private:
    inline void getRasterPix(NeuQuant& nq, const uint8_t* pic, const int pix) {
        const auto i3 = pix * 3;
        rasterBits[pix] = nq.inxsearch(pic[i3], pic[i3 + 1], pic[i3 + 2]);
    }
};
#pragma endregion

class GifEncoder {

    // ---------------------
    // Xiaozhuai's Original:

public:

#pragma region ORIGINAL_PUBLIC

    GifEncoder() = default;

    /**
      * create gif file - original synchronous version for compatibily, allowing cancellation
      *
      * @param file file path
      * @param width gif width
      * @param height gif height
      * @param quality 1..30, 1 is best
      * @param useGlobalColorMap if true each frame wil lhave it's own color map, and gets encoded during a push, if false, all framnes get encoded at close
      * @param loop loop count, 0 is endless
      * @param preAllocSize For better performance, it's suggested to set preAllocSize. If you can't determine it, set to 0.
      *        If use global color map (you won't, it's disabled in this threading version), all frames size must be same, and preAllocSize = width * height * 3 * nFrame
      *        If use local color map, preAllocSize = MAX(width * height) * 3
      * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
      * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable
      * @return success
      */
    bool open(const std::string& file, const int width, const int height,
              const int quality = 1, const bool useGlobalColorMap = false, const int16_t loop = 0,
              const int preAllocSize = 0, bool cancelType = false, void* cancel = nullptr);

    /**
     * add frame - original compatibility - allowing out of order pushing, allowing cancellation
     *
     * @param format pixel format
     * @param frame frame data
     * @param width frame width
     * @param height frame height
     * @param delay delay time 0.01s
     * @param task - starts its own push task and puts it into the reference
     * @param frameIndex -1 for original-like in order pushing, otherwise supply the frame index (and don't mix or mess that up or else you get a failed return when closing)
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable, or if you want to use the one supplied from open()
     * @return
     */
    bool push(const GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height,
              const int delay = 5, const int frameIndex = -1, bool cancelType = false, void* cancel = nullptr);

    /**
     * add frame - original compatibility - only uncopied BGR, allowing out of order pushing, allowing cancellation
     *
     * @param frame frame data
     * @param width frame width
     * @param height frame height
     * @param delay delay time 0.01s
     * @param task - starts its own push task and puts it into the reference
     * @param frameIndex -1 for original-like in order pushing, otherwise supply the frame index (and don't mix or mess that up or else you get a failed return when closing)
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable, or if you want to use the one supplied from open()
     * @return
     */
    inline bool push(uint8_t* frame, const int width, const int height,
                     const int delay = 5, const int frameIndex = -1, bool cancelType = false, void* cancel = nullptr) {
        return push(GifEncoderPixelFormat::PIXEL_FORMAT_BGR_NATIVE, frame, width, height, delay, frameIndex, cancelType, cancel);
    }

    /**
     * close gif file, but if in parallel it just marks the pushes as final, and you need to complete the file by calling tryWrite until a finished or failed result
     *
     * @multiFrameGlobalMap if left true, it will learn the global colormap from all the frames, if false, it will only learn it from the first frame, assuming the other frames have similar colors
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable, or if you want to use the one supplied from open()
     * @return
     */
    bool close(bool multiFrameGlobalMap = true, bool cancelType = false, void* cancel = nullptr);

#pragma endregion

private:

#pragma region ORIGINAL_PRIVATES_AND_VARIABLES
    bool setupTask(NeuQuantTask* pushPtr, const GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height, const bool first, const int delay, bool cancelType = false, void* cancel = nullptr);

    /** Encodes the frame */
    void encodeFrame(NeuQuantTask& task, const int delay, const bool globalColorMap);

    //inline bool isFirstFrame() const {return m_frameCount == 0;} // unused

    inline void reset();

    // Original variables:
    void* m_gifFileHandler = nullptr;
    int m_quality = 10;
    bool m_useGlobalColorMap = false;
    std::vector<int> m_allFrameDelays{};
    int m_frameCount = 0;
#pragma endregion

    // Xiaozhuai's Original end
    // ------------------------

    // ----------------------
    // SkrFractals expansion:

public:

#pragma region EXPANSION_PUBLIC

    /**
     * create gif file
     *
     * @param file file path
     * @param width gif width
     * @param height gif height
     * @param quality 1..30, 1 is best
     * @param useGlobalColorMap will only laren a global colormap from the first frame (a global colormap from all frames is not available with parallel mode)
     * @param loop loop count, 0 is endless
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable
     * @return success
     */
    bool open_parallel(const std::string& file, const int width, const int height,
                       const int quality = 1, const bool useGlobalColorMap = false, int16_t loop = 0, bool cancelType = false, void* cancel = nullptr);

    /** set transparent color, if any value is negative, it resets transparency */
    inline void setTransparent(const int b, const int g, const int r) {
        if (r < 0 || g < 0 || b < 0) {
            transparent = false;
            return;
        }
        transB = b;
        transG = g;
        transR = r;
        transparent = true;
    }

    /** resets transparency (makes opaque again) */
    inline void resetTransparent() {
        transB = transG = transR = -1;
        transparent = false;
    }

    /**
     * add frame - allowing out of order, cancellable (or cancellable from open's cancel)
     *
     * @param format pixel format
     * @param frame frame data
     * @param width frame width
     * @param height frame height
     * @param delay delay time 0.01s
     * @param frameIndex -1 for original-like in order pushing, otherwise supply the frame index (and don't mix or mess that up or else you get a failed return when closing)
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable, or if you want to use the one supplied from open()
     * @return
     */
    bool push_parallel(const GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height,
                       const int delay = 5, const int frameIndex = -1, bool cancelType = false, void* cancel = nullptr);

    /**
     * add frame - only uncopied BGR, allowing out of order, cancellable (or cancellable from open's cancel)
     *
     * @param frame frame data
     * @param width frame width
     * @param height frame height
     * @param delay delay time 0.01s
     * @param frameIndex -1 for original-like in order pushing, otherwise supply the frame index (and don't mix or mess that up or else you get a failed return when closing)
     * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
     * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable, or if you want to use the one supplied from open()
     * @return
     */
    inline bool push_parallel(uint8_t* frame, const int width, const int height, const int delay = 5, const int frameIndex = -1, bool cancelType = false, void* cancel = nullptr) {
        return push_parallel(GifEncoderPixelFormat::PIXEL_FORMAT_BGR_NATIVE, frame, width, height, delay, frameIndex, cancelType, cancel);
    }

    /**
     * try to write next frame into the file - this is only for parallel mode
     *
     * @param parallel - if you are calling this in parallel, set this to true so it locks inself to preserve data integrity!
     * @return Failed = somethign went wrong, stop trying the file has been closed. Waiting - try again later, or some frames are missing. FinishedFrame - one frame was written. FinishedAnimation - file is completed.
     */
    GifEncoderTryWriteResult tryWrite(bool parallel = false);

    /** has the file been fully written ? */
    inline bool isFinishedAnimation() const { return finishedAnimation; }

    /** how many frames have been written into the file already? */
    inline int getFinishedFrame() const { return finishedFrame; }
#pragma endregion

private:

#pragma region EXPANSION_PRIVATES_AND_VARIABLES
    GifEncoderTryWriteResult tryWriteInternal();

    /** Attempts to open a file, returns true if failed (unlike most other function returns here) */
    bool initOpen(const std::string& file, const int width, const int height);

    /** Attempts to start writing the file */
    bool endOpen(NeuQuantTask& task, int16_t loop);

    /** Clears the allocated pixels of the task, if they were allocated */
    void clearTask(NeuQuantTask& task);

    /** Fetch the next data_encode, and make a new empty one after that, uses a mutex lock for data integrity */
    NeuQuantTask* GifEncoder::nextEncode(const int index, const int width, const int height, bool& first);

    /** Attempts to abort a cancelled/failed file */
    bool abort(const bool fail);

    /** Attempts to free the allocated memory of a failed/cancelled file, MIGHT CRASH SOMETIMES */
    void freeEverything();

    inline void factorize(NeuQuantTask& task);

    inline int getSamplePixels(NeuQuantTask& task) {
        return task.lengthcount / (3 * m_quality);
    }

    // SkrFractal's vareiables:
    NeuQuantTask
        * data_first = nullptr,         // first task data (for a global colormap pointer)
        * data_push = nullptr,          // task data to push frame
        * data_write = nullptr;         // task data to write next
    bool transparent,                   // is it transparent?
        finishedAnimation = false,      // was the file completed?
        m_parallel = false;             // is it running in parallel mode?
    int transR, transG, transB,         // transparent color
        samplepixels,       // presaved width * height * 3 / (3 * quality) for the neuquant's learn
        factor,             // balanced factor for splitting the neuquant's learn loop over samplepixels
        finishedFrame = 0;  // how many frames have been written?
    std::mutex mtx;         // mutex for parallel integrity
#pragma endregion

    // SkrFractals expansion end
    // -------------------------
};

#endif //GIF_GIFENCODER_H
