set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/tools/ffmpeg_config.sh"

SRC="$ROOT/third_party/ffmpeg/src"
BUILD="$ROOT/obj/ffmpeg/x64"
PREFIX="$ROOT/third_party/ffmpeg/x64"

if ! command -v cl > /dev/null 2>&1; then
    echo "The MSVC compiler (cl.exe) is not on the PATH. Run tools/build_ffmpeg.ps1 instead."
    exit 1
fi

CL_DIR=$(dirname "$(command -v cl)")
export PATH="$CL_DIR:$PATH"

ASM_FLAG=""
if ! command -v nasm > /dev/null 2>&1; then
    echo "nasm was not found: building without x86 assembly optimizations (slower RGB to YUV conversion)."
    ASM_FLAG="--disable-x86asm"
fi

rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

"$SRC/configure" --prefix="$PREFIX" $FFMPEG_COMMON_FLAGS $FFMPEG_WINDOWS_FLAGS $ASM_FLAG

bash "$ROOT/tools/verify_ffmpeg_lgpl.sh" "$BUILD" yes

make -j"$(nproc)"

rm -rf "$PREFIX"
make install-libs install-headers

echo "FFmpeg static libraries installed in $PREFIX"
