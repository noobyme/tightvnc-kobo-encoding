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

#ifndef __RFB_MONO1BPPZ_ENCODER_H_INCLUDED__
#define __RFB_MONO1BPPZ_ENCODER_H_INCLUDED__

#include "Mono1bppEncoder.h"
#include "util/Deflater.h"

//
// Mono1bppZEncoder implements the MONO1BPPZ encoding (code -753 / 0xFFFFFD0F).
//
// Identical to Mono1bppEncoder except the packed 1-bpp bits are deflated
// before transmission.
//
// Wire format per rectangle (payload only — the 12-byte rect header is sent
// by UpdateSender before calling sendRectangle):
//
//   [uint8_t  dither_mode]          1 byte  – always 0 (Floyd-Steinberg)
//   [uint32_t nBytes     ]          4 bytes – size of compressed data (BE)
//   [uint8_t  compressed[nBytes]]           – zlib-deflated packed bits
//
// The Deflater member maintains its z_stream across rectangles, building a
// shared dictionary that improves compression on consecutive updates.
//
class Mono1bppZEncoder : public Mono1bppEncoder
{
public:
  Mono1bppZEncoder(PixelConverter *conv, DataOutputStream *output);
  virtual ~Mono1bppZEncoder();

  virtual int getCode() const;

  virtual void sendRectangle(const Rect *rect,
                             const FrameBuffer *serverFb,
                             const EncodeOptions *options) throw(IOException);

private:
  // Persistent deflate stream — maintains dictionary across rects so
  // repeated patterns (e.g. large white/black areas) compress better over time.
  Deflater m_deflater;

  // Do not allow copying.
  Mono1bppZEncoder(const Mono1bppZEncoder &);
  Mono1bppZEncoder &operator=(const Mono1bppZEncoder &);
};

#endif // __RFB_MONO1BPPZ_ENCODER_H_INCLUDED__
