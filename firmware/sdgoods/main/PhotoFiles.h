// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "platform/common/PosixUserFiles.h"

namespace Sdgoods {
// The application thread owns this archive for the platform lifetime. Resetting
// the game does not unmount, erase, or otherwise modify this independent store.
class PhotoFiles final : public Platform::PosixUserFiles {
public:
    PhotoFiles() : PosixUserFiles("/userdata/photos") {}
    bool begin() override;
    size_t availableBytes() override;
    bool queryAvailableBytes(size_t& available) override;
private:
    bool mounted_ = false;
};
}
