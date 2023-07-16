#include "EPD_IT8951.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

IT8951_Dev_Info Dev_Info;
IT8951_Load_Img_Info stLdImgInfo;
#define TRANSFER_READ_SIZE 4096 * 4

uint32_t Init_Target_Memory_Addr;
int target_screen_width = 1872;
int target_screen_height = 1404;
int should_revert = 0;
bool Four_Byte_Align;
IT8951_Area_Img_Info area_info;

void abort_(const char *s)
{
    printf("%s\n", s);
    abort();
}

bool start_board()
{
    if (DEV_Module_Init() != 0)
    {
        printf("DEV_Module_Init failed\n");
        return false;
    }
    Dev_Info = EPD_IT8951_Init(1520); // VCOM = -1.52
    // get some important info from Dev_Info structure
    printf("Panel Width = %d, panel height = %d\n", Dev_Info.Panel_W, Dev_Info.Panel_H);
    Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);
    char *LUT_Version = (char *)Dev_Info.LUT_Version;
    if (strcmp(LUT_Version, "M641") == 0)
    {
        // 6inch e-Paper HAT(800,600), 6inch HD e-Paper HAT(1448,1072), 6inch HD touch e-Paper HAT(1448,1072)
        A2_Mode = 4;
        Four_Byte_Align = true;
    }
    else if (strcmp(LUT_Version, "M841_TFAB512") == 0)
    {
        // Another firmware version for 6inch HD e-Paper HAT(1448,1072), 6inch HD touch e-Paper HAT(1448,1072)
        A2_Mode = 6;
        Four_Byte_Align = true;
    }
    else if (strcmp(LUT_Version, "M841") == 0)
    {
        // 9.7inch e-Paper HAT(1200,825)
        A2_Mode = 6;
    }
    else if (strcmp(LUT_Version, "M841_TFA2812") == 0)
    {
        // 7.8inch e-Paper HAT(1872,1404)
        A2_Mode = 6;
    }
    else if (strcmp(LUT_Version, "M841_TFA5210") == 0)
    {
        // 10.3inch e-Paper HAT(1872,1404)
        A2_Mode = 6;
    }
    else
    {
        // default set to 6 as A2 Mode
        A2_Mode = 6;
    }
    Debug("A2 Mode:%d\r\n", A2_Mode);

    EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, INIT_Mode);
    area_info.Area_X = 0;
    area_info.Area_Y = 0;
    area_info.Area_W = Dev_Info.Panel_W;
    area_info.Area_H = Dev_Info.Panel_W;
}

void stop_board()
{
    Init_Target_Memory_Addr = NULL;
    // EPD_IT8951_Cancel();
    printf("Board is now stopped\n");
}

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t displayMode;
} Rectangle;

uint8_t preamble[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

typedef struct
{
    uint8_t shouldStop;
    uint16_t startX;
    uint16_t startY;
    uint16_t width;
    uint16_t height;
} Image;

int readImage(FILE *file, Image *image, uint8_t **imageBufPtr, uint16_t maxWidth, uint16_t maxHeight)
{
    uint8_t filePreamble[8];
    size_t read = fread(&filePreamble, 1, 8, file);
    if (read != 8)
    {
        printf("file too short\n");
        return 1;
    }
    int equals = memcmp(&preamble, &filePreamble, 8);
    if (equals != 0)
    {
        printf("wrong preamble\n");
        return 1;
    }
    // 0 - exit signal
    // 1 - width high
    // 2 - width low
    // 3 - height high
    // 4 - height low
    // 5 - startX high
    // 6 - startX low
    // 7 - startY high
    // 8 - startY low
    uint8_t imageheader[9];
    read = fread(&imageheader, 1, 9, file);
    if (read != 9)
    {
        printf("content too short\n");
        return 1;
    }
    image->shouldStop = imageheader[0];
    if (image->shouldStop)
    {
        return 2;
    }
    image->startX = (imageheader[5] << 8) + imageheader[6];
    image->startY = (imageheader[7] << 8) + imageheader[8];
    image->width = (imageheader[1] << 8) + imageheader[2];
    image->height = (imageheader[3] << 8) + imageheader[4];
    if ((image->startX + image->width > maxWidth) ||
        (image->startY + image->height > maxHeight))
    {
        printf("image size wrong, x = %u, y = %u, w = %u, h = %u\n",
               (int)image->startX, (int)image->startY, (int)image->width, (int)image->height);
        return 1;
    }
    uint32_t bufSize = image->width * image->height / 2;
    uint8_t *imageBuf = malloc(bufSize);
    *imageBufPtr = imageBuf;
    read = fread(imageBuf, 1, bufSize, file);
    if (read != bufSize)
    {
        printf("error reading image data, read = %zu, bufSize = %u\n", read, bufSize);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    start_board();
    uint32_t targetMemory = (((uint32_t)Dev_Info.Memory_Addr_H) << 16) + Dev_Info.Memory_Addr_L;
    Image image;
    uint8_t *imageBuf;
    printf("Listening for stdin...\n");
    while (1)
    {
        int res = readImage(stdin, &image, &imageBuf, Dev_Info.Panel_W, Dev_Info.Panel_H);
        if (res == 2)
        {
            break;
        }
        else if (res == 0)
        {
            EPD_IT8951_4bp_Refresh(imageBuf, image.startX, image.startY, image.width, image.height, true, Init_Target_Memory_Addr, true);
        }
        else if (res == 1)
        {
            printf("error reading or displaying image\n");
            break;
        }
        free(imageBuf);
    }
    stop_board();
}
