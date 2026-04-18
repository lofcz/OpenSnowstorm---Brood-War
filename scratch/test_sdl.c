#include <SDL.h>
#include <stdio.h>
int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized!\n");
    SDL_Quit();
    return 0;
}
