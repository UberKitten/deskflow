/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Symless Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXClipboardImageConverter.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <MobileCoreServices/MobileCoreServices.h>

#include <vector>

namespace {

struct CBMPInfoHeader {
  uint32_t biSize;
  int32_t biWidth;
  int32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t biXPelsPerMeter;
  int32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};

static inline uint16_t fromLEU16(const uint8_t *data)
{
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

static inline int32_t fromLES32(const uint8_t *data)
{
  return static_cast<int32_t>(
      static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
      (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24)
  );
}

static inline uint32_t fromLEU32(const uint8_t *data)
{
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static void toLE(uint8_t *&dst, uint16_t src)
{
  dst[0] = static_cast<uint8_t>(src & 0xffu);
  dst[1] = static_cast<uint8_t>((src >> 8) & 0xffu);
  dst += 2;
}

static void toLE(uint8_t *&dst, int32_t src)
{
  dst[0] = static_cast<uint8_t>(src & 0xffu);
  dst[1] = static_cast<uint8_t>((src >> 8) & 0xffu);
  dst[2] = static_cast<uint8_t>((src >> 16) & 0xffu);
  dst[3] = static_cast<uint8_t>((src >> 24) & 0xffu);
  dst += 4;
}

static void toLE(uint8_t *&dst, uint32_t src)
{
  dst[0] = static_cast<uint8_t>(src & 0xffu);
  dst[1] = static_cast<uint8_t>((src >> 8) & 0xffu);
  dst[2] = static_cast<uint8_t>((src >> 16) & 0xffu);
  dst[3] = static_cast<uint8_t>((src >> 24) & 0xffu);
  dst += 4;
}

static bool readDIBHeader(const std::string &dib, CBMPInfoHeader &info)
{
  if (dib.size() < sizeof(CBMPInfoHeader)) {
    return false;
  }

  const auto *raw = reinterpret_cast<const uint8_t *>(dib.data());
  info.biSize = fromLEU32(raw + 0);
  info.biWidth = fromLES32(raw + 4);
  info.biHeight = fromLES32(raw + 8);
  info.biPlanes = fromLEU16(raw + 12);
  info.biBitCount = fromLEU16(raw + 14);
  info.biCompression = fromLEU32(raw + 16);
  info.biSizeImage = fromLEU32(raw + 20);
  info.biXPelsPerMeter = fromLES32(raw + 24);
  info.biYPelsPerMeter = fromLES32(raw + 28);
  info.biClrUsed = fromLEU32(raw + 32);
  info.biClrImportant = fromLEU32(raw + 36);

  return true;
}

static CGImageRef createCGImageFromDIB(const std::string &dib)
{
  CBMPInfoHeader info{};
  if (!readDIBHeader(dib, info)) {
    return nullptr;
  }

  constexpr uint32_t kBI_RGB = 0;
  if (info.biPlanes != 1 || info.biCompression != kBI_RGB ||
      (info.biBitCount != 24 && info.biBitCount != 32) ||
      info.biWidth == 0 || info.biHeight == 0) {
    return nullptr;
  }

  const bool topDown = info.biHeight < 0;
  const int32_t height = topDown ? -info.biHeight : info.biHeight;
  const int32_t width = info.biWidth;

  const size_t headerSize = info.biSize;
  if (headerSize < sizeof(CBMPInfoHeader) || dib.size() <= headerSize) {
    return nullptr;
  }

  const size_t srcRowBytes = (info.biBitCount == 24)
                                 ? ((static_cast<size_t>(width) * 3 + 3) & ~3u)
                                 : (static_cast<size_t>(width) * 4);
  const uint8_t *src = reinterpret_cast<const uint8_t *>(dib.data()) + headerSize;

  std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
  const size_t dstRowBytes = static_cast<size_t>(width) * 4;

  for (int32_t row = 0; row < height; ++row) {
    const int32_t srcRow = topDown ? row : (height - 1 - row);
    const uint8_t *srcRowPtr = src + static_cast<size_t>(srcRow) * srcRowBytes;
    uint8_t *dstRowPtr = pixels.data() + static_cast<size_t>(row) * dstRowBytes;

    if (info.biBitCount == 24) {
      for (int32_t col = 0; col < width; ++col) {
        const uint8_t *p = srcRowPtr + col * 3;
        dstRowPtr[col * 4 + 0] = p[0]; // B
        dstRowPtr[col * 4 + 1] = p[1]; // G
        dstRowPtr[col * 4 + 2] = p[2]; // R
        dstRowPtr[col * 4 + 3] = 0xFF; // A
      }
    } else {
      for (int32_t col = 0; col < width; ++col) {
        const uint8_t *p = srcRowPtr + col * 4;
        dstRowPtr[col * 4 + 0] = p[0];
        dstRowPtr[col * 4 + 1] = p[1];
        dstRowPtr[col * 4 + 2] = p[2];
        dstRowPtr[col * 4 + 3] = p[3];
      }
    }
  }

  CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, pixels.data(), pixels.size());
  if (!dataRef) {
    return nullptr;
  }

  CGDataProviderRef provider = CGDataProviderCreateWithCFData(dataRef);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;

  CGImageRef image = CGImageCreate(
      width, height, 8, 32, dstRowBytes, colorSpace, bitmapInfo, provider, nullptr, false,
      kCGRenderingIntentDefault
  );

  CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  CFRelease(dataRef);

  return image;
}

static std::string createDIBFromCGImage(CGImageRef image)
{
  if (!image) {
    return std::string();
  }

  const size_t width = CGImageGetWidth(image);
  const size_t height = CGImageGetHeight(image);
  if (width == 0 || height == 0) {
    return std::string();
  }

  const size_t rowBytes = width * 4;
  std::vector<uint8_t> pixels(rowBytes * height);

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate(
      pixels.data(), width, height, 8, rowBytes, colorSpace,
      kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst
  );
  CGColorSpaceRelease(colorSpace);

  if (!ctx) {
    return std::string();
  }

  CGContextDrawImage(ctx, CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)), image);
  CGContextRelease(ctx);

  // build BITMAPINFOHEADER (40 bytes)
  uint8_t header[40];
  uint8_t *dst = header;
  toLE(dst, static_cast<uint32_t>(40));
  toLE(dst, static_cast<int32_t>(width));
  toLE(dst, static_cast<int32_t>(height));
  toLE(dst, static_cast<uint16_t>(1));
  toLE(dst, static_cast<uint16_t>(32));
  toLE(dst, static_cast<uint32_t>(0)); // BI_RGB
  toLE(dst, static_cast<uint32_t>(rowBytes * height));
  toLE(dst, static_cast<int32_t>(2834)); // 72 dpi
  toLE(dst, static_cast<int32_t>(2834)); // 72 dpi
  toLE(dst, static_cast<uint32_t>(0));
  toLE(dst, static_cast<uint32_t>(0));

  std::string dib(reinterpret_cast<const char *>(header), sizeof(header));

  // DIB is bottom-up when height is positive
  for (size_t row = 0; row < height; ++row) {
    const size_t srcRow = height - 1 - row;
    const uint8_t *src = pixels.data() + srcRow * rowBytes;
    dib.append(reinterpret_cast<const char *>(src), rowBytes);
  }

  return dib;
}

static std::string convertDIBToFormat(const std::string &dib, CFStringRef type)
{
  CGImageRef image = createCGImageFromDIB(dib);
  if (!image) {
    return std::string();
  }

  CFMutableDataRef outData = CFDataCreateMutable(kCFAllocatorDefault, 0);
  if (!outData) {
    CGImageRelease(image);
    return std::string();
  }

  CGImageDestinationRef dest = CGImageDestinationCreateWithData(outData, type, 1, nullptr);
  if (!dest) {
    CFRelease(outData);
    CGImageRelease(image);
    return std::string();
  }

  CGImageDestinationAddImage(dest, image, nullptr);
  if (!CGImageDestinationFinalize(dest)) {
    CFRelease(dest);
    CFRelease(outData);
    CGImageRelease(image);
    return std::string();
  }

  std::string out(reinterpret_cast<const char *>(CFDataGetBytePtr(outData)), CFDataGetLength(outData));

  CFRelease(dest);
  CFRelease(outData);
  CGImageRelease(image);
  return out;
}

static std::string convertFormatToDIB(const std::string &data)
{
  CFDataRef inData = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(data.data()), data.size());
  if (!inData) {
    return std::string();
  }

  CGImageSourceRef source = CGImageSourceCreateWithData(inData, nullptr);
  CFRelease(inData);
  if (!source) {
    return std::string();
  }

  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
  CFRelease(source);
  if (!image) {
    return std::string();
  }

  std::string dib = createDIBFromCGImage(image);
  CGImageRelease(image);
  return dib;
}

} // namespace

OSXClipboardImageConverter::OSXClipboardImageConverter(CFStringRef type) : m_type(type)
{
  if (m_type != nullptr) {
    CFRetain(m_type);
  }
}

OSXClipboardImageConverter::~OSXClipboardImageConverter()
{
  if (m_type != nullptr) {
    CFRelease(m_type);
  }
}

IClipboard::Format OSXClipboardImageConverter::getFormat() const
{
  return IClipboard::Format::Bitmap;
}

CFStringRef OSXClipboardImageConverter::getOSXFormat() const
{
  return m_type;
}

std::string OSXClipboardImageConverter::fromIClipboard(const std::string &data) const
{
  return convertDIBToFormat(data, m_type);
}

std::string OSXClipboardImageConverter::toIClipboard(const std::string &data) const
{
  return convertFormatToDIB(data);
}
