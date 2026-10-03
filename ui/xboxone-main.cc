/*
 * Xbox One UWP entrypoint support.
 *
 * SDL3's WinRT backend supplies the CoreApplication/IFrameworkView entrypoint.
 * This translation unit must be compiled as C++ so SDL can initialize WinRT.
 */

#include <SDL3/SDL_main.h>
