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

#define CHECK_GL_ERROR \
    if (auto err = glGetError(); err != GL_NO_ERROR) { \
        printf("GL error 0x%04x at line %i\n", int(err), __LINE__); \
        exit(1); \
    } \
    (void)0

int main(int argc, char **argv)
{
    printf("Started\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    printf("SDL inited\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
        printf("SDL_GetDesktopDisplayMode failed\n");
        return 1;
    }
    printf("Desktop display mode: %i x %i @ %i\n", displayMode.w, displayMode.h,
           displayMode.refresh_rate);

    uint32_t windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN
                           | SDL_WINDOW_BORDERLESS;

    auto *window = SDL_CreateWindow("Screenberry", 0, 0, displayMode.w, displayMode.h, windowFlags);
    if (!window) {
        printf("SDL_CreateWindow failed");
        return 1;
    }
    printf("Window created\n");

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    SDL_GLContext parallelContext = SDL_GL_CreateContext(window);
    SDL_GLContext mainContext = SDL_GL_CreateContext(window);
    if (!parallelContext || !mainContext) {
        printf("SDL_GL_CreateContext failed\n");
        return 1;
    }

    if (SDL_GL_SetSwapInterval(1) != 0) {
        printf("SDL_GL_SetSwapInterval failed");
        return 1;
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        printf("gladLoadGL failed\n");
        return 1;
    }
    std::mutex mutex;
    std::condition_variable cond;
    bool parallelMadeCurrent = false;
    std::atomic_bool finished = false;
    struct TextureBuffer
    {
        GLuint texture = 0;
        GLsync sync = 0;
    };
    std::vector<TextureBuffer> buffers;
    const uint32_t texturesCount = 5;
    const uint32_t texWidth = 1920;
    const uint32_t texHeight = 1080;
    uint32_t readIndex = 0;
    uint32_t writeIndex = 0;
    GLsync glSync = nullptr;

    std::thread thread([window, parallelContext, &finished, &mutex, &cond, &parallelMadeCurrent,
                        texWidth, texHeight, texturesCount, &buffers, &writeIndex, &readIndex,
                        &glSync]() {
        {
            std::lock_guard guard(mutex);
            SDL_GL_MakeCurrent(window, parallelContext);

            for (uint32_t i = 0; i < texturesCount; ++i) {
                TextureBuffer buffer;
                glGenTextures(1, &buffer.texture);
                glBindTexture(GL_TEXTURE_2D, buffer.texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, 0);
                buffers.push_back(buffer);
            }

            parallelMadeCurrent = true;
            cond.notify_all();
        }
        const uint32_t channels = 4;
        const uint32_t dataSize = texWidth * texHeight * channels;
        auto data = std::make_unique<uint8_t[]>(dataSize);
        uint32_t offset = 0;
        const uint32_t step = 5;
        const uint32_t barsCount = 4;
        const uint32_t barPeriod = texWidth / barsCount;
        const uint32_t barWidth = barPeriod / 2;
        while (!finished) {
            for (uint32_t y = 0; y < texHeight; ++y) {
                for (uint32_t x = 0; x < texWidth; ++x) {
                    const bool white = (x + offset) / barWidth % 2 == 0;
                    const uint8_t value = white ? 255 : 0;
                    const size_t index = (y * texWidth + x) * channels;
                    for (uint32_t channel = 0; channel < channels; ++channel) {
                        data[index + channel] = value;
                    }
                }
            }
            {
                std::unique_lock lock(mutex);
                while (!finished && writeIndex != readIndex) {
                    cond.wait(lock);
                }
            }
            glBindTexture(GL_TEXTURE_2D, buffers[writeIndex].texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                            (const GLvoid *)data.get());
            CHECK_GL_ERROR;
            buffers[writeIndex].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();
            {
                std::lock_guard guard(mutex);
                writeIndex = (writeIndex + 1) % texturesCount;
                cond.notify_all();
            }
            offset = (offset + step) % barPeriod;
        }
    });
    {
        std::unique_lock lock(mutex);
        while (!parallelMadeCurrent) {
            cond.wait(lock);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    CHECK_GL_ERROR;
    uint32_t frame = 0;
    {
        Shader shader;
        CHECK_GL_ERROR;
        while (true) {
            //            printf("frame: %i\n", frame);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: finished = true; break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE: finished = true; break;
                    default: break;
                    }
                    break;
                default: break;
                }
            }
            if (finished) {
                break;
            }

            {
                std::unique_lock lock(mutex);
                while (writeIndex == readIndex) {
                    cond.wait(lock);
                }
            }
            glWaitSync(buffers[readIndex].sync, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(buffers[readIndex].sync);
            buffers[readIndex].sync = nullptr;

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glViewport(0, 0, displayMode.w, displayMode.h);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            const uint32_t windowsX = displayMode.w / 960;
            const uint32_t windowsY = displayMode.h / 540;
            const uint32_t windowSizeX = displayMode.w / windowsX;
            const uint32_t windowSizeY = displayMode.h / windowsY;
            const uint32_t padding = 5;
            for (uint32_t wy = 0; wy < windowsY; ++wy) {
                for (uint32_t wx = 0; wx < windowsX; ++wx) {
                    const uint32_t x = wx * windowSizeX + padding;
                    const uint32_t y = wy * windowSizeY + padding;
                    const uint32_t sx = windowSizeX - padding * 2;
                    const uint32_t sy = windowSizeY - padding * 2;
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(x, y, sx, sy);
                    glViewport(x, y, sx, sy);
                    shader.render(buffers[readIndex].texture);
                    glDisable(GL_SCISSOR_TEST);
                }
            }

            CHECK_GL_ERROR;

            SDL_GL_SwapWindow(window);

            {
                std::lock_guard guard(mutex);
                readIndex = (readIndex + 1) % texturesCount;
                cond.notify_all();
            }

            frame++;
        }
    }
    printf("Rendered %i frames\n", frame);
    cond.notify_all();
    thread.join();

    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Finished\n");
    return 0;
}
