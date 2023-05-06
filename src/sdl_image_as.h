#ifndef SDL_IMAGE_AS_H_
#define SDL_IMAGE_AS_H_

#include <SDL2/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

SDL_Surface *image_as_surface(uint32_t *image, int width, int height) {
    SDL_Surface* image_surface =
        SDL_CreateRGBSurfaceFrom(
            image,
            width,
            height,
            32,
            width * 4,
            0x000000FF,
            0x0000FF00,
            0x00FF0000,
            0xFF000000);
    return image_surface;
}

SDL_Texture *image_as_texture(SDL_Renderer *renderer, uint32_t *image, int width, int height) {
    SDL_Surface *image_surface = image_as_surface(image, width, height);
    SDL_Texture *image_texture = SDL_CreateTextureFromSurface(renderer, image_surface);
    SDL_FreeSurface(image_surface);
    return image_texture;
}

SDL_Texture *file_as_texture(SDL_Renderer *renderer, char *filename) {
    int w, h;
    uint32_t *image = (uint32_t *)stbi_load(filename, &w, &h, NULL, 4);
    SDL_Texture *texture = image_as_texture(renderer, image, w, h);
    stbi_image_free(image);
    return texture;
}

#endif
