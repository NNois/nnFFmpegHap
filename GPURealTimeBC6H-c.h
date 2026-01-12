#ifndef GPU_REALTIME_BC6H_C_H
#define GPU_REALTIME_BC6H_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPURealTimeBC6H_Image {
    uint32_t width;
    uint32_t height;
    void *data;
    size_t dataSize;
} GPURealTimeBC6H_Image;

enum {
    GPURealTimeBC6H_ImageFormat_RGBA32F = 0,
};

enum {
    GPURealTimeBC6H_Preset_Speed = 0,
    GPURealTimeBC6H_Preset_Quality = 1,
};

void GPURealTimeBC6H_Initialize(uint32_t preset);
void GPURealTimeBC6H_Release(void);
bool GPURealTimeBC6H_Compress(const GPURealTimeBC6H_Image *srcImage,
                              uint32_t srcFormat,
                              GPURealTimeBC6H_Image *dstImage);
void GPURealTimeBC6H_FreeImage(GPURealTimeBC6H_Image *image);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GPU_REALTIME_BC6H_C_H */
