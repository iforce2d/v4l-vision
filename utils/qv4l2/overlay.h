#ifndef OVERLAY_H
#define OVERLAY_H

#include <QImage>
#include <QPaintEvent>

class Overlay : public QImage
{
    //Q_OBJECT
public:
    Overlay(uchar *data, int width, int height, int bytesPerLine, Format format);

    void vision();

signals:

public slots:
};

#endif // OVERLAY_H
