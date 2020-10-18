#pragma warning(disable : 4996)
// libSDL and libVLC sample code.
// License: [http://en.wikipedia.org/wiki/WTFPL WTFPL]

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include "SDL.h"
#include "SDL_mutex.h"

#include "vlc/vlc.h"

#define WIDTH 640
#define HEIGHT 480

#define VIDEOWIDTH 320
#define VIDEOHEIGHT 240

struct context {
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_mutex* mutex;
    int n;
};

// VLC prepares to render a video frame.
static void* lock(void* data, void** p_pixels) {

    struct context* c = (context*)data;

    int pitch;
    SDL_LockMutex(c->mutex);
    SDL_LockTexture(c->texture, NULL, p_pixels, &pitch);

    return NULL; // Picture identifier, not needed here.
}

// VLC just rendered a video frame.
static void unlock(void* data, void* id, void* const* p_pixels) {

    struct context* c = (context*)data;

    uint16_t* pixels = (uint16_t*)*p_pixels;

    // We can also render stuff.
    int x, y;
    for (y = 10; y < 80; y++) {
        for (x = 10; x < 40; x++) {
            if (x < 13 || y < 13 || x > 36 || y > 36) {
                pixels[y * VIDEOWIDTH + x] = 0xffff;
            }
            else {
                // RV16 = 5+6+5 pixels per color, BGR.
                pixels[y * VIDEOWIDTH + x] = 0x02ff;
            }
        }
    }

    SDL_UnlockTexture(c->texture);
    SDL_UnlockMutex(c->mutex);
}

// VLC wants to display a video frame.
static void display(void* data, void* id) {

    struct context* c = (context*)data;

    SDL_Texture* texture;
    texture = SDL_CreateTexture(c->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, VIDEOWIDTH, VIDEOHEIGHT);


    SDL_Rect rect;
    rect.w = VIDEOWIDTH;
    rect.h = VIDEOHEIGHT;
    rect.x = (int)((1. + .5 * sin(0.03 * c->n)) * (WIDTH - VIDEOWIDTH) / 2);
    rect.y = (int)((1. + .5 * cos(0.03 * c->n)) * (HEIGHT - VIDEOHEIGHT) / 2);
    /*rect.x = 0;
    rect.y = 0;*/

    SDL_SetRenderDrawColor(c->renderer, 0, 80, 0, 255);
    SDL_RenderClear(c->renderer);
    SDL_RenderCopy(c->renderer, c->texture, NULL, NULL); // &rect

    /*SDL_SetRenderTarget(c->renderer, texture);
    SDL_SetRenderDrawColor(c->renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(c->renderer);*/
    //SDL_RenderDrawRect(c->renderer, &rect);
    //SDL_SetRenderDrawColor(c->renderer, 0xFF, 0x00, 0x00, 0x00);
    //SDL_RenderFillRect(c->renderer, &rect);
    //SDL_SetRenderTarget(c->renderer, NULL);
    //SDL_RenderCopy(c->renderer, texture, NULL, &rect);


    SDL_RenderPresent(c->renderer);
}

static void quit(int c) {
    SDL_Quit();
    exit(c);
}

static int TestThread(void* ptr) {
    int cnt;

    for (cnt = 0; cnt < 1000; ++cnt) {
        printf("Thread counter: %d\n", cnt);
        SDL_Delay(1000);
    }

    return cnt;
}

int main(int argc, char* argv[]) {

    libvlc_instance_t* libvlc;
    libvlc_media_t* m;
    libvlc_media_player_t* mp;
    char const* vlc_argv[] = {

        //"--no-audio", // Don't play audio.
        "--no-xlib", // Don't use Xlib.

        // Apply a video filter.
        //"--video-filter", "sepia",
        //"--sepia-intensity=200"
    };
    int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);

    SDL_Thread* thread;
    int threadReturnValue;

    SDL_Event event;
    int done = 0, action = 0, pause = 0, n = 0;

    struct context context;

    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Initialise libSDL.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    int num = SDL_GetNumRenderDrivers();
    if (num < 0) {
        SDL_Log("%s", SDL_GetError());
        return 1;
    } else 
        SDL_Log("NumRenderDrivers %d\n", num);

    SDL_RendererInfo info;
    for (size_t i = 0; i < num; i++)
    {
        SDL_GetRenderDriverInfo(i, &info);
        SDL_Log("NumRenderDrivers %d %s %d %d\n", i, info.name, info.max_texture_width, info.max_texture_height);
    } 

    const SDL_MessageBoxButtonData buttons[] = {
    { /* .flags, .buttonid, .text */        0, 0, "no" },
    { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "yes" },
    { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
    };
    const SDL_MessageBoxColorScheme colorScheme = {
        { /* .colors (.r, .g, .b) */
            /* [SDL_MESSAGEBOX_COLOR_BACKGROUND] */
            { 255,   0,   0 },
            /* [SDL_MESSAGEBOX_COLOR_TEXT] */
            {   0, 255,   0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] */
            { 255, 255,   0 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] */
            {   0,   0, 255 },
            /* [SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] */
            { 255,   0, 255 }
        }
    };
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_INFORMATION, /* .flags */
        NULL, /* .window */
        "example message box", /* .title */
        "select a button", /* .message */
        SDL_arraysize(buttons), /* .numbuttons */
        buttons, /* .buttons */
        &colorScheme /* .colorScheme */
    };
    int buttonid;
    if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
        SDL_Log("error displaying message box");
        return 1;
    }
    if (buttonid == -1) {
        SDL_Log("no selection");
    }
    else {
        SDL_Log("selection was %s", buttons[buttonid].text);
    }

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
        "Missing file",
        "File is missing. Please reinstall the program.",
        NULL);

    printf("Simple SDL_CreateThread test:\n");

    /* Simply create a thread */
    thread = SDL_CreateThread(TestThread, "TestThread", (void*)NULL);

    if (NULL == thread) {
        printf("SDL_CreateThread failed: %s\n", SDL_GetError());
    }
    else {
        //SDL_WaitThread(thread, &threadReturnValue);
        //printf("Thread returned value: %d\n", threadReturnValue);
    }


    // Create SDL graphics objects.
    SDL_Window* window = SDL_CreateWindow(
        "Fartplayer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
        quit(3);
    }

    context.renderer = SDL_CreateRenderer(window, -1, 0);
    if (!context.renderer) {
        fprintf(stderr, "Couldn't create renderer: %s\n", SDL_GetError());
        quit(4);
    }

    context.texture = SDL_CreateTexture(
        context.renderer,
        SDL_PIXELFORMAT_BGR565, SDL_TEXTUREACCESS_STREAMING,
        VIDEOWIDTH, VIDEOHEIGHT);
    if (!context.texture) {
        fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());
        quit(5);
    }

    context.mutex = SDL_CreateMutex();

    // If you don't have this variable set you must have plugins directory
    // with the executable or libvlc_new() will not work!
    printf("VLC_PLUGIN_PATH=%s\n", getenv("VLC_PLUGIN_PATH"));

    // Initialise libVLC.
    libvlc = libvlc_new(vlc_argc, vlc_argv);
    if (NULL == libvlc) {
        printf("LibVLC initialization failure.\n");
        return EXIT_FAILURE;
    }

    m = libvlc_media_new_path(libvlc, argv[1]);
    mp = libvlc_media_player_new_from_media(m);
    libvlc_media_release(m);

    libvlc_video_set_callbacks(mp, lock, unlock, display, &context);
    libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * 2);
    libvlc_media_player_play(mp);

    // Main loop.
    while (!done) {

        action = 0;

        // Keys: enter (fullscreen), space (pause), escape (quit).
        while (SDL_PollEvent(&event)) {

            switch (event.type) {
            case SDL_QUIT:
                done = 1;
                break;
            case SDL_KEYDOWN:
                action = event.key.keysym.sym;
                break;
            }
        }

        switch (action) {
        case SDLK_ESCAPE:
        case SDLK_q:
            done = 1;
            break;
        case ' ':
            printf("Pause toggle.\n");
            pause = !pause;
            break;
        }

        if (!pause) { context.n++; }

        SDL_Delay(1000 / 10);
    }

    // Stop stream and clean up libVLC.
    libvlc_media_player_stop(mp);
    libvlc_media_player_release(mp);
    libvlc_release(libvlc);

    // Close window and clean up libSDL.
    SDL_DestroyMutex(context.mutex);
    SDL_DestroyRenderer(context.renderer);

    quit(0);

    return 0;
}