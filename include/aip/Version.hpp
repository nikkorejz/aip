#pragma once

#define AIP_VERSION_MAJOR 0
#define AIP_VERSION_MINOR 1
#define AIP_VERSION_PATCH 1

#define AIP_VERSION_HEX ((AIP_VERSION_MAJOR<<16) | (AIP_VERSION_MINOR<<8) | (AIP_VERSION_PATCH))

namespace aip {
inline constexpr int version_major = AIP_VERSION_MAJOR;
inline constexpr int version_minor = AIP_VERSION_MINOR;
inline constexpr int version_patch = AIP_VERSION_PATCH;
} // namespace aip
