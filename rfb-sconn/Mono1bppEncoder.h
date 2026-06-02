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

#ifndef __RFB_MONO1BPP_ENCODER_H_INCLUDED__
#define __RFB_MONO1BPP_ENCODER_H_INCLUDED__

#include "Encoder.h"

//
// Mono1bppEncoder implements the MONO1BPP encoding (code -752 / 0xFFFFFD10).
//
// Wire format per rectangle (payload only – the 12-byte rect header is sent
// by UpdateSender before calling sendRectangle):
//
//   [uint8_t dither_mode]         1 byte  – always 0 (Floyd-Steinberg)
//   [uint8_t packed[ceil(w/8)*h]]         – MSB = leftmost pixel, rows
//                                            zero-padded to byte boundary
//
// Pixels are first converted to the client pixel format via m_pixelConverter,
// then luminance is derived from the converted pixels and Floyd-Steinberg
// dithering is applied before packing 8 pixels per byte.
//
class Mono1bppEncoder : public Encoder
{
public:
  Mono1bppEncoder(PixelConverter *conv, DataOutputStream *output);
  virtual ~Mono1bppEncoder();

  virtual int getCode() const;

  virtual void sendRectangle(const Rect *rect,
                             const FrameBuffer *serverFb,
                             const EncodeOptions *options) throw(IOException);

private:
  // Do not allow copying.
  Mono1bppEncoder(const Mono1bppEncoder &);
  Mono1bppEncoder &operator=(const Mono1bppEncoder &);

protected:
  // Compute 8-bit luminance from a pixel stored in the converted framebuffer.
  // `bpp` is bytes-per-pixel (1, 2, or 4); `pf` is the pixel's format.
  static int pixelLuma(const unsigned char *p, int bpp,
                       const PixelFormat &pf);

  // Apply Floyd-Steinberg dithering to one (w x h) region of `fb` (already
  // in client pixel format) and write packed 1-bpp output to `out`.
  // `out` must be at least ceil(w/8)*h bytes.
  // Returns false on allocation failure.
  static bool fsDitherAndPack(const FrameBuffer *fb,
                              const Rect *rect,
                              unsigned char *out);
};

#endif // __RFB_MONO1BPP_ENCODER_H_INCLUDED__
