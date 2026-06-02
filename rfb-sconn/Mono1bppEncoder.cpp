// Copyright (C) 2009,2010,2011,2012 GlavSoft LLC.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//-------------------------------------------------------------------------
//

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "Mono1bppEncoder.h"
#include "rfb/EncodingDefs.h"

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Mono1bppEncoder::Mono1bppEncoder(PixelConverter *conv,
                                 DataOutputStream *output)
: Encoder(conv, output)
{
}

Mono1bppEncoder::~Mono1bppEncoder()
{
}

// ---------------------------------------------------------------------------
// getCode
// ---------------------------------------------------------------------------

int Mono1bppEncoder::getCode() const
{
  return EncodingDefs::MONO1BPP;
}

// ---------------------------------------------------------------------------
// pixelLuma
//
// Extracts 8-bit luminance from a single pixel using BT.601 integer weights:
//   Y = (77*R + 150*G + 29*B) / 256
// The pixel pointer `p` points to the start of the pixel in the converted
// framebuffer; `bpp` is bytes-per-pixel (1, 2, or 4).
// ---------------------------------------------------------------------------

/*static*/ int Mono1bppEncoder::pixelLuma(const unsigned char *p, int bpp,
                                          const PixelFormat &pf)
{
  unsigned int pixel = 0;

  // Assemble pixel value from bytes (little-endian, as used by TightVNC).
  switch (bpp) {
  case 1:
    pixel = p[0];
    break;
  case 2:
    pixel = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
    break;
  default: // 4
    pixel = (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
            ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
    break;
  }

  int r = (int)((pixel >> pf.redShift)   & pf.redMax);
  int g = (int)((pixel >> pf.greenShift) & pf.greenMax);
  int b = (int)((pixel >> pf.blueShift)  & pf.blueMax);

  // Scale channels to 0-255.
  if (pf.redMax   != 255) r = (r * 255) / (int)pf.redMax;
  if (pf.greenMax != 255) g = (g * 255) / (int)pf.greenMax;
  if (pf.blueMax  != 255) b = (b * 255) / (int)pf.blueMax;

  return (77 * r + 150 * g + 29 * b) >> 8;
}

// ---------------------------------------------------------------------------
// fsDitherAndPack
//
// Floyd-Steinberg dithers the rectangle `rect` in the already-converted
// framebuffer `fb` and writes the packed 1-bpp result to `out`.
//
// Error diffusion kernel (fractions of 1):
//   right        7/16
//   lower-left   3/16
//   below        5/16
//   lower-right  1/16
//
// `out` must be at least ceil(w/8)*h bytes. Returns false on OOM.
// ---------------------------------------------------------------------------

/*static*/ bool Mono1bppEncoder::fsDitherAndPack(const FrameBuffer *fb,
                                                  const Rect *rect,
                                                  unsigned char *out)
{
  int w = rect->getWidth();
  int h = rect->getHeight();
  int rowBytes = (w + 7) >> 3;  // ceil(w/8)

  PixelFormat pf = fb->getPixelFormat();
  int bpp = (int)fb->getBytesPerPixel();
  int fbWidth = fb->getDimension().width;
  int stride = fbWidth * bpp;

  // Two heap-allocated error rows.  We over-allocate by 2 so the kernel
  // can safely write one slot past either end without a bounds check.
  std::vector<int> errCurVec(w + 2, 0);
  std::vector<int> errNextVec(w + 2, 0);

  // Offset by 1 so that index 0 maps to column 0 and index -1 is valid.
  int *errCur  = &errCurVec[1];
  int *errNext = &errNextVec[1];

  const unsigned char *fbBase =
    (const unsigned char *)fb->getBufferPtr(rect->left, rect->top);

  for (int row = 0; row < h; row++) {
    const unsigned char *srcRow = fbBase + row * stride;
    unsigned char *outRow = out + row * rowBytes;

    memset(outRow, 0, rowBytes);

    // Reset next-row error accumulator.
    for (int k = -1; k <= w; k++) {
      errNext[k] = 0;
    }

    for (int col = 0; col < w; col++) {
      const unsigned char *p = srcRow + col * bpp;
      int luma = pixelLuma(p, bpp, pf);

      // Add accumulated error and clamp.
      int lumaErr = luma + errCur[col];
      if (lumaErr < 0)   lumaErr = 0;
      if (lumaErr > 255) lumaErr = 255;

      // Threshold at 128.
      int bit   = (lumaErr >= 128) ? 1 : 0;
      int error = lumaErr - (bit ? 255 : 0);

      // Pack: MSB = leftmost pixel.
      if (bit) {
        outRow[col >> 3] |= (unsigned char)(0x80u >> (col & 7));
      }

      // Distribute error.
      errCur [col + 1] += (error * 7) >> 4;  // right        7/16
      errNext[col - 1] += (error * 3) >> 4;  // lower-left   3/16
      errNext[col    ] += (error * 5) >> 4;  // below         5/16
      errNext[col + 1] += (error * 1) >> 4;  // lower-right  1/16
    }

    // Swap error rows.
    int *tmp = errCur;
    errCur   = errNext;
    errNext  = tmp;
  }

  return true;
}

// ---------------------------------------------------------------------------
// sendRectangle
//
// Called by UpdateSender after it has already sent the 12-byte rect header
// (x, y, w, h, encoding code). We only write the payload:
//   1 byte : dither-mode flag (0 = Floyd-Steinberg)
//   N bytes: packed 1-bpp data, ceil(w/8)*h bytes
// ---------------------------------------------------------------------------

void Mono1bppEncoder::sendRectangle(const Rect *rect,
                                    const FrameBuffer *serverFb,
                                    const EncodeOptions *options)
  throw(IOException)
{
  // Convert the rectangle to the client pixel format.
  const FrameBuffer *fb = m_pixelConverter->convert(rect, serverFb);

  int w = rect->getWidth();
  int h = rect->getHeight();
  int rowBytes = (w + 7) >> 3;
  int dataLen  = rowBytes * h;

  // Allocate packed output buffer.
  std::vector<unsigned char> packed(dataLen, 0);

  fsDitherAndPack(fb, rect, &packed[0]);

  // Send 1-byte dither-mode flag.
  m_output->writeUInt8((UINT8)EncodingDefs::MONO1BPP_DITHER_FLOYD_STEINBERG);

  // Send packed pixel data.
  m_output->writeFully((char *)&packed[0], dataLen);
}
