#ifndef SDL_IMAGE_AS_H_
#define SDL_IMAGE_AS_H_

#include "gamepad-image.h"
#include <SDL2/SDL.h>

SDL_Surface *image_as_surface(uint32_t *image)
{
    SDL_Surface* image_surface =
        SDL_CreateRGBSurfaceFrom(
            image,
            IMAGE_WIDTH,
            IMAGE_HEIGHT,
            32,
            IMAGE_WIDTH * 4,
            0x000000FF,
            0x0000FF00,
            0x00FF0000,
            0xFF000000);
    return image_surface;
}

SDL_Texture *image_as_texture(SDL_Renderer *renderer, uint32_t *image) /* ,SDL_Color color_key) */
{
    SDL_Surface *image_surface = image_as_surface(image);
    SDL_Texture *image_texture = SDL_CreateTextureFromSurface(renderer, image_surface);
    SDL_FreeSurface(image_surface);
    return image_texture;
}

#endif
