#ifndef PAINT_BUFFER_H
#define PAINT_BUFFER_H

#include <windows.h>

// Reuse the backing bitmap between paints and release every owned GDI handle.
class PaintBuffer {
public:
    PaintBuffer() = default;
    PaintBuffer(const PaintBuffer&) = delete;
    PaintBuffer& operator=(const PaintBuffer&) = delete;
    ~PaintBuffer() { Reset(); }

    HDC Get(HDC target, const RECT& bounds) {
        int width = bounds.right - bounds.left;
        int height = bounds.bottom - bounds.top;
        if (width <= 0 || height <= 0) return target;
        if (dc && width == bitmapWidth && height == bitmapHeight) return dc;

        Reset();
        dc = CreateCompatibleDC(target);
        if (dc) bitmap = CreateCompatibleBitmap(target, width, height);
        if (!bitmap) {
            Reset();
            return target;
        }
        originalBitmap = SelectObject(dc, bitmap);
        bitmapWidth = width;
        bitmapHeight = height;
        return dc;
    }

    void Present(HDC target, const RECT& dirty) const {
        if (dc && bitmap) {
            BitBlt(target, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top,
                dc, dirty.left, dirty.top, SRCCOPY);
        }
    }

    void Reset() {
        if (dc && originalBitmap) SelectObject(dc, originalBitmap);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = NULL;
        bitmap = NULL;
        originalBitmap = NULL;
        bitmapWidth = bitmapHeight = 0;
    }

private:
    HDC dc = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ originalBitmap = NULL;
    int bitmapWidth = 0, bitmapHeight = 0;
};

#endif // PAINT_BUFFER_H
