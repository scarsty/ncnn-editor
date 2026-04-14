#include "node_editor.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#ifdef USE_OPENGL3
#include <imgui_impl_opengl3.h>
#else
#include <imgui_impl_sdlrenderer3.h>
#endif

#include <imnodes.h>
#include <stdio.h>
#include <SDL3/SDL.h>

#ifdef USE_OPENGL3
#include <SDL3/SDL_opengl.h>
#endif

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

#ifdef USE_OPENGL3
#ifdef __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS,
        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#endif
    SDL_WindowFlags flag = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef USE_OPENGL3
    flag |= SDL_WINDOW_OPENGL;
#endif

    SDL_Window* window = SDL_CreateWindow("Neural Net Editor", 1280, 720, flag);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

#ifdef USE_OPENGL3
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == NULL)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return -1;
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

#ifdef USE_OPENGL3
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
#else
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
#endif

    ImNodes::CreateContext();

    // Setup style
    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    auto& io = ImGui::GetIO();
#ifdef _WIN32
    io.Fonts->AddFontFromFileTTF("c:/windows/fonts/msyh.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
#else
#ifdef __APPLE__
    io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Songti.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
#else
    io.Fonts->AddFontFromFileTTF("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
#endif
#endif
    bool done = false;
    bool initialized = false;


    const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    {
        //glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    }

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                example::NodeEditorSetExit(1);;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                example::NodeEditorSetExit(1);;
        }

        // Start the Dear ImGui frame
#ifdef USE_OPENGL3
        ImGui_ImplOpenGL3_NewFrame();
#else
        ImGui_ImplSDLRenderer3_NewFrame();
#endif
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (!initialized)
        {
            initialized = true;
            example::NodeEditorInitialize(argc, argv);
        }

        example::NodeEditorShow();

        // Rendering
        ImGui::Render();

#ifdef USE_OPENGL3
        int fb_width, fb_height;
    SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
#else
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
#endif
    }

    example::NodeEditorShutdown();
    ImNodes::DestroyContext();
#ifdef USE_OPENGL3
    ImGui_ImplOpenGL3_Shutdown();
#else
    ImGui_ImplSDLRenderer3_Shutdown();
#endif
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#ifdef USE_OPENGL3
    SDL_GL_DestroyContext(gl_context);
#else
    SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
