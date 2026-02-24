# Clipboard: Windows → macOS image paste failure (2026-02-24)

## Symptom
- Images copied on Windows (e.g., Snipping Tool / Discord) do not paste correctly in macOS apps (Webex/Word). In Word, a small blank image (~50x50) appears.

## Server log evidence
- Windows server converts CF_DIB to canonical 32-bit BI_RGB and sends it:
  - `bitmap: 949x273 32`
  - `convert image from: depth=32 comp=3`
  - `sending clipboard ... size=1036360`

## Root cause hypothesis
- macOS side only advertises the clipboard image as **com.microsoft.bmp** (BMP flavor). Many macOS apps prefer **public.png** or **public.tiff** and may ignore the BMP flavor, resulting in failed or blank pastes.

## Fix implemented
- Added **PNG and TIFF clipboard flavors** on macOS for `IClipboard::Format::Bitmap`:
  - New converter `OSXClipboardImageConverter` uses ImageIO to translate DIB → PNG/TIFF and PNG/TIFF → DIB.
  - `OSXClipboard` now registers converters for `kUTTypePNG` and `kUTTypeTIFF` (in addition to BMP).

## Files changed
- `src/lib/platform/OSXClipboardImageConverter.{h,cpp}` (new)
- `src/lib/platform/OSXClipboard.cpp` (register PNG/TIFF converters)
- `src/lib/platform/CMakeLists.txt` (add new sources)
