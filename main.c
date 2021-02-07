#include "IT8951.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define TRANSFER_READ_SIZE 4096 * 4

uint8_t *buffer_to_write = NULL;
int target_screen_width = 1872;
int target_screen_height = 1404;
int should_revert = 0;

void abort_(const char *s) {
    printf("%s\n", s);
    abort();
}


void start_board() {
    if (!(buffer_to_write = IT8951_Init(target_screen_width, target_screen_height, should_revert))) {
        printf("IT8951_Init error, exiting\n");
        exit(1);
    } else {
        printf("IT8951 started\n");
    }
}

void stop_board() {
    buffer_to_write = NULL;
    IT8951_Cancel();
    printf("Board is now stopped\n");
}

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t displayMode;
} Rectangle;

uint8_t preamble[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

typedef struct {
    uint8_t shouldStop;
    uint16_t width;
    uint16_t height;
    uint8_t numOfRectangles;
    Rectangle *rectangles;
} Image;

int readImage(FILE *file, Image *image, uint8_t *hostFrameBuf, uint16_t maxWidth, uint16_t maxHeight) {
    uint8_t filePreamble[8];
    size_t read = fread(&filePreamble, 1, 8, file);
    if (read != 8) {
        printf("file too short\n");
        return 1;
    }
    int equals = memcmp(&preamble, &filePreamble, 8);
    if (equals != 0) {
        printf("wrong preamble\n");
        return 1;
    }
    uint8_t imageheader[6];
    read = fread(&imageheader, 1, 6, file);
    if (read != 6) {
        printf("content too short\n");
        return 1;
    }
    image->shouldStop = imageheader[0];
    if (image->shouldStop) {
        return 2;
    }
    image->width = (imageheader[1] << 8) + imageheader[2];
    image->height = (imageheader[3] << 8) + imageheader[4];
    image->numOfRectangles = imageheader[5];
    if ((image->width != maxWidth) || (image->height != maxHeight) || (image->numOfRectangles > 10)) {
        printf("image size wrong, w = %u, h = %u, nor = %u\n", (int) image->width, (int) image->height,
               (int) image->numOfRectangles);
        return 1;
    }
    int rectangleBufSize = 9 * image->numOfRectangles;
    uint8_t* recBuf = malloc(rectangleBufSize);
    read = fread(recBuf, 1, rectangleBufSize, file);
    if (read != rectangleBufSize) {
        printf("cannot read rectangles\n");
        return 1;
    }
    image->rectangles = malloc(sizeof (Rectangle) * image->numOfRectangles);
    int index = 0;
    for(int i = 0; i < image->numOfRectangles; i++) {
        image->rectangles[i].x = (recBuf[index] << 8) + recBuf[index+1];
        image->rectangles[i].y= (recBuf[index+2] << 8) + recBuf[index+3];
        image->rectangles[i].w = (recBuf[index+4] << 8) + recBuf[index+5];
        image->rectangles[i].h = (recBuf[index+6] << 8) + recBuf[index+7];
        image->rectangles[i].displayMode = recBuf[index+8];
        index += 9;
    }
    free(recBuf);
    uint32_t bufSize = image->width * image->height / 2;
    read = fread(hostFrameBuf, 1, bufSize, file);
    if (read != bufSize) {
        printf("error reading image data, read = %i, bufSize = %u\n", read, bufSize);
        return 1;
    }
    uint32_t nonWhiteCount = 0;
    for(int i = 0; i < bufSize; i++) {
        if (hostFrameBuf[i] != 0xff) {
            nonWhiteCount++;
        }
    }
    printf("nonWhiteCount = %u\n", nonWhiteCount);
    //memset(hostFrameBuf, 0xff, bufSize);
    return 0;
}

void free_image_data(Image *image) {
    free(image->rectangles);
    image->rectangles = NULL;
}

void displayRectangles(Image *image) {
    IT8951WaitForDisplayReady();
    IT8951HostAreaPackedPixelWrite(&stLdImgInfo);
    for (int i = 0; i < image->numOfRectangles; i++) {
        printf("displaying rectangle %i, w = %u, h = %u, w = %u, h = %u, displayMode = %i\n", i,
               image->rectangles[i].x,
               image->rectangles[i].y,
               image->rectangles[i].w,
               image->rectangles[i].h,
               image->rectangles[i].displayMode
        );
        IT8951WaitForDisplayReady();
        IT8951DisplayArea(
                image->rectangles[i].x,
                image->rectangles[i].y,
                image->rectangles[i].w,
                image->rectangles[i].h,
                image->rectangles[i].displayMode
        );
    }
}

int main(int argc, char *argv[]) {
    start_board();
    Image image;
    printf("Listening for stdin...\n");
    while (1) {
        int res = readImage(stdin, &image, buffer_to_write, gstI80DevInfo.usPanelW, gstI80DevInfo.usPanelH);
        if (res == 2) {
            break;
        } else if (res == 0) {
            displayRectangles(&image);
        } else if (res == 1) {
            printf("error reading or displaying image\n");
            break;
        }
        free_image_data(&image);
    }
    stop_board();
}


