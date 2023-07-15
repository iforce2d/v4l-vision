#include <QImage>
#include "vision.h"
#include "quickblob.h"

int visionCount = 0;

visionParams_t visionParams;
blobrun_t br;
blobstore_t* bestblob = NULL;

int init_pixel_stream_hook(void* user_struct, struct stream_state* stream) {
    blobrun_t* br = (blobrun_t*)user_struct;

    if ( visionParams.maskSize > br->image->width() )
        visionParams.maskSize = br->image->width();
    if ( visionParams.maskSize > br->image->height() )
        visionParams.maskSize = br->image->height();

    stream->w = visionParams.maskSize;
    stream->h = visionParams.maskSize;
    return 0;
}

int close_pixel_stream_hook(void* user_struct, struct stream_state* stream) {
    // free up anything you allocated in init_pixel_stream_hook
    return 0;
}

int next_row_hook(void* user_struct, struct stream_state* stream) {
    // load the (grayscale) row at stream->y into the (8 bit) stream->row array
    blobrun_t* br = (blobrun_t*)user_struct;

    //stream->row = br->image->bits() + stream->y * stream->w;

    int startOffsetX = (br->image->width() - visionParams.maskSize) / 2;
    int startOffsetY = (br->image->height() - visionParams.maskSize) / 2;
    stream->row = br->image->bits() + (stream->y+startOffsetY) * br->image->width() + startOffsetX;

    return 0;
}

int next_frame_hook(void* user_struct, struct stream_state* stream) {
    blobrun_t* br = (blobrun_t*)user_struct;
    return br->frame++;
}

void log_blob_hook(void* user_struct, struct blob* b) {
    if ( b->color != visionParams.color || b->size < visionParams.minPixels || b->size > visionParams.maxPixels )
        return;
    int w = b->bb_x2 - b->bb_x1;
    if ( w < visionParams.minWidth || w > visionParams.maxWidth )
        return;
    int h = b->bb_y2 - b->bb_y1;
    if ( h < visionParams.minWidth || h > visionParams.maxWidth )
        return;
    int diff = abs(w - h);
    int maxAspectDiff = ((visionParams.maxWidth + visionParams.minWidth) / 2) / 10;
    if ( diff > maxAspectDiff )
        return;

    float radius = (w+h) / 4.0f;
    int area = 3.14159265359f * radius * radius;
    int areaDiff = abs(area - b->size);
    if ( areaDiff > area / 4 )
        return;

    blobrun_t* br = (blobrun_t*)user_struct;
    int startOffsetX = (br->image->width() - visionParams.maskSize) / 2;
    int startOffsetY = (br->image->height() - visionParams.maskSize) / 2;

    blobstore_t bs;
    bs.size = b->size;
    bs.x = b->center_x + startOffsetX;
    bs.y = b->center_y + startOffsetY;
    bs.bb_x1 = b->bb_x1 + startOffsetX;
    bs.bb_x2 = b->bb_x2 + startOffsetX;
    bs.bb_y1 = b->bb_y1 + startOffsetY;
    bs.bb_y2 = b->bb_y2 + startOffsetY;
    br->blobs.push_back( bs );
}

bool compareBlobSize(blobstore_t& a, blobstore_t& b)
{
    return (a.size > b.size);
}


void initVision() {
    visionParams.color = VISION_DEFAULT_COLOR;
    visionParams.threshold = VISION_DEFAULT_THRESHOLD;
    visionParams.maskSize = VISION_DEFAULT_MASKSIZE;
    visionParams.minPixels = VISION_DEFAULT_MINPIXELS;
    visionParams.maxPixels = VISION_DEFAULT_MAXPIXELS;
    visionParams.minWidth = VISION_DEFAULT_MINWIDTH;
    visionParams.maxWidth = VISION_DEFAULT_MAXWIDTH;
    //visionParams.maxAspectDiff = (visionParams.maxWidth + visionParams.minWidth) / 2 / 10;
    visionParams.saveFile = false;
    visionParams.displayType = VISION_DEFAULT_DISPLAYTYPE;

    visionParams.focus = VISION_DEFAULT_FOCUS;
    visionParams.zoom = VISION_DEFAULT_ZOOM;

    visionParams.overlayElements = 0xFFFFFFFF;
}

void runVision(QImage* image) {

    unsigned char* bytes = image->bits();

    int w = image->width();
    int h = image->height();

    int numBytes = w * h;
    for ( int i = 0; i < numBytes; i++ ) {
        if (bytes[i] > visionParams.threshold)
            bytes[i] = 255;
        else
            bytes[i] = 0;
    }


    br.image = image;
    br.frame = 0;
    br.blobs.clear();
    br.blobs.reserve(100);
    extract_image((void*)&br);

    sort(br.blobs.begin(), br.blobs.end(), compareBlobSize);

//    if ( visionCount++ % 10 == 0 ) {
//        image->save("threshold.png");
//    }


    br.bestblob = NULL;

    int bestDelta = 480*480;

    int hx = w / 2;
    int hy = h / 2;

    std::vector<blobstore_t>::iterator it = br.blobs.begin();
    while ( it != br.blobs.end() ) {
        blobstore_t& bs = *it;

        float dx = bs.x - hx;
        float dy = bs.y - hy;
        int delta = dx*dx + dy*dy;
        if ( /*delta < 200*200 &&*/ delta < bestDelta ) {
            bestDelta = delta;
            br.bestblob = &bs;
            br.bbdx = dx;
            br.bbdy = dy;
        }

        it++;
    }

}
