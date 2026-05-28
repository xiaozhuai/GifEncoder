//
// Created by xiaozhuai on 2020/12/20.
// Expanded by SkrFractals on 2025/01/31
//

#include <exception>
#include <stdexcept>
#include <cstdlib>
#include "GifEncoder.h"

#define m_gifFile ((GifFileType *) m_gifFileHandler)
#define GifAddExtensionBlockFor(a, func, len, data) GifAddExtensionBlock(       \
        &((a)->ExtensionBlockCount),                                            \
        &((a)->ExtensionBlocks),                                                \
        func, len, data)

#pragma region TASK_STRUCT
NeuQuantTask::NeuQuantTask(const int width, const int height) {
    factor = 0;
    cancel = nullptr;
    nextTask = nullptr;
    thisTask = nullptr;
    colorMap = nullptr;
    thepicture = nullptr;
    rasterBits = nullptr;
    finished = failed = false;
    transIndex = NO_TRANSPARENT_COLOR;
    frameIndex = m_allocSize = transR = transG = transB = -1;
    setSize(width, height);
}

void NeuQuantTask::setTransparent(const int b, const int g, const int r) {
    transB = b;
    transG = g;
    transR = r;
}

bool NeuQuantTask::setSize(const int width, const int height) {
    const int pl = lengthcount; // remember old size
    return pl != (lengthcount = 3 * (m_frameWidth = width) * (m_frameHeight = height)); // set the new size and return true if it's different
}

bool NeuQuantTask::alloc(int needSize) {
    if (m_allocSize >= (needSize *= lengthcount))
        return false; // already allocated enough
    void* tryalloc = m_allocSize <= 0 // try allocate or reallocate
        ? malloc(m_allocSize = needSize)
        : realloc(thepicture, m_allocSize = needSize);
    if (tryalloc == nullptr)
        return true; // failed to allocate memory
    thepicture = (uint8_t*)tryalloc;
    return false; // success
}

bool NeuQuantTask::convertToBGR(GifEncoderPixelFormat format, uint8_t* dst, const uint8_t* src) const {
    switch (format) {
    case PIXEL_FORMAT_BGR_NATIVE: // in case of global color map, it will need to get copied anyway
    case PIXEL_FORMAT_BGR:
        memcpy(dst, src, lengthcount); // just copy
        break;
    case PIXEL_FORMAT_RGB: // flip to RGB
        for (const uint8_t* dstEnd = dst + lengthcount; dst < dstEnd; src += 3) {
            *(dst++) = *(src + 2);
            *(dst++) = *(src + 1);
            *(dst++) = *(src);
        }
        break;
    case PIXEL_FORMAT_BGRA: // toss A
        for (const uint8_t* dstEnd = dst + lengthcount; dst < dstEnd; src += 4) {
            *(dst++) = *(src);
            *(dst++) = *(src + 1);
            *(dst++) = *(src + 2);
        }
        break;
    case PIXEL_FORMAT_RGBA: // flip to RBG and toss A
        for (const uint8_t* dstEnd = dst + lengthcount; dst < dstEnd; src += 4) {
            *(dst++) = *(src + 2);
            *(dst++) = *(src + 1);
            *(dst++) = *(src);
        }
        break;
    default:
        return false;
    }
    return true;
}

bool NeuQuantTask::getColorMap(const int quality) {
    colorMap = GifMakeMapObject(256, nullptr);
    neuquant.initnet(); // Initializes the neural network with random weights.
    if (neuquant.learn(thepicture, lengthcount, quality, samplepixels, factor, cancelType, cancel)) // Trains the neural network on input image colors.
        return true; // cancelled
    neuquant.unbiasnet(); // Adjusts the trained network to compensate for bias introduced during training.
    neuquant.inxbuild(); // Constructs an indexed lookup table for faster nearest-neighbor searches.
    neuquant.getcolourmap((uint8_t*)colorMap->Colors); // Extracts the final palette (typically 256 colors) from the trained network.
    return false;
}

bool NeuQuantTask::getRasterBits(uint8_t* pixels, NeuQuantTask* task) {
    const int pix = m_frameWidth * m_frameHeight; // get frame pixel count
    rasterBits = (GifByteType*)malloc(pix); // allocate raster bits

    NeuQuant& nq = task == nullptr ? neuquant : task->neuquant;

    transIndex = transR < 0 ? NO_TRANSPARENT_COLOR : nq.inxsearch(transB, transG, transR); // get transparent index
    if (cancel == nullptr) {
        // without cancellation token:
        for (int i = 0; i < pix; getRasterPix(nq, pixels, i++));
        return false; // finished
    }
    // with cancellation token:
    //auto& tokenR = *cancel; // dereference the token
    for (int i = 0, y = m_frameHeight; y > 0; --y) {
        if (isCancel(cancelType, cancel))
            return true; // cancelled
        for (int x = m_frameWidth; x > 0; --x)
            getRasterPix(nq, pixels, i++);
    }
    return false; // finished
}
#pragma endregion

#pragma region ORIGINAL_COMPATIBILITY
bool GifEncoder::open(const std::string& file, const int width, const int height,
                      const int quality, const bool useGlobalColorMap, const int16_t loop, const int preAllocSize, bool cancelType, void* cancel) {
    if (initOpen(file, width, height))
        return true; // failed to open
    m_quality = quality; // remember quality
    m_parallel = false; // flag that we are NOT running a parallel mode. whis will make any parallel calls refuse
    auto& task = *data_push; // dereference the data_push task
    if (preAllocSize > 0) // preallocate the frame buffer
        task.alloc(preAllocSize);
    task.cancelType = cancelType;
    task.cancel = cancel; // set the task's cancel token
    task.setSize(width, height); // set the task size
    if (!(m_useGlobalColorMap = useGlobalColorMap)) {  // global color map is available in old synchronous mode
        task.samplepixels = samplepixels = getSamplePixels(task); // calculate the samplepixels
        factorize(task); // find a balanced factor of samplepixels
    }
    return endOpen(task, loop);
}

#define UpdateCancel(cancelType, cancel) if (cancel != nullptr) { task.cancel = cancel; task.cancelType = cancelType; }

bool GifEncoder::push(const GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height, const int delay, const int frameIndex, bool cancelType, void* cancel) {
    if (m_gifFile == nullptr || frame == nullptr || m_parallel)
        return abort(true); // no file or no image or opened parallel - fail
    bool first;
    if (!m_useGlobalColorMap) // local color map branch:
        return setupTask(nextEncode(frameIndex, width, height, first), format, frame, width, height, false, delay, cancelType, cancel); // setup the task with all the supplied data
    auto& task = *data_push; // dereference the data_push task 
    if (task.setSize(width, height)) // set the task's size
        return abort(true); //throw std::runtime_error("Frame size must be same when use global color map!");
    if (transparent) // set the tasks's transparency
        task.setTransparent(transB, transG, transR);
    UpdateCancel(cancelType, cancel) // update the task's cancel token
        int fi = frameIndex < 0 ? m_frameCount : frameIndex; // get the frame index of this frame (automatic or manual)
    int allocateframes = fi > m_frameCount ? fi : m_frameCount; // how much memory needs to be allocated for this?
    if (task.alloc(allocateframes + 1) || !task.convertToBGR(format, task.thepicture + task.lengthcount * fi, frame)) // allocate the memory until the last byte of this frame and try to convert into BGR
        return abort(true); // failed to allocate memory or convert to BGR
    m_allFrameDelays.push_back(delay); // add delay to the array to be used later
    ++m_frameCount; // only do this here, because otherwise the nextEncode does it
    return true; // success
}

bool GifEncoder::close(bool multiFrameGlobalMap, bool cancelType, void* cancel) {
    if (m_gifFile == nullptr)
        return abort(true); // file not opened - fail
    if (m_parallel) // if parallel mode, just mark the last task as finished wihtout starting it, that will let the tryWrite know it's the end
        return data_push->finished = true;
    auto& task = *data_write; // dereference the data_write task
    if (m_useGlobalColorMap) { // encode the whole gif here if global map (this is why I can't parallelize this)
        UpdateCancel(cancelType, cancel) // update the task's cancel token
        if (multiFrameGlobalMap)
            task.lengthcount *= m_frameCount;
        task.samplepixels = samplepixels = getSamplePixels(task); // calculate the samplepixels
        factorize(task); // if we supplied token just now, calculate the missing factor    
        task.getColorMap(m_quality); // get global color map of the whole animation
        m_gifFile->SColorMap = task.colorMap;
        if (multiFrameGlobalMap)
            task.lengthcount /= m_frameCount;
        // write all the global map frames into the file:
        for (int i = 0; i < m_frameCount; ++i) { // raster and encode all the frames
            task.getRasterBits(task.thepicture + task.lengthcount * i);
            encodeFrame(task, m_allFrameDelays[i], true); // encode the frame with a global colormap flag (but it still does write teh color map for each frame...?)
        }
    } else {
        m_parallel = true; // this will allow us to use tryWrite in this closing finalization (it's not allowed to use manually in classic mode)
        while (data_write != nullptr && data_write->finished) {
            // if we were pushing frames out of order in non-parallel mode, we might need to finish writing
            const auto r = tryWriteInternal();
            if (r == GifEncoderTryWriteResult::Failed || r == GifEncoderTryWriteResult::Waiting)
                return abort(true); // something went wrong
        }
    }
    // finish the file
    int error, ok = true;
    if (EGifSpew(m_gifFile) == GIF_ERROR) { // write the file
        EGifCloseFile(m_gifFile, &error); // close the file if an error happened
        m_gifFileHandler = nullptr;
        reset();
        return false;
    }
    freeEverything();
    return ok && (finishedAnimation = true);
}

bool GifEncoder::setupTask(NeuQuantTask* pushPtr, const GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height, const bool first, const int delay, bool cancelType, void* cancel) {
    if (pushPtr == nullptr)
        return abort(true); // something must have failed in previous task if its returning null
    auto& task = *pushPtr; // dereference the data_push task
    task.m_delay = delay; // set the task's delay
    if (task.setSize(width, height)) {
        samplepixels = getSamplePixels(task); // update samplepixels
        factor = 0; // reset factor
    }
    if (transparent) // set the task's transparent color
        task.setTransparent(transB, transG, transR);
    UpdateCancel(cancelType, cancel) // update the task's cancel token
        task.samplepixels = samplepixels; // give the samplepixels to the task
    task.factor = factor; // give the smaplepixels balanced factorization factor to the task
    factorize(task); // if there's a token and the factor is zero, calculate the factor
    if (format == GifEncoderPixelFormat::PIXEL_FORMAT_BGR_NATIVE)  // give the task the image pixels
        task.thepicture = frame; // use the pixels directly without copying if they are already BGR and we allow not copying with the "NATIVE" option
    else if (task.alloc(1) || !task.convertToBGR(format, task.thepicture, frame)) // otherwise, do the copy allocation coversion
        return abort(true); // failed to allocate memory or convert to BGR
    if (first || !m_useGlobalColorMap) { // a parallel single frame globalmap or local map
        if (task.getColorMap(m_quality)) // run the neuquant learning
            return abort(task.failed = true); // fail if cancelled
        if (task.getRasterBits(task.thepicture)) // make raster bits
            return abort(task.failed = true); // fail if cancelled
    }
    return task.finished = true;
}

void GifEncoder::encodeFrame(NeuQuantTask& task, const int delay, const bool globalColorMap) {
    auto* gifImage = GifMakeSavedImage(m_gifFile, nullptr);

    gifImage->ImageDesc.Left = 0;
    gifImage->ImageDesc.Top = 0;
    gifImage->ImageDesc.Width = task.m_frameWidth;
    gifImage->ImageDesc.Height = task.m_frameHeight;
    gifImage->ImageDesc.Interlace = false;
    // ISSUE: if we have a global colormap, this code appears to still frite it down for every frame, instead of just writing it once at the beginning of the file, can we optimize this?
    gifImage->ImageDesc.ColorMap = (ColorMapObject*)(globalColorMap ? data_first->colorMap : task.colorMap);
    gifImage->RasterBits = (GifByteType*)task.rasterBits;
    gifImage->ExtensionBlockCount = 0;
    gifImage->ExtensionBlocks = nullptr;

    GraphicsControlBlock gcb;
    gcb.DisposalMode = task.transIndex < 0 ? DISPOSE_DO_NOT : DISPOSE_BACKGROUND; // dispose the background if its transparent
    gcb.UserInputFlag = false;
    gcb.DelayTime = delay;
    gcb.TransparentColor = task.transIndex; // set the transparent color
    uint8_t gcbBytes[4];
    EGifGCBToExtension(&gcb, gcbBytes);
    GifAddExtensionBlockFor(gifImage, GRAPHICS_EXT_FUNC_CODE, sizeof(gcbBytes), gcbBytes);
}

bool GifEncoder::abort(const bool fail) {
    if (!fail)
        return true; // do not abourt
    int error;
    mtx.lock();
    if (m_gifFile != nullptr) // close the file if opened
        EGifCloseFile(m_gifFile, &error);
    freeEverything(); // try to free the data
    m_gifFileHandler = nullptr; // forget the file
    mtx.unlock();
    reset(); // reset the encoder
    return false; // successfully aborted
}

void GifEncoder::freeEverything() {
    if (m_gifFile == nullptr)
        return;

    // ISSUE: this keeps randomly crashing, when i debugged, it looks like the EGifSpew already frees these things
    /*
    int extCount = m_gifFile->ExtensionBlockCount;
    auto* extBlocks = m_gifFile->ExtensionBlocks;
    int savedImageCount = m_gifFile->ImageCount;
    auto* savedImages = m_gifFile->SavedImages;
    if(extCount > 0)
        GifFreeExtensions(&extCount, &extBlocks);
    if (savedImageCount > 0) {
        for (auto* sp = savedImages; sp < savedImages + savedImageCount; sp++) {
            if (sp->ImageDesc.ColorMap != nullptr) {
                GifFreeMapObject(sp->ImageDesc.ColorMap);
                sp->ImageDesc.ColorMap = nullptr;
            }
            if (sp->RasterBits != nullptr) {
                free((char*)sp->RasterBits);
                sp->RasterBits = nullptr;
            }
            GifFreeExtensions(&sp->ExtensionBlockCount, &sp->ExtensionBlocks);
        }
        free(savedImages);
    }
    */
    m_gifFileHandler = nullptr; // forget the file
    reset(); // reset the encoder
}

inline void GifEncoder::reset() {
    // Free the task memory, encode should always be the same as write or further, so start clearing out from write
    while (data_first != nullptr) {
        clearTask(*data_first); // free the allocated framebuffer of the task
        data_first = data_first->nextTask;
    }
    data_push = data_write = nullptr; // reset the task list
    m_allFrameDelays.clear(); // reset the delays
    m_frameCount = 0; // reset the frame number
}
#pragma endregion

#pragma region EXPANSION
bool GifEncoder::open_parallel(const std::string& file, const int width, const int height,
                               const int quality, const bool useGlobalColorMap, int16_t loop, bool cancelType, void* cancel) {
    if (initOpen(file, width, height)) // open the file
        return true; // open failed
    m_quality = quality; // remember quality
    m_useGlobalColorMap = useGlobalColorMap; // globa color map dones't work it parallel mode, because i am literally parallel batching the separate local map learnings
    m_parallel = true; // flag that we are running a parallel mode. whis will make any synchronous calls refuse
    auto& task = *data_push; // dereference the data_push
    task.cancel = cancel; // set the task's cancel token
    task.cancelType = cancelType;
    task.setSize(width, height); // set the task's size
    task.samplepixels = samplepixels = getSamplePixels(task); // calculate the samplepixels
    factorize(task); // calculate the balanced factorization of samplepixels
    return endOpen(task, loop); // write the file header
}

bool GifEncoder::push_parallel(GifEncoderPixelFormat format, uint8_t* frame, const int width, const int height, const int delay, const int frameIndex, bool cancelType, void* cancel) {
    if (m_gifFile == nullptr || frame == nullptr || !m_parallel)
        return abort(true); // file not opened, or frame invlaid, or not parallel - fail
    mtx.lock(); // lock this fetch for parallelism data integrity
    bool first;
    auto pushPtr = nextEncode(frameIndex, width, height, first); // fetch the task data struct to run the task from, and make another empty one after that
    mtx.unlock(); // can continue in parallel from here
    return setupTask(pushPtr, format, frame, width, height, first, delay, cancelType, cancel); // setup the task with all the supplied data
}

GifEncoderTryWriteResult GifEncoder::tryWrite(bool parallel) {
    GifEncoderTryWriteResult r;
    if (parallel) // lock if we are calling this in parallel to preserve data integrity
        mtx.lock();
    r = tryWriteInternal(); // call the internal function itself from this public wrapper
    if (r == GifEncoderTryWriteResult::Failed)
        abort(true); // abort if the fucntion failed
    if (parallel)
        mtx.unlock();
    return r; // return the function's return
}

GifEncoderTryWriteResult GifEncoder::tryWriteInternal() {
    if (!m_parallel)
        return GifEncoderTryWriteResult::Failed; // This should only be called to attempt to write frames in parallel mode! With original synchronous mode, just call the close!
    if (finishedAnimation)
        return GifEncoderTryWriteResult::FinishedAnimation; // Already finished (you seem to have called the tryWrite even after it resturned finished once already)
    if (m_gifFile == nullptr || data_write->failed)
        return GifEncoderTryWriteResult::Failed;    // No file opened, or something failed in the frame pushing
    auto write = data_write;
    if (write == nullptr)
        return GifEncoderTryWriteResult::Waiting; // no frames yet
    const bool parallelGlobalColorMap = m_parallel && m_useGlobalColorMap;
    if (parallelGlobalColorMap && !data_first->finished)
        return GifEncoderTryWriteResult::Waiting; // global single need to finish making the map
    // Find the task containing the next frame to write into the file
    NeuQuantTask* prev = nullptr;
    while (write->frameIndex > finishedFrame) {
        if (write->nextTask == nullptr) {
            return write->finished
                ? GifEncoderTryWriteResult::Failed	    // You must have called finish while having gaps in supplied out of order frames!
                : GifEncoderTryWriteResult::Waiting;	// You have not supplied the next out of order frame yet, so the Task has not even started running yet.
        }
        write = (prev = write)->nextTask;
    }
    if (write == nullptr)
        return GifEncoderTryWriteResult::Failed; // This should never happen, but I'm gonna check it anyway. (the last taskdata should always have frameIndex -1, so the while should not go to nullptr from there)
    auto& w = *write;
    if (!w.finished)
        return GifEncoderTryWriteResult::Waiting; // the task for the next sequential frame is not finished yet, try again later
    if (w.nextTask == nullptr) {
        // i always assign to next right before i start its task, so if i set the finished flag without starting a task, that means i have called close() and there's no next frame
        // The finish normally closed the filestream, but not I will do it here
        // and the next close() just sets the encodeTaskData->finished = true to trigger this code when all frames wating to be written have been written
        m_useGlobalColorMap = w.finished = m_parallel = false; // makes sure it actually writed the file end instead of setting the finish flag again, and reset the finish flag so it doesn't attempt to write this task, and reset global color map so it doesn't attempt to write the regular global color map
        return close() ? GifEncoderTryWriteResult::FinishedAnimation : GifEncoderTryWriteResult::Failed; // Has writing the full file finished or failed?
    }
    // this nextTask is not null, so this task is actually a frame and not an end yet, so write that frame into the file
    if (w.frameIndex < finishedFrame) {
        // You must have messed up supplying the frameIndexes! The should be a queued frame with an index less than what has alraedy been written!
        // This has probablty happened because you either mixed the automatic ordered, and out of order AddFrame.
        // Or you messed out the out of order frame counting and supplied the same index twice.
        return GifEncoderTryWriteResult::Failed;
    }
    if (w.thisTask != nullptr && w.thisTask->joinable())
        w.thisTask->join(); // join the task if if was created here
    if (parallelGlobalColorMap && write != data_first && write->getRasterBits(write->thepicture, data_first))
        return GifEncoderTryWriteResult::Failed;// failed to make indexes pixels from a global color map
    // write the frame into the file
    encodeFrame(w, w.m_delay, parallelGlobalColorMap);
    // increment the counter of frames written into the file
    ++finishedFrame;
    //remove the written task from the list:
    if (prev == nullptr) {
        // Now that I wrote the frame, I can forget the task data, and move on to the next one.
        data_write = data_write->nextTask;
    } else {
        // The first queued writeTaskData was not the next sequential frame to be written and we were not writing that one...
        // ...so we have to skip and forget the one later in the list that we actually wrote
        prev->nextTask = w.nextTask;
    }
    return GifEncoderTryWriteResult::FinishedFrame; // Has writing this next frame finished or failed?
}

NeuQuantTask* GifEncoder::nextEncode(const int index, const int width, const int height, bool& first) {
    NeuQuantTask* encode = nullptr;
    // make sure to monitor lock this part to preserve the list integrity while multithreading
    if (m_gifFile == nullptr || (encode = data_push)->failed)
        return nullptr; // file not opened, or previous task failed
    //create the new empty task on the end of the list, we will return the previous one (the one before the new last one)
    data_push = encode->nextTask = new NeuQuantTask(width, height); // make a next one for the next call of Addframe to work on
    encode->frameIndex = index < 0 ? m_frameCount : index; // add automatic sequential, or manual our of order frameIndex
    first = m_frameCount == 0;
    ++m_frameCount; // increment the pushed framecount
    return encode;
}

bool GifEncoder::initOpen(const std::string& file, const int width, const int height) {
    if (m_gifFile != nullptr)
        return true; // file already opened
    int error;
    m_gifFileHandler = EGifOpenFileName(file.c_str(), false, &error);
    if (!m_gifFile)
        return true; // file failed to open
    finishedAnimation = false; // reset the finished flag
    factor = finishedFrame = m_frameCount = 0; // reset the counters and the factor
    transR = transG = transB = -1; // reset transparency
    transparent = false; // turn off transparency
    reset(); // reset the encoder
    data_first = data_push = data_write = new NeuQuantTask(width, height); // create a fresh new task list
    return false; // file successfuly opened
}

bool GifEncoder::endOpen(NeuQuantTask& task, const int16_t loop) {
    m_gifFile->SWidth = task.m_frameWidth; m_gifFile->SHeight = task.m_frameHeight;
    m_gifFile->SColorResolution = 8;
    m_gifFile->SBackGroundColor = 0;
    m_gifFile->SColorMap = nullptr;

    // maybe this will fix the random crashes when freeing the data...? (no it didn't)
    //m_gifFile->ExtensionBlockCount = 0;
    //m_gifFile->ImageCount = 0;

    uint8_t appExt[11] = {
            'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
            '2', '.', '0'
    };
    uint8_t appExtSubBlock[3] = {
            0x01,       // hex 0x01
            0x00, 0x00  // little-endian short. The number of times the loop should be executed.
    };

    // ISSUE: windows photo viewer doesn't loop, even when it can loop gifs from other encoders!

    memcpy(appExtSubBlock + 1, &loop, sizeof(loop));
    GifAddExtensionBlockFor(m_gifFile, APPLICATION_EXT_FUNC_CODE, sizeof(appExt), appExt);
    GifAddExtensionBlockFor(m_gifFile, CONTINUE_EXT_FUNC_CODE, sizeof(appExtSubBlock), appExtSubBlock);
    return true;
}

void GifEncoder::clearTask(NeuQuantTask& task) {
    if (task.m_allocSize >= 0) { // is the memory allocated?
        // only clear the pixeldata if they were copy allocated, not when using a byte pointer from the push directly
        free(task.thepicture);
        task.thepicture = nullptr;
    }
    task.m_allocSize = -1; // set the unallocated flag
}

inline void GifEncoder::factorize(NeuQuantTask& task) {
    // factorize the smplepixels if there's a token and the size got changed from the last factorization
    if (task.cancel != nullptr && task.factor == 0) {
        factor = 1; // start with factor 1
        int s = task.samplepixels; // keep factorizing until the factor and smaplepixels get balanced
        for (int t = 2, ft = factor * t, st = s / t; ft - st < s - factor; ft = factor * t, st = s / t) {
            if ((s % t) == 0) { // is samplepixels divisible with this prime?
                factor = ft; // multiply the factor with that prime
                s = st; // divide the amplepixels by that prime
                continue;
            }
            ++t;
        }
    }
    task.factor = factor;
}
#pragma endregion
