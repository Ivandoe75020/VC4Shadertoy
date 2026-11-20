FFMPEG_LICENSE_FLAGS="--disable-gpl --disable-nonfree --disable-version3"

FFMPEG_MINIMAL_FLAGS="--disable-everything --disable-autodetect --disable-programs --disable-doc --disable-debug --disable-network --disable-avdevice --disable-avfilter --disable-iconv --disable-shared --enable-static"

FFMPEG_LIBRARY_FLAGS="--enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample"

FFMPEG_COMPONENT_FLAGS="--enable-encoder=aac --enable-decoder=mp3float --enable-demuxer=mp3 --enable-muxer=mp4 --enable-parser=mpegaudio --enable-protocol=file"

FFMPEG_COMMON_FLAGS="$FFMPEG_LICENSE_FLAGS $FFMPEG_MINIMAL_FLAGS $FFMPEG_LIBRARY_FLAGS $FFMPEG_COMPONENT_FLAGS"

FFMPEG_WINDOWS_FLAGS="--toolchain=msvc --arch=x86_64 --target-os=win64 --enable-mediafoundation --enable-encoder=h264_mf --extra-cflags=-MD"
