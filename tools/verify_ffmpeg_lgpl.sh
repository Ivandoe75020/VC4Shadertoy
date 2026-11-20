set -e

BUILD_DIR="$1"
EXPECT_H264_MF="$2"

CONFIG_H="$BUILD_DIR/config.h"
COMPONENTS_H="$BUILD_DIR/config_components.h"

if [ ! -f "$CONFIG_H" ] || [ ! -f "$COMPONENTS_H" ]; then
    echo "FFmpeg configuration headers not found in $BUILD_DIR"
    exit 1
fi

fail=0

require_define() {
    if ! grep -hEq "^#define $1 $2\$" "$CONFIG_H" "$COMPONENTS_H"; then
        echo "FFmpeg configuration check failed: expected $1 = $2"
        fail=1
    fi
}

require_define CONFIG_GPL 0
require_define CONFIG_NONFREE 0
require_define CONFIG_VERSION3 0
require_define CONFIG_GPLV3 0
require_define CONFIG_AVFILTER 0
require_define CONFIG_AVDEVICE 0
require_define CONFIG_NETWORK 0
require_define CONFIG_AAC_ENCODER 1
require_define CONFIG_MP3FLOAT_DECODER 1
require_define CONFIG_MP4_MUXER 1
require_define CONFIG_MP3_DEMUXER 1
require_define CONFIG_FILE_PROTOCOL 1

if [ "$EXPECT_H264_MF" = "yes" ]; then
    require_define CONFIG_H264_MF_ENCODER 1
    require_define CONFIG_MEDIAFOUNDATION 1
fi

external=$(grep -hE "^#define CONFIG_LIB[A-Z0-9_]+ 1\$" "$CONFIG_H" "$COMPONENTS_H" || true)
if [ -n "$external" ]; then
    echo "FFmpeg configuration check failed: external libraries are enabled:"
    echo "$external"
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    exit 1
fi

echo "FFmpeg configuration is LGPL only (no GPL, no non-free, no external libraries)."
