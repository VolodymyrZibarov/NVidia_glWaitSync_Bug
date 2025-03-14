#include <cstdio>
#include <string>
#include <vector>
#include <chrono>

#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "glad/gl.h"

#define NOMINMAX
#include <windows.h>

namespace {

void critical(const std::string &str)
{
    MessageBoxA(NULL, "Critical Error", str.c_str(), MB_OK | MB_ICONERROR);
}

void info(const std::string &str)
{
    MessageBoxA(NULL, str.c_str(),
                "OpenGL buffer map test app by Volodymyr Zibarov, 2025",
                MB_OK | MB_ICONINFORMATION);
}

const uint32_t buffersCount = 10;
const uint32_t bufferSize = 2048 * 4096 * 2; // 2048 x 4096 YUV 422
GLuint buffers[buffersCount] = {};

void createBuffers()
{
    glGenBuffers(buffersCount, buffers);
    for (uint32_t i = 0; i < buffersCount; ++i) {
        if (buffers[i] == 0) {
            critical("glGenBuffers failed");
            exit(1);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffers[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize, NULL, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
}

std::vector<int> mapBuffers()
{
    std::vector<int> result;
    for (uint32_t i = 0; i < buffersCount; ++i) {
        auto start = std::chrono::steady_clock::now();
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffers[i]);
        auto *mappedPtr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, bufferSize,
                                           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (!mappedPtr) {
            critical("Error: glMapBufferRange failed\n");
            exit(1);
        }
        memset(mappedPtr, 0, bufferSize);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
        result.push_back(timeMs);
    }
    return result;
}

void destroyBuffers() { glDeleteBuffers(buffersCount, buffers); }

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        critical("SDL_Init failed: " + std::string(SDL_GetError()));
        exit(1);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    auto *window = SDL_CreateWindow("OpenGL Buffer Maping Test", 200, 100, 800, 600,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        critical("SDL_CreateWindow failed");
        exit(1);
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        critical("SDL_GL_CreateContext failed");
        exit(1);
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        critical("gladLoadGL failed");
        exit(1);
    }

    createBuffers();
    auto resultsMs = mapBuffers();
    destroyBuffers();
    processSdlEvents();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (resultsMs.empty() || buffersCount == 0) {
        critical("No measurements");
        exit(1);
    }
    int minMs = resultsMs[0];
    int maxMs = minMs;
    int sumMs = minMs;
    for (int i = 1; i < buffersCount; ++i) {
        minMs = std::min(minMs, resultsMs[i]);
        maxMs = std::max(maxMs, resultsMs[i]);
        sumMs += resultsMs[i];
    }
    int avgMs = sumMs / buffersCount;

    info("Test results: Map and write time min " + std::to_string(minMs) + " / avg "
         + std::to_string(avgMs) + " / max " + std::to_string(maxMs) + " ms.\nTest "
         + std::string(maxMs > 30 ? "FAILED" : "Passed OK"));

    return 0;
}
