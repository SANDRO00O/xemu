/*
 * xemu Xbox One UWP diagnostic frontend.
 *
 * M1 deliberately uses the NV2A Null renderer. The purpose of this frontend is
 * to prove that the Xbox UWP host can initialize xemu's core and enter its
 * main loop without pulling in the desktop SDL/OpenGL frontend.
 */

#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "qemu/rcu.h"
#include "qapi/error.h"
#include "system/system.h"
#include "system/runstate.h"
#include "hw/xbox/nv2a/nv2a.h"
#include "ui/xemu-settings.h"

#include <locale.h>
#include <stdatomic.h>

static QemuThread qemu_thread;
static atomic_bool qemu_initialized;
static atomic_bool qemu_finished;
static atomic_int qemu_exit_status;

static void xemu_queue_error_message(const char *message)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "xemu: %s", message);
}

static void xemu_queue_notification(const char *message)
{
    SDL_Log("xemu: %s", message);
}

static void *xboxone_qemu_thread(void *opaque)
{
    char *argv[] = {
        (char *)"xemu-xboxone",
        (char *)"-machine",
        (char *)"xbox",
        (char *)"-display",
        (char *)"none",
        (char *)"-accel",
        (char *)"tcg",
        (char *)"-m",
        (char *)"64",
        NULL,
    };
    const int argc = 9;

    qemu_init(argc, argv);
    atomic_store_explicit(&qemu_initialized, true, memory_order_release);

    const int status = qemu_main_loop();
    atomic_store_explicit(&qemu_exit_status, status, memory_order_release);
    atomic_store_explicit(&qemu_finished, true, memory_order_release);

    bql_unlock();
    return NULL;
}

static void draw_status(SDL_Renderer *renderer, bool core_ready)
{
    SDL_SetRenderDrawColor(renderer, 8, 8, 12, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 30, 120, 70, SDL_ALPHA_OPAQUE);
    SDL_FRect bar = { 80.0f, 70.0f, 1120.0f, 80.0f };
    SDL_RenderFillRect(renderer, &bar);

    SDL_SetRenderDrawColor(renderer, 235, 235, 235, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 110.0f, 95.0f, "XEMU - XBOX ONE M1");

    SDL_SetRenderDrawColor(renderer, 180, 180, 190, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 110.0f, 190.0f,
                        "UWP frontend: RUNNING");
    SDL_RenderDebugText(renderer, 110.0f, 230.0f,
                        core_ready ? "xemu core: INITIALIZED"
                                   : "xemu core: STARTING");
    SDL_RenderDebugText(renderer, 110.0f, 270.0f,
                        "NV2A renderer: NULL (M1)");
    SDL_RenderDebugText(renderer, 110.0f, 310.0f,
                        "Graphics backend: DEFERRED TO M2");

    SDL_SetRenderDrawColor(renderer, 70, 70, 80, SDL_ALPHA_OPAQUE);
    SDL_FRect footer = { 80.0f, 380.0f, 1120.0f, 2.0f };
    SDL_RenderFillRect(renderer, &footer);

    SDL_SetRenderDrawColor(renderer, 150, 150, 160, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 110.0f, 420.0f,
                        "This screen is intentionally not the guest framebuffer.");
    SDL_RenderDebugText(renderer, 110.0f, 460.0f,
                        "Guest display / D3D12 presentation begins in M2.");

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    setlocale(LC_NUMERIC, "C");

    if (!xemu_settings_load()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to load xemu settings: %s",
                     xemu_settings_get_error_message());
        return 1;
    }

    /*
     * M1 must not initialize OpenGL/Vulkan. The Null renderer implements the
     * existing PGRAPHRenderer abstraction without touching a host GPU API.
     */
    g_config.display.renderer = CONFIG_DISPLAY_RENDERER_NULL;
    nv2a_context_init();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL initialization failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "xemu - Xbox One M1", 1280, 720,
        SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    atomic_init(&qemu_initialized, false);
    atomic_init(&qemu_finished, false);
    atomic_init(&qemu_exit_status, 0);

    qemu_thread_create(&qemu_thread, "xemu-core",
                       xboxone_qemu_thread, NULL, QEMU_THREAD_JOINABLE);

    bool running = true;
    while (running && !atomic_load_explicit(&qemu_finished, memory_order_acquire)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
            }
        }

        draw_status(renderer,
                    atomic_load_explicit(&qemu_initialized, memory_order_acquire));
        SDL_Delay(16);
    }

    if (!atomic_load_explicit(&qemu_finished, memory_order_acquire)) {
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
    }

    qemu_thread_join(&qemu_thread);

    const int status =
        atomic_load_explicit(&qemu_exit_status, memory_order_acquire);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    xemu_settings_save();
    return status;
}
