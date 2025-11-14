# FFmpeg Build Instructions
## Alternative Development Edition

Build FFmpeg with HAP encoder enhancements and comprehensive codec support

### Prerequisites
You need to be in the **MSYS2 MINGW64** terminal (not regular cmd or PowerShell).

### Step 1: Install Dependencies

First time setup or if you need to reinstall dependencies:

```bash
./install-dependencies.sh
```

This installs all required MSYS2 packages including:
- **Snappy** (required for HAP encoder)
- x264, x265, libvpx (video codecs)
- libvorbis, opus, lame (audio codecs)
- Build tools (GCC, YASM, NASM)

### Step 2: Build FFmpeg Portable Build (Recommended for apps)
```bash
./rebuild-static.sh
```
- Same executables (~30MB) + required DLLs (~5 files)
- **Automatically bundles MINGW64 DLLs**
- Works from PowerShell, CMD, and Python apps without MSYS2

build include:
1. Clean previous build
2. Configure FFmpeg with all codecs (including HAP with Snappy)
3. Build FFmpeg (takes 5-15 minutes depending on CPU)
4. Create executables: `ffmpeg.exe`, `ffplay.exe`, `ffprobe.exe`
5. Test that HAP encoder is available

### Step 3: Deploy to Your App (Optional)

Deploy executables and DLLs to your application folder:

```bash
./copy-with-dlls.sh /c/AD/nnTools/tools/ffmpeg
```

This works for both build options:
- **Option A**: Finds and copies the required DLLs
- **Option B**: Copies the pre-bundled DLLs from the build directory

### Codecs Included

✅ **HAP** (Alternative Development Edition enhancements!)
- hap (DXT1) - FourCC: 'Hap1'
- hap_alpha (DXT5) - FourCC: 'Hap5'
- **hap_q** (DXT5-YCoCg) - FourCC: 'HapY' ← Fixed YCoCg transform!
- **hap_a** (RGTC1 alpha-only) - FourCC: 'HapA' ← New format!
- **hap_m** (DXT5-YCoCg + RGTC1) - FourCC: 'HapM' ← Hap Q Alpha! New format!

✅ **ProRes** - Apple ProRes (built-in)
- prores - Standard ProRes encoder
- prores_aw - Anatoliy's encoder
- **prores_ks** - Kostya's encoder (high quality)

✅ **x264** - H.264/AVC encoder
✅ **x265** - H.265/HEVC encoder
✅ **libvpx** - VP8/VP9 encoder
✅ **OpenEXR** - EXR decoder
✅ **qtrle** - QuickTime Animation (built-in)
✅ **vorbis** - Vorbis audio
✅ **opus** - Opus audio
✅ **mp3lame** - MP3 encoder

⚠️ **fdk-aac** - Not available (licensing restrictions in MSYS2)
- Alternative: Use FFmpeg's built-in AAC encoder (`-c:a aac`)

### Test Your Build

Check HAP codecs:
```bash
./ffmpeg.exe -encoders | grep hap
./ffmpeg.exe -decoders | grep hap
```

### HAP Encoding Examples

**HAP (DXT1)** - Basic, no alpha:
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap output_hap.mov
```

**HAP Alpha (DXT5)** - RGBA with alpha:
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap_alpha output_hap_alpha.mov
```

**HAP Q (DXT5-YCoCg)** - Better quality, no alpha:
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap_q output_hap_q.mov
```

**HAP Alpha-Only (RGTC1)** - Alpha channel only:
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap_a output_hap_a.mov
```

**HAP M (DXT5-YCoCg + RGTC1)** - HAP Q Alpha, best quality with alpha:
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap_m output_hap_m.mov
```

**Multi-threaded encoding** (faster):
```bash
./ffmpeg.exe -i input.mov -c:v hap -format hap_q -chunks 4 output_hap_q.mov
```

### Troubleshooting

**Error: "pacman: command not found"**
- You're not in MSYS2 MINGW64 terminal
- Open Windows Terminal → Select "MSYS2 MINGW64"

**Error: "configure: not found"**
- Wrong directory. Run: `cd /c/ff/ff`

**Build fails**
- Run `make clean` and try again
- Check you have enough disk space (>5GB recommended)



FFmpeg README
=============

FFmpeg is a collection of libraries and tools to process multimedia content
such as audio, video, subtitles and related metadata.

## Libraries

* `libavcodec` provides implementation of a wider range of codecs.
* `libavformat` implements streaming protocols, container formats and basic I/O access.
* `libavutil` includes hashers, decompressors and miscellaneous utility functions.
* `libavfilter` provides means to alter decoded audio and video through a directed graph of connected filters.
* `libavdevice` provides an abstraction to access capture and playback devices.
* `libswresample` implements audio mixing and resampling routines.
* `libswscale` implements color conversion and scaling routines.

## Tools

* [ffmpeg](https://ffmpeg.org/ffmpeg.html) is a command line toolbox to
  manipulate, convert and stream multimedia content.
* [ffplay](https://ffmpeg.org/ffplay.html) is a minimalistic multimedia player.
* [ffprobe](https://ffmpeg.org/ffprobe.html) is a simple analysis tool to inspect
  multimedia content.
* Additional small tools such as `aviocat`, `ismindex` and `qt-faststart`.

## Documentation

The offline documentation is available in the **doc/** directory.

The online documentation is available in the main [website](https://ffmpeg.org)
and in the [wiki](https://trac.ffmpeg.org).

### Examples

Coding examples are available in the **doc/examples** directory.

## License

FFmpeg codebase is mainly LGPL-licensed with optional components licensed under
GPL. Please refer to the LICENSE file for detailed information.

## Contributing

Patches should be submitted to the ffmpeg-devel mailing list using
`git format-patch` or `git send-email`. Github pull requests should be
avoided because they are not part of our review process and will be ignored.
