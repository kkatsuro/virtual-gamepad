#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

// modified version of tsoding image2c, it works on multiple pictures with the same size

const char *shift(int *argc, char ***argv)
{
    if (*argc <= 0) {
        return NULL;
    }
    const char *result = *argv[0];
    *argc -= 1;
    *argv += 1;
    return result;
}

// returns path without slashes and extension
char *extract_filename(char *path) {
    char filename[4096];
    strncpy(filename, path, 4096);

    char *cursor = filename;
    char *slash = cursor;
    char *dot = cursor;
    while (*cursor) {
        if (*cursor == '/') { slash = cursor; }
        if (*cursor == '.') {   dot = cursor; }
        if (*cursor == '-') {  *cursor = '_'; }
        cursor++;
    }

    if (cursor - dot <= 4) {
        *dot = '\0';
    }
    
    if (slash != filename) {
        return slash + 1;
    }
    return filename;
}

void print_picture(char *filename, uint32_t *data, int x, int y) {
    printf("uint32_t %s[] = {", filename);
    for (size_t i = 0; i < (size_t)(x * y); ++i) {
        printf("0x%x, ", data[i]);
    }
    printf("};\n");
}

int main(int argc, char *argv[])
{
    shift(&argc, &argv);        // skip program name

    if (argc <= 0) {
        fprintf(stderr, "Usage: image2c <filepath> [filepath2...]\n");
        fprintf(stderr, "ERROR: expected file path\n");
        exit(1);
    }

    char *filepath = shift(&argc, &argv);

    int x, y, n;
    uint32_t *data = (uint32_t *)stbi_load(filepath, &x, &y, &n, 4);

    if (data == NULL) {
        fprintf(stderr, "Could not load file `%s`\n", filepath);
        exit(1);
    }

    printf("#ifndef IMAGE_H_\n");
    printf("#define IMAGE_H_\n");
    printf("#define IMAGE_WIDTH %d\n", x);
    printf("#define IMAGE_HEIGHT %d\n", y);
    print_picture(extract_filename(filepath), data, x, y);

    while ((filepath = shift(&argc, &argv)) != NULL) {
        // TODO: iterate over list of files to check length, then use appropriate padding setting
        fprintf(stderr, "file: %-30snamed: %s\n", filepath, extract_filename(filepath));
        data = (uint32_t *)stbi_load(filepath, &x, &y, &n, 4);

        if (data == NULL) {
            fprintf(stderr, "Could not load file `%s`\n", filepath);
            exit(1);
        }
        print_picture(extract_filename(filepath), data, x, y);
    }
    
    printf("#endif // IMAGE_H_\n");

    return 0;
}
