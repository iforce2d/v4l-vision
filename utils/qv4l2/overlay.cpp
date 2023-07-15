#include "overlay.h"
#include <QPainter>
#include "vision.h"

Overlay::Overlay(uchar *data, int width, int height, int bytesPerLine, Format format) : QImage(data, width, height, bytesPerLine, format)
{
}

void Overlay::vision()
{
    int hx = width() / 2;
    int hy = height() / 2;

    QPainter p;
    p.begin(this);

    if ( visionParams.overlayElements & VOE_BLOBS ) {

        p.setFont( QFont("Arial", 12) );
        p.setPen(QPen(Qt::yellow,3));

        std::vector<blobstore_t>::iterator it = br.blobs.begin();
        while ( it != br.blobs.end() ) {
            blobstore_t& bs = *it;
            int w = bs.bb_x2 - bs.bb_x1;
            int h = bs.bb_y2 - bs.bb_y1;
            p.drawEllipse(bs.bb_x1, bs.bb_y1, w, h);

            if ( visionParams.overlayElements & VOE_ANYBLOB_TEXT ) {
                p.fillRect(bs.bb_x2, bs.bb_y2-16,60,20,Qt::black);
                QString s;
                s.sprintf("%d,%d", bs.size, w);
                p.drawText(bs.bb_x2, bs.bb_y2, s);
            }

            it++;
        }
    }

    if ( br.bestblob ) {
        p.setPen(QPen(Qt::magenta,3));

        if ( visionParams.overlayElements & VOE_BESTBLOB ) {
            int w = br.bestblob->bb_x2 - br.bestblob->bb_x1;
            int h = br.bestblob->bb_y2 - br.bestblob->bb_y1;
            p.drawEllipse(br.bestblob->bb_x1, br.bestblob->bb_y1, w, h);
        }

        if ( visionParams.overlayElements & VOE_BESTBLOB_TEXT ) {
            p.setFont( QFont("Arial", 18) );
            p.fillRect(0,0,90,30,Qt::black);
            QString s;
            s.sprintf("%d,%d", (int)br.bbdx, (int)br.bbdy);
            p.drawText(8, 22, s);
        }
    }

    if ( visionParams.overlayElements & VOE_MASKAREA ) {
        int startOffsetX = (width() - visionParams.maskSize) / 2;
        int startOffsetY = (height() - visionParams.maskSize) / 2;
        p.setPen(Qt::green);
        p.drawRect(startOffsetX, startOffsetY, visionParams.maskSize, visionParams.maskSize);
    }

    if ( visionParams.overlayElements & VOE_CROSSHAIR ) {
        p.setPen(Qt::red);
        p.drawLine(0, hy, width(), hy);
        p.drawLine(hx, 0, hx, height());
    }

    p.end ();
}
