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

#include <string.h>
#include <vector>

#include "Mono1bppZEncoder.h"
#include "rfb/EncodingDefs.h"

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Mono1bppZEncoder::Mono1bppZEncoder(PixelConverter *conv,
                                   DataOutputStream *output)
: Mono1bppEncoder(conv, output)
{
  // m_deflater is default-constructed: deflateInit with Z_DEFAULT_COMPRESSION,
  // persistent z_stream that accumulates a dictionary across all rects sent
  // on this connection.
}

Mono1bppZEncoder::~Mono1bppZEncoder()
{
}

// ---------------------------------------------------------------------------
// getCode
// ---------------------------------------------------------------------------

int Mono1bppZEncoder::getCode() const
{
  return EncodingDefs::MONO1BPPZ;
}

// ---------------------------------------------------------------------------
// sendRectangle
//
// Called by UpdateSender after it has already sent the 12-byte rect header.
// Payload written:
//   1 byte  : dither-mode flag (0 = Floyd-Steinberg)
//   4 bytes : uint32 nBytes of compressed data (big-endian via writeUInt32)
//   N bytes : deflated packed 1-bpp bits
// ---------------------------------------------------------------------------

void Mono1bppZEncoder::sendRectangle(const Rect *rect,
                                     const FrameBuffer *serverFb,
                                     const EncodeOptions *options)
  throw(IOException)
{
  // Convert rectangle to client pixel format.
  const FrameBuffer *fb = m_pixelConverter->convert(rect, serverFb);

  int w        = rect->getWidth();
  int h        = rect->getHeight();
  int rowBytes = (w + 7) >> 3;   // ceil(w/8)
  int dataLen  = rowBytes * h;

  // Dither into a packed 1-bpp buffer.
  std::vector<unsigned char> packed(dataLen, 0);
  fsDitherAndPack(fb, rect, &packed[0]);

  // Deflate the packed bits.
  m_deflater.setInput((const char *)&packed[0], dataLen);
  m_deflater.deflate();

  // Send dither-mode byte.
  m_output->writeUInt8((UINT8)EncodingDefs::MONO1BPP_DITHER_FLOYD_STEINBERG);

  // Send 4-byte compressed-data length followed by the compressed data.
  m_output->writeUInt32((UINT32)m_deflater.getOutputSize());
  m_output->writeFully(m_deflater.getOutput(),
                       m_deflater.getOutputSize());
}
