/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Symless Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "platform/OSXClipboard.h"

//! Clipboard image converter for macOS (PNG/TIFF)
class OSXClipboardImageConverter : public IOSXClipboardConverter
{
public:
  explicit OSXClipboardImageConverter(CFStringRef type);
  ~OSXClipboardImageConverter() override;

  // IOSXClipboardConverter overrides
  IClipboard::Format getFormat() const override;
  CFStringRef getOSXFormat() const override;
  std::string fromIClipboard(const std::string &data) const override;
  std::string toIClipboard(const std::string &data) const override;

private:
  CFStringRef m_type;
};
