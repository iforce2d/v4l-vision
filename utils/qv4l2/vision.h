#ifndef VISION_H
#define VISION_H

#include <QImage>

#define VISION_DEFAULT_COLOR            0
#define VISION_DEFAULT_THRESHOLD        64
#define VISION_DEFAULT_MASKSIZE         240
#define VISION_DEFAULT_MINPIXELS        450
#define VISION_DEFAULT_MAXPIXELS        900
#define VISION_DEFAULT_MINWIDTH         25
#define VISION_DEFAULT_MAXWIDTH         40
//#define VISION_DEFAULT_MAXASPECTDIFF    6
#define VISION_DEFAULT_DISPLAYTYPE      VDT_COLOR

// logitech B910
#define VISION_DEFAULT_FOCUS            136
#define VISION_DEFAULT_ZOOM             1

#define VISION_CTRL_FOCUS_ID            10094858
#define VISION_CTRL_ZOOM_ID             10094861

#define VOE_CROSSHAIR        1
#define VOE_MASKAREA         2
#define VOE_BLOBS            4
#define VOE_BESTBLOB         8
#define VOE_BESTBLOB_TEXT    16
#define VOE_ANYBLOB_TEXT     32

enum visionDisplayType {
    VDT_COLOR,
    VDT_GREY,
    VDT_THRESHOLD,
};

typedef struct {
    uint8_t color;
    uint8_t threshold;
    int maskSize;
    int minPixels;
    int maxPixels;
    int minWidth;
    int maxWidth;
    //int maxAspectDiff;
    bool saveFile;
    int displayType;
    int focus;
    int zoom;
    uint32_t overlayElements;

} visionParams_t;

typedef struct {
    int size;
    float x;
    float y;
    int bb_x1, bb_y1, bb_x2, bb_y2;
} blobstore_t;

typedef struct {
    QImage* image;
    int frame;
    std::vector<blobstore_t> blobs;
    blobstore_t* bestblob;
    float bbdx;
    float bbdy;
} blobrun_t;

extern blobrun_t br;
extern visionParams_t visionParams;

void initVision();
void runVision(QImage* image);

#endif // VISION_H
