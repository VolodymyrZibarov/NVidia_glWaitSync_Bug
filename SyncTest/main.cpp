#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "glad/gl.h"

#include "Shader.h"

namespace {
const uint32_t texturesCount = 4;
const uint32_t texWidth = 1920;
const uint32_t texHeight = 1080;
const uint32_t bpp = 4;
const uint32_t dataSize = texWidth * texHeight * bpp;

const uint32_t barsCount = 8;
const uint32_t barPeriod = texWidth / barsCount;
const uint32_t barWidth = barPeriod / 2;
const uint32_t barMoveStep = 4;

struct TextureBuffer
{
    GLuint pbo = 0;
    GLuint texture = 0;
    GLsync sync = 0;
};

void generateBars(uint8_t *data, size_t size, uint32_t offset)
{
    for (uint32_t y = 0; y < texHeight; ++y) {
        for (uint32_t x = 0; x < texWidth; ++x) {
            const uint8_t value = ((x + offset) / barWidth % 2 == 0) ? 255 : 0;
            const size_t index = (y * texWidth + x) * bpp;
            for (uint32_t i = 0; i < bpp; ++i) {
                data[index + i] = value;
            }
        }
    }
}

std::vector<TextureBuffer> createBuffers() {
    std::vector<TextureBuffer> result;
    for (uint32_t i = 0; i < texturesCount; ++i) {
        TextureBuffer buffer;

        glGenTextures(1, &buffer.texture);
        if (!buffer.texture) {
            printf("glGenTextures failed\n");
            exit(1);
        }
        glBindTexture(GL_TEXTURE_2D, buffer.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenBuffers(1, &buffer.pbo);
        if (!buffer.pbo) {
            printf("glGenBuffers failed\n");
            exit(1);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, texWidth * texHeight * 4, NULL, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        result.push_back(buffer);
    }
    return result;
}

void destroyBuffers(std::vector<TextureBuffer> buffers) {
    for (const auto &buf : buffers) {
        glDeleteTextures(1, &buf.texture);
        glDeleteBuffers(1, &buf.pbo);
    }
}

bool processSdlEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT: return false;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_CLOSE: return false;
            default: break;
            }
            break;
        default: break;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    printf("Started\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s", SDL_GetError());
        exit(1);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
        printf("SDL_GetDesktopDisplayMode failed\n");
        exit(1);
    }
    printf("Desktop display mode: %i x %i @ %i\n", mode.w, mode.h, mode.refresh_rate);

    auto *window = SDL_CreateWindow("Screenberry", 0, 0, mode.w, mode.h,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        printf("SDL_CreateWindow failed\n");
        exit(1);
    }

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    SDL_GLContext parallelContext = SDL_GL_CreateContext(window);
    SDL_GLContext mainContext = SDL_GL_CreateContext(window);
    if (!parallelContext || !mainContext) {
        printf("SDL_GL_CreateContext failed\n");
        exit(1);
    }

    if (SDL_GL_SetSwapInterval(1) != 0) {
        printf("SDL_GL_SetSwapInterval failed\n");
        exit(1);
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        printf("gladLoadGL failed\n");
        exit(1);
    }

    std::mutex mutex;
    std::condition_variable cond;
    bool parallelMadeCurrent = false;
    std::atomic_bool finished = false;

    std::vector<TextureBuffer> buffers;
    bool buffersReady = false;

    uint32_t readIndex = 0;
    uint32_t writeIndex = 0;

    std::thread thread([window, parallelContext, &finished, &mutex, &cond, &parallelMadeCurrent,
                        &buffers, &writeIndex, &readIndex, &buffersReady]() {
        {
            std::lock_guard guard(mutex);
            SDL_GL_MakeCurrent(window, parallelContext);
            parallelMadeCurrent = true;
            cond.notify_all();
        }
        {
            std::unique_lock lock(mutex);
            while (!finished && !buffersReady) {
                cond.wait(lock);
            }
        }
        auto data = std::make_unique<uint8_t[]>(dataSize);
        uint32_t barsOffset = 0;
        while (!finished) {
            barsOffset = (barsOffset + barMoveStep) % barPeriod;
            generateBars(data.get(), dataSize, barsOffset);

            {
                std::unique_lock lock(mutex);
                while (!finished && writeIndex != readIndex) {
                    cond.wait(lock);
                }
            }

            TextureBuffer &writebuffer = buffers[writeIndex];

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, writebuffer.pbo);
            auto mappedPtr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dataSize,
                                              GL_MAP_WRITE_BIT);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            std::memcpy(mappedPtr, data.get(), dataSize);

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, writebuffer.pbo);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            glBindTexture(GL_TEXTURE_2D, writebuffer.texture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, writebuffer.pbo);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                            0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            writebuffer.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();

            {
                std::lock_guard guard(mutex);
                writeIndex = (writeIndex + 1) % texturesCount;
                cond.notify_all();
            }
        }
    });
    {
        std::unique_lock lock(mutex);
        while (!parallelMadeCurrent) {
            cond.wait(lock);
        }
    }

    buffers = createBuffers();
    {
        std::lock_guard guard(mutex);
        buffersReady = true;
        cond.notify_all();
    }

    auto shader = std::make_unique<Shader>();
    uint32_t frame = 0;
    while (true) {
        if (!processSdlEvents()) {
            break;
        }

        {
            std::unique_lock lock(mutex);
            while (writeIndex == readIndex) {
                cond.wait(lock);
            }
        }

        TextureBuffer &readBuffer = buffers[readIndex];

        if (!readBuffer.sync) {
            printf("Error: No sync\n");
            exit(1);
        }
        glWaitSync(readBuffer.sync, 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(readBuffer.sync);
        readBuffer.sync = nullptr;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, mode.w, mode.h);

        shader->render(readBuffer.texture);

        SDL_GL_SwapWindow(window);

        if (auto err = glGetError(); err != GL_NO_ERROR) {
            printf("GL error: 0x%04x\n", err);
            exit(1);
        }

        {
            std::lock_guard guard(mutex);
            readIndex = (readIndex + 1) % texturesCount;
            cond.notify_all();
        }

        frame++;
    }
    shader = {};
    printf("Rendered %i frames\n", frame);
    finished = true;
    cond.notify_all();
    thread.join();

    destroyBuffers(std::move(buffers));

    SDL_GL_DeleteContext(parallelContext);
    SDL_GL_DeleteContext(mainContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Finished\n");
    return 0;
}
