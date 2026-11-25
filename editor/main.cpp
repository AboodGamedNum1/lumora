// Lumora Editor, M0 skeleton: an empty window and a headless Dear ImGui frame loop.
// The real editor (docking shell, viewport, inspector) is milestone M7a.
#include <SDL3/SDL.h>
#include <imgui.h>

#include <cstdio>
#include <cstring>

#include "lumora/version.h"

int main(int argc, char** argv) {
  bool smoke = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--smoke") == 0) smoke = true;
  }

  std::printf("%s (%s)\n", lumora::version_string().c_str(), lumora::toolchain_string().c_str());

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    // Headless CI machines may have no display, fall back to the dummy driver.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
      std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return 1;
    }
  }
  std::printf("video driver: %s\n", SDL_GetCurrentVideoDriver());

  SDL_Window* window = SDL_CreateWindow("Lumora Editor", 1280, 720,
                                        smoke ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.IniFilename = nullptr;
  unsigned char* pixels = nullptr;
  int fw = 0, fh = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &fw, &fh);

  int frames = 0;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) running = false;
    }
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::Begin("Lumora");
    ImGui::TextUnformatted("Empty editor window (M0 skeleton)");
    ImGui::End();
    ImGui::Render();
    ++frames;
    if (smoke && frames >= 5) running = false;
    if (!smoke) SDL_Delay(16);
  }

  ImGui::DestroyContext();
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::printf("editor smoke ok, frames=%d\n", frames);
  return 0;
}
