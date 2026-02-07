# HEVC Alpha Channel Support - Investigation & Fix Guide

## Executive Summary

**Status**: HEVC alpha encoding **works**, but decoding **fails**. Alpha data IS encoded in the file, but FFmpeg can't decode it back.

**The Good News** ✅:
- x265 library has full alpha support enabled
- FFmpeg correctly encodes alpha (confirms with `yuva420p10le` output format)
- Two encoding statistics prove both base + alpha layers are written
- VP8/VP9 already implements alpha support - we can copy their pattern!

**The Problem** ❌:
- HEVC decoder doesn't recognize multi-layer profile → "Unknown profile bitstream"
- Decoder can't separate alpha layer NALs from base layer NALs
- No code to merge decoded layers into YUVA output frames

**Solution**: Implement multi-layer HEVC decoding following the working VP9 pattern (estimated ~1 week effort).

---

## Current Status (2026-01-29)

HEVC alpha encoding with x265 **partially works** but decoding/playback fails.

## What Works ✅

1. **x265 Library**: Built with `ENABLE_ALPHA=ON` and `HIGH_BIT_DEPTH=ON`
2. **Encoding**: FFmpeg's libx265 wrapper correctly detects alpha planes and enables alpha encoding
3. **Muxing**: MP4 muxer writes the multi-layer HEVC bitstream to file
4. **Alpha Detection**: Encoder shows `yuva420p10le` format (the "A" = alpha plane detected!)

## What Fails ❌

1. **Demuxing**: MP4 demuxer only exposes base layer, not alpha layer
2. **Decoding**: HEVC decoder doesn't recognize multi-layer profile
3. **Playback**: No alpha channel visible in decoded output (only shows `yuv420p10le` - missing "A"!)

## Test Case

### Encoding Command
```bash
ffmpeg.exe -y -i J:/test/adGrid_checker_1k.mov -c:v libx265 -crf 22 -tag:v hvc1 -write_tmcd 0 -an J:\test\adGrid_checker_1k__@22.mp4
```

**Input**: ProRes 4444 XQ with alpha (`yuva444p12le`, 1024x1024, 25fps)
**Output**: Multi-layer HEVC with alpha (base + alpha layers)

### FFmpeg Correctly Detects Alpha During Encoding ✅

Look at the Stream mapping output:
```
Stream mapping:
  Stream #0:0 -> #0:0 (prores (native) -> hevc (libx265))

Output #0, mp4, to 'J:\test\adGrid_checker_1k__@22.mp4':
  Stream #0:0(eng): Video: hevc (hvc1 / 0x31637668), yuva420p10le(tv, bt709, progressive), 1024x1024
                                                        ^^^^^^^^^^^
                                                        Alpha detected!
```

**Key Point**: FFmpeg's libx265 encoder wrapper correctly:
- Detected the alpha plane in the input (`yuva444p12le`)
- Set `yuva420p10le` as the encoding pixel format (note the "A" = alpha)
- Enabled x265's alpha encoding mode automatically
- x265 encoded two separate layers (shown in statistics)

### Evidence of Alpha Encoding
x265 outputs **TWO sets** of encoding statistics:
```
encoded 282 frames in 4.32s (65.31 fps), 2177.96 kb/s, Avg QP:26.98  ← Base layer
encoded 282 frames in 4.32s (65.31 fps), 471.05 kb/s, Avg QP:28.73   ← Alpha layer
```

### Decoding Errors
```
[hevc @ ...] Multiple scalability types presented
[hevc @ ...] Unknown profile bitstream
Stream #0:0: Video: hevc (Main 10) (hvc1 / 0x31637668), yuv420p10le(tv, bt709, progressive), 1024x1024
```

**Problem**: Only shows `yuv420p10le` (no alpha), and decoder doesn't recognize the profile.

### Visual Comparison: What Should Happen vs What Actually Happens

| Stage | Expected (Working) | Actual (Broken) |
|-------|-------------------|-----------------|
| **Input Format** | `yuva444p12le` (ProRes with alpha) | `yuva444p12le` ✅ |
| **Encoder Input** | Detects alpha plane | Detected ✅ (`yuva420p10le`) |
| **Encoding** | Two layers: base + alpha | Two layers encoded ✅ (see stats) |
| **File Size** | ~3.6 MB (base + alpha) | ~3.6 MB ✅ (alpha is there!) |
| **Decoder Output** | `yuva420p10le` (base + alpha) | `yuv420p10le` ❌ (alpha missing!) |
| **Playback** | Transparent background visible | Solid black background ❌ |

The alpha data **is in the file** - we just need to teach the decoder how to extract it!

---

## Technical Analysis

### 1. Encoder (libx265 wrapper) - **WORKS** ✅

**File**: [libavcodec/libx265.c:559-566](libavcodec/libx265.c#L559-L566)

```c
#if X265_BUILD >= 210 && FF_X265_MAX_LAYERS > 1
    if (desc->flags & AV_PIX_FMT_FLAG_ALPHA) {
        if (ctx->api->param_parse(ctx->params, "alpha", "1") < 0) {
            av_log(avctx, AV_LOG_ERROR, "Loaded libx265 does not support alpha layer encoding.\n");
            return AVERROR(ENOTSUP);
        }
    }
#endif
```

This correctly detects alpha planes and enables x265 alpha encoding.

### 2. Demuxer (MP4/MOV) - **INCOMPLETE** ⚠️

**File**: [libavformat/hevc.c](libavformat/hevc.c)

#### What Exists:
- **Lines 86, 430-431, 452**: Detects `alpha_layer_nuh_id` from VPS extensions
- **Line 885**: Filters NAL units based on `alpha_layer_nuh_id`

```c
typedef struct HEVCDecoderConfigurationRecord {
    // ... other fields ...
    uint8_t alpha_layer_nuh_id;  // Line 86
} HEVCDecoderConfigurationRecord;

// Line 885: Filter NAL units
if (!is_lhvc && nuh_layer_id > 0 && nuh_layer_id != hvcc->alpha_layer_nuh_id)
    goto end;
```

#### What's Missing:
- Code to **create a second stream** for the alpha layer
- OR code to **merge alpha into the base layer** during decoding
- The demuxer only exposes `Stream #0:0` (base layer), alpha is ignored

### 3. Decoder (HEVC/H.265) - **FAILS** ❌

**File**: [libavcodec/hevc/ps.c](libavcodec/hevc/ps.c)

#### Profile Validation (Lines 238-311)
```c
// Line 249: Read profile_idc from bitstream
ptl->profile_idc = get_bits(gb, 5);

// Lines 252-256: Look up profile name
for (int i = 0; ff_hevc_profiles[i].profile != AV_PROFILE_UNKNOWN; i++)
    if (ff_hevc_profiles[i].profile == ptl->profile_idc) {
        profile_name = ff_hevc_profiles[i].name;
        break;
    }

// Lines 258-259: Log warning if profile not recognized
av_log(avctx, profile_name ? AV_LOG_DEBUG : AV_LOG_WARNING,
       "%s profile bitstream\n", profile_name ? profile_name : "Unknown");
```

#### Supported Profiles (libavcodec/profiles.c:97-105)
```c
const AVProfile ff_hevc_profiles[] = {
    { AV_PROFILE_HEVC_MAIN,                 "Main"                },  // 1
    { AV_PROFILE_HEVC_MAIN_10,              "Main 10"             },  // 2
    { AV_PROFILE_HEVC_MAIN_STILL_PICTURE,   "Main Still Picture"  },  // 3
    { AV_PROFILE_HEVC_REXT,                 "Rext"                },  // 4
    { AV_PROFILE_HEVC_MULTIVIEW_MAIN,       "Multiview Main"      },  // 6
    { AV_PROFILE_HEVC_SCC,                  "Scc"                 },  // 9
    { AV_PROFILE_UNKNOWN },
};
```

**Problem**: Multi-layer HEVC with alpha uses a profile_idc not in this list!

#### Alpha Channel Support (Lines 544-587)
```c
// Line 544: Comment hints at x265 behavior
// x265 specify MULTIVIEW when the stream really is alpha video only.

// Lines 580-586: Alpha support is limited
/* AuxId 1 is alpha, 2 is depth. Only support alpha */
if (vps->scalability_mask_flag & HEVC_SCALABILITY_AUXILIARY &&
    dimension_id[index] != HEVC_AUX_ALPHA) {
    av_log(avctx, AV_LOG_WARNING,
           "Unsupported dimension_id %d for HEVC_SCALABILITY_AUXILIARY\n",
           dimension_id[index]);
    return AVERROR_PATCHWELCOME;
}
```

The decoder has **partial** multi-layer support but can't actually decode the layers.

---

## Root Causes

1. **Demuxer doesn't expose alpha stream**:
   - Detects `alpha_layer_nuh_id` but doesn't use it
   - Only creates one AVStream instead of two

2. **Decoder doesn't recognize multi-layer profile**:
   - x265 uses a profile_idc that's not in `ff_hevc_profiles[]`
   - Likely uses Multiview Main (profile_idc=6) OR a newer profile

3. **No alpha decoding pipeline**:
   - Even if profile was recognized, decoder doesn't decode alpha layer
   - No code to merge alpha back into decoded frames

---

## How x265 Encodes Alpha

x265 uses **HEVC multi-layer extension** (specified in ITU-T H.265 Annex F):

1. **Base layer** (nuh_layer_id=0): RGB/YUV color data
2. **Enhancement layer** (nuh_layer_id>0): Alpha auxiliary data
3. **VPS extension**: Signals scalability type and layer structure

The layers are interleaved in the bitstream with NAL unit headers indicating which layer each NAL belongs to.

---

## Fix Strategy

### Phase 1: Decoder Support (REQUIRED)

**Goal**: Make FFmpeg's HEVC decoder recognize and decode multi-layer streams

**Files to modify**:
1. `libavcodec/profiles.c` - Add multi-layer alpha profile
2. `libavcodec/hevc/ps.c` - Extend profile validation
3. `libavcodec/hevc/hevcdec.c` - Add alpha layer decoding logic
4. `libavcodec/hevc/hevc.h` - Add data structures for alpha

**Steps**:
1. Identify the exact profile_idc x265 uses for alpha encoding
2. Add it to `ff_hevc_profiles[]` array
3. Implement alpha layer NAL unit handling in decoder
4. Output decoded frames with alpha plane (e.g., `yuva420p10le`)

### Phase 2: Demuxer Support (OPTIONAL)

**Goal**: Expose alpha layer as separate stream OR merge during demux

**Files to modify**:
1. `libavformat/hevc.c` - Use `alpha_layer_nuh_id` to create second stream
2. `libavformat/isom.h` - Add alpha track structures
3. `libavformat/mov.c` - Handle multi-track HEVC alpha

**Options**:
- **Option A**: Create two streams (Stream #0:0=color, Stream #0:1=alpha)
- **Option B**: Keep one stream, decoder merges layers → output with alpha plane

**Recommendation**: Option B is simpler and matches user expectations.

---

## Investigation Steps

### Step 1: Verify Alpha is Actually Encoded

Extract raw HEVC bitstream:
```bash
ffmpeg -i J:\test\adGrid_checker_1k__@22.mp4 -c:v copy -bsf:v hevc_mp4toannexb -f hevc output.hevc
```

Examine with hex editor or H.265 analyzer:
- Look for NAL units with `nuh_layer_id > 0`
- Check VPS NAL unit for scalability extension
- Confirm alpha layer data exists

### Step 2: Identify Profile Used

Extract decoder configuration:
```bash
ffprobe -show_packets -show_data J:\test\adGrid_checker_1k__@22.mp4 2>&1 | grep -A 50 "codec_type=video"
```

Look for:
- `profile=...` value
- VPS/SPS general_profile_idc

### Step 3: Test with x265 CLI Decoder

If x265 has a decoder (not standard), test:
```bash
# This may not work - x265 is primarily an encoder
x265 --input output.hevc --output decoded.yuv
```

### Step 4: Compare with Reference

Test with other HEVC multi-layer decoders:
- Intel Media SDK HEVC decoder
- libde265 (if it supports multi-layer)
- Hardware decoder (if available)

---

## Patch Development Checklist

### Decoder Patches

- [ ] Extract encoded file and analyze bitstream structure
- [ ] Identify profile_idc value from VPS/SPS
- [ ] Add profile to `libavcodec/profiles.c:ff_hevc_profiles[]`
- [ ] Add `AV_PROFILE_HEVC_*` constant to `libavcodec/defs.h`
- [ ] Modify `libavcodec/hevc/ps.c` to accept new profile
- [ ] Implement alpha layer NAL parsing in `libavcodec/hevc/hevcdec.c`
- [ ] Add alpha plane to decoded AVFrame
- [ ] Test decoding: `ffmpeg -i input.mp4 -f null -`
- [ ] Test playback: `ffplay input.mp4`

### Demuxer Patches (if needed)

- [ ] Modify `libavformat/hevc.c:hvcc_add_nal_unit()` to handle alpha NALs
- [ ] Update MOV demuxer to expose alpha metadata
- [ ] Test demuxing: `ffprobe -show_streams input.mp4`

### Integration Testing

- [ ] Encode test file: ProRes 4444 → HEVC+alpha
- [ ] Decode test file: HEVC+alpha → ProRes 4444
- [ ] Verify alpha channel integrity (compare input vs output)
- [ ] Test with different resolutions and frame rates
- [ ] Test with different pixel formats (420, 422, 444)

---

## VP8/VP9 Alpha Implementation - Reference Model ✅

VP8/VP9 provides a **working example** of alpha channel support in FFmpeg that we can use as a blueprint for HEVC!

### How VP8/VP9 Alpha Works

#### Container Level (WebM/Matroska)
**File**: [libavformat/matroskadec.c](libavformat/matroskadec.c)

Alpha data is stored as **BlockAdditional** side data in Matroska:
- Line 532: `MATROSKA_ID_VIDEOALPHAMODE` signals alpha presence
- Line 777: `MATROSKA_ID_BLOCKADDITIONAL` contains alpha frame data
- Line 3961: Demuxer attaches alpha as `AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL`

```c
// Line 3025-3026: Export alpha mode to stream metadata
if (track->video.alpha_mode)
    av_dict_set_int(&st->metadata, "alpha_mode", 1, 0);

// Line 3961: Attach alpha data to packet
side_data = av_packet_new_side_data(pkt, AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL, ...);
```

#### Encoder Level (libvpx)
**File**: [libavcodec/libvpxenc.c](libavcodec/libvpxenc.c)

**Key Architecture** (lines 84-86, 93):
```c
typedef struct VPxEncoderContext {
    struct vpx_codec_ctx encoder;           // Main color encoder
    struct vpx_image rawimg;                // Main color image
    struct vpx_codec_ctx encoder_alpha;     // Separate alpha encoder!
    struct vpx_image rawimg_alpha;          // Separate alpha image!
    uint8_t is_alpha;                       // Alpha enabled flag
    struct FrameListData *coded_frame_list;       // Main frames
    struct FrameListData *alpha_coded_frame_list; // Alpha frames
} VPxEncoderContext;
```

**Alpha Encoding Flow**:
1. **Line 983**: Detect alpha plane → set `ctx->is_alpha = 1`
2. **Line 1176-1178**: Initialize **second encoder** for alpha channel
3. **Line 1721-1722**: Feed alpha plane (frame->data[3]) to alpha encoder
4. **Line 1844-1845**: Encode alpha separately: `vpx_codec_encode(&ctx->encoder_alpha, rawimg_alpha, ...)`
5. **Line 1855**: Queue alpha frames to separate list

#### Decoder Level (libvpx)
**File**: [libavcodec/libvpxdec.c](libavcodec/libvpxdec.c)

**Key Architecture** (lines 43-49):
```c
typedef struct VPxDecoderContext {
    struct vpx_codec_ctx decoder;        // Main color decoder
    struct vpx_codec_ctx decoder_alpha;  // Separate alpha decoder!
    int has_alpha_channel;               // Alpha present flag
} VPxContext;
```

**Alpha Decoding Flow**:
1. **Line 228**: Decode main frame from packet data
2. **Lines 232-238**: Check for `AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL` side data
3. **Line 239**: If `additional_id == 1` → alpha channel data detected
4. **Lines 240-254**: Initialize second decoder for alpha if not already done
5. **Line 256-257**: Decode alpha frame separately
6. **Lines 263-265**: Wait for BOTH decoders to output frames
7. **Lines 300-309**: Merge results:
   - planes[0-2] = Y, U, V from main decoder
   - planes[3] = Y plane from alpha decoder (grayscale alpha)
   - Set pixel format to `AV_PIX_FMT_YUVA420P` (line 132)
8. **Lines 323-326**: Copy merged planes to output AVFrame

**Critical Code** (lines 300-309):
```c
planes[0] = img->planes[VPX_PLANE_Y];           // Y from color
planes[1] = img->planes[VPX_PLANE_U];           // U from color
planes[2] = img->planes[VPX_PLANE_V];           // V from color
planes[3] = ctx->has_alpha_channel ?
            img_alpha->planes[VPX_PLANE_Y] : NULL;  // Alpha as grayscale
```

### Key Differences: VP9 vs HEVC Alpha

| Aspect | VP9 (Working ✅) | HEVC (Broken ❌) |
|--------|------------------|------------------|
| **Container** | WebM/Matroska BlockAdditional | MP4 multi-layer (interleaved NALs) |
| **Encoding** | Two separate VP9 encoders | Single x265 encoder, two layers |
| **Bitstream** | Two separate bitstreams | Single multi-layer bitstream |
| **Muxing** | Alpha as packet side data | Alpha NALs tagged with nuh_layer_id |
| **Demuxing** | Side data extraction works | Layer separation not implemented |
| **Decoding** | Two separate decoders | Single decoder (doesn't support multi-layer) |
| **Merging** | Manual plane copy | Not implemented |

### Why VP9 Approach Won't Work Directly for HEVC

VP9 uses **container-level separation**:
- Color and alpha are **completely separate streams** in the container
- Decoder doesn't need to understand alpha - container handles it
- Simple "decode twice, merge once" approach

HEVC uses **codec-level interleaving**:
- Color and alpha are **interleaved NAL units** in single stream
- Decoder MUST understand layer structure to separate them
- Can't "decode twice" without first splitting layers

### Adaptation Strategy for HEVC

We have **two options**:

#### Option A: Container-Level (VP9-style) - EASIER
**Pros**: Minimal decoder changes, reuse VP9 pattern
**Cons**: Non-standard, breaks compatibility with other HEVC decoders

1. MP4 demuxer splits layers → create side data like VP9
2. HEVC decoder modified to accept pre-split layers
3. Two decode calls, merge like VP9

#### Option B: Decoder-Level (Proper) - HARDER
**Pros**: Standard-compliant, proper HEVC support
**Cons**: Requires deep decoder changes

1. HEVC decoder parses VPS to understand layer structure
2. Decoder internally splits NALs by `nuh_layer_id`
3. Decoder manages two decoding contexts internally
4. Outputs merged YUVA frame directly

**Recommendation**: Start with **Option B** (proper decoder support) because:
- HEVC multi-layer is a standard (ITU-T H.265 Annex F)
- Other tools may generate similar streams
- More maintainable long-term

---

## References

### HEVC Specifications
- **ITU-T H.265**: HEVC base specification
- **ITU-T H.265 Annex F**: Multi-layer extensions (for scalability)
- **ITU-T H.265 Annex I**: 3D and auxiliary video (includes alpha)

### x265 Documentation
- x265 source: `C:\ff\ff\x265_4.1\source`
- Alpha encoding: `x265_4.1/source/encoder/encoder.cpp` (search `ENABLE_ALPHA`)
- CLI reference: `x265 --fullhelp` (if built with alpha support)

### FFmpeg Code References

#### HEVC (Needs Fixing)
- Encoder wrapper: [libavcodec/libx265.c](libavcodec/libx265.c)
- Demuxer support: [libavformat/hevc.c](libavformat/hevc.c)
- Decoder: [libavcodec/hevc/hevcdec.c](libavcodec/hevc/hevcdec.c)
- Profile parsing: [libavcodec/hevc/ps.c](libavcodec/hevc/ps.c)
- Profile definitions: [libavcodec/profiles.c](libavcodec/profiles.c)

#### VP8/VP9 (Working Reference)
- Encoder wrapper: [libavcodec/libvpxenc.c](libavcodec/libvpxenc.c) ⭐
- Decoder: [libavcodec/libvpxdec.c](libavcodec/libvpxdec.c) ⭐
- Demuxer: [libavformat/matroskadec.c](libavformat/matroskadec.c) ⭐

### Similar Formats
- **x264 with alpha**: Not supported (H.264 has no standard alpha extension)
- **VP9 with alpha**: ✅ **Working** - separate streams in WebM container
- **ProRes with alpha**: ✅ Working (4444/4444XQ profiles)
- **FFV1 with alpha**: ✅ Supported via YUVA pixel formats
- **AV1 with alpha**: Uses Film Grain Synthesis for alpha (experimental)

---

## Expected Behavior After Fix

### Encoding (already works)
```bash
$ ffmpeg -i input_with_alpha.mov -c:v libx265 -crf 22 -tag:v hvc1 output.mp4
x265 [info]: Main 10 profile, Level-4 (Main tier)
x265 [info]: tools: ... alpha
encoded 282 frames, 2177.96 kb/s  ← Base layer
encoded 282 frames, 471.05 kb/s   ← Alpha layer
```

### Decoding (should work after fix)
```bash
$ ffprobe output.mp4
Stream #0:0: Video: hevc (Main 10), yuva420p10le, 1024x1024
                                      ^^^ Alpha present!

$ ffplay output.mp4
# Should display video with transparent background/alpha channel
```

### Transcoding Round-trip
```bash
# Should preserve alpha without loss
ffmpeg -i input.mov -c:v libx265 -crf 18 temp.mp4
ffmpeg -i temp.mp4 -c:v prores_ks -profile:v 4444 output.mov
# Compare input.mov vs output.mov - alpha should match
```

---

## Build Configuration

### x265 Build (COMPLETED ✅)
```bash
./build-msys-x265-with-alpha.sh
```

Configuration:
- `ENABLE_ALPHA=ON`
- `HIGH_BIT_DEPTH=ON`
- `ENABLE_SHARED=ON`

Installed to: `/mingw64` (MSYS2)

### FFmpeg Build
```bash
./build-msys-shared.sh
```

Configuration includes:
- `--enable-libx265`
- `--enable-gpl --enable-version3 --enable-nonfree`

Current version: `aea6a75d12 "Alternative Development Edition"`

---

## Notes

- The "Multiple scalability types presented" warning suggests x265 may be using **both** Multiview AND Auxiliary scalability types
- The comment at [ps.c:544](libavcodec/hevc/ps.c#L544) says: `"x265 specify MULTIVIEW when the stream really is alpha video only"` - this is a known x265 quirk
- FFmpeg's current multi-layer support is marked `AVERROR_PATCHWELCOME` (contributions wanted)
- Consider contributing patches upstream to FFmpeg after testing

---

## Implementation Roadmap (Based on VP9 Pattern)

### Phase 1: Basic Multi-Layer Decoder Support

**Goal**: Make HEVC decoder recognize and parse multi-layer streams

**Files to Modify**:
1. `libavcodec/hevc/hevcdec.c` - Add second decoder context for alpha
2. `libavcodec/hevc/hevc.h` - Add alpha decoder structures
3. `libavcodec/hevc/ps.c` - Parse VPS layer information

**Code Pattern** (from VP9):
```c
// In hevcdec.c, add to HEVCContext:
typedef struct HEVCContext {
    // ... existing fields ...
    HEVCLayerContext *layers[MAX_LAYERS];  // Support multiple layers
    int has_alpha_layer;                    // Flag for alpha presence
    int alpha_layer_id;                     // Alpha layer nuh_layer_id
} HEVCContext;
```

### Phase 2: Layer Separation

**Goal**: Split incoming NAL units by `nuh_layer_id`

**Algorithm**:
```c
// Pseudocode for NAL splitting
int hevc_decode_frame(AVCodecContext *avctx, AVPacket *pkt) {
    HEVCContext *s = avctx->priv_data;

    // Parse all NALs in packet
    while (has_more_nals(pkt)) {
        HEVCNAL *nal = parse_next_nal(pkt);
        int layer_id = nal->nuh_layer_id;

        if (layer_id == 0) {
            // Base layer (color) - feed to main decoder
            decode_nal(&s->layers[0]->decoder, nal);
        } else if (s->has_alpha_layer && layer_id == s->alpha_layer_id) {
            // Alpha layer - feed to alpha decoder
            decode_nal(&s->layers[layer_id]->decoder, nal);
        }
    }

    // Merge decoded frames like VP9 does
    return merge_layers(s, output_frame);
}
```

### Phase 3: Frame Merging (Copy from VP9)

**Goal**: Combine base layer + alpha layer into YUVA frame

**Direct Copy from** [libavcodec/libvpxdec.c:300-326](libavcodec/libvpxdec.c#L300-L326):
```c
// After both layers decoded successfully:
AVFrame *base_frame = s->layers[0]->decoded_frame;
AVFrame *alpha_frame = s->layers[alpha_layer_id]->decoded_frame;

// Allocate YUVA frame
output_frame->format = AV_PIX_FMT_YUVA420P10LE;  // Match base + alpha
ff_get_buffer(avctx, output_frame, 0);

// Copy base layer
output_frame->data[0] = base_frame->data[0];  // Y
output_frame->data[1] = base_frame->data[1];  // U
output_frame->data[2] = base_frame->data[2];  // V
output_frame->linesize[0] = base_frame->linesize[0];
output_frame->linesize[1] = base_frame->linesize[1];
output_frame->linesize[2] = base_frame->linesize[2];

// Copy alpha layer (grayscale Y plane becomes A plane)
output_frame->data[3] = alpha_frame->data[0];  // Y → A
output_frame->linesize[3] = alpha_frame->linesize[0];
```

### Phase 4: Profile Support

**Goal**: Add multi-layer profile to profile list

**File**: `libavcodec/profiles.c`

```c
// Add after AV_PROFILE_HEVC_SCC:
{ AV_PROFILE_HEVC_MULTIVIEW_MAIN, "Multiview Main" },  // Already exists at line 102
// OR add new if x265 uses different profile:
{ AV_PROFILE_HEVC_SCALABLE_MAIN, "Scalable Main" },
```

**File**: `libavcodec/defs.h`

```c
// Add after existing HEVC profiles:
#define AV_PROFILE_HEVC_SCALABLE_MAIN 10  // Or whatever profile_idc x265 uses
```

### Phase 5: Testing

**Test Encoding**:
```bash
# Encode with alpha
ffmpeg -i input_with_alpha.mov -c:v libx265 -crf 22 -tag:v hvc1 output.mp4

# Check encoding
ffprobe -v trace output.mp4 2>&1 | grep -E "layer|profile"
```

**Test Decoding**:
```bash
# Should now show yuva420p10le (with alpha)
ffprobe output.mp4

# Decode to PNG with alpha
ffmpeg -i output.mp4 output_%04d.png

# Verify round-trip
ffmpeg -i input_with_alpha.mov -c:v libx265 -crf 18 temp.mp4
ffmpeg -i temp.mp4 -c:v prores_ks -profile:v 4444 output.mov
# Compare input vs output alpha
```

### Phase 6: MP4 Container Optimization (Optional)

If MP4 muxer needs changes:

**File**: `libavformat/movenc.c`

Add metadata to signal alpha layer:
```c
// Similar to VP9's alpha_mode in Matroska
if (track->has_alpha_layer) {
    // Write alpha layer indication to MP4 metadata
    // This helps other decoders identify alpha support
}
```

---

## Code Snippets for Quick Start

### 1. Detecting Alpha from VPS (Already Exists!)

**File**: [libavformat/hevc.c:430-431](libavformat/hevc.c#L430-L431)

```c
// This code already exists - we just need to USE it!
if (dimension_id == 1 /* AUX_ALPHA */ && scalability_mask_flag[j] == 3 /* AuxId */)
    hvcc->alpha_layer_nuh_id = layer_id_in_nuh[i];
```

### 2. Adding Alpha Decoder Context

**File**: `libavcodec/hevc/hevc.h` (add to HEVCContext)

```c
// Copy from VP9 pattern
typedef struct HEVCContext {
    // ... existing fields ...

    // Alpha support (following VP9 pattern)
    struct AVCodecContext *alpha_avctx;    // Separate context for alpha
    struct AVFrame *alpha_frame;           // Alpha decoded frame
    int has_alpha_channel;                 // Alpha present flag
    int alpha_nuh_layer_id;               // Which layer ID is alpha
} HEVCContext;
```

### 3. NAL Unit Layer Filtering

**File**: `libavcodec/hevc/hevcdec.c` (in decode_nal_units())

```c
// Before processing NAL:
int layer_id = nal->nuh_layer_id;

if (layer_id == 0) {
    // Base layer - process normally
    ret = decode_nal_unit(s, nal);
} else if (s->has_alpha_channel && layer_id == s->alpha_nuh_layer_id) {
    // Alpha layer - process with alpha context
    ret = decode_nal_unit_alpha(s, nal);
} else {
    // Unknown layer - skip
    av_log(s->avctx, AV_LOG_DEBUG, "Skipping layer %d\n", layer_id);
}
```

### 4. Pixel Format Selection with Alpha

**File**: `libavcodec/hevc/hevcdec.c` (in get_format())

```c
// Modify pixel format selection to include alpha variants
static enum AVPixelFormat get_format(HEVCContext *s, const HEVCSPS *sps) {
    // ... existing bit depth checks ...

    if (s->has_alpha_channel) {
        if (sps->bit_depth == 8)
            return AV_PIX_FMT_YUVA420P;
        else if (sps->bit_depth == 10)
            return AV_PIX_FMT_YUVA420P10LE;
        else if (sps->bit_depth == 12)
            return AV_PIX_FMT_YUVA420P12LE;
    }

    // ... existing non-alpha formats ...
}
```

---

## Verifying Your Encoded File Has Alpha

Your test file **DOES** contain alpha data! Here's how to verify:

### 1. Check Encoding Statistics (Already Confirmed ✅)
```
x265 [info]: encoded 282 frames, 2177.96 kb/s, Avg QP:26.98  ← Base layer (2.1 Mbps)
x265 [info]: encoded 282 frames, 471.05 kb/s, Avg QP:28.73   ← Alpha layer (471 kbps)
```
Two sets of statistics = Two layers encoded!

### 2. Extract and Analyze Bitstream
```bash
# Extract raw HEVC from MP4
ffmpeg -i J:\test\adGrid_checker_1k__@22.mp4 -c:v copy -bsf:v hevc_mp4toannexb -f hevc output.hevc

# Search for NAL units with nuh_layer_id > 0 (alpha layer)
# NAL header format: 0x00 0x00 0x01 [byte1] [byte2]
# byte1 bits 1-6 = NAL type
# byte1 bit 7 + byte2 bits 0-5 = nuh_layer_id (6 bits)

# Quick hex check for multi-layer NALs
xxd output.hevc | grep -E "0000 01 [4-7][0-9a-f] [1-9a-f]"
# Any matches show nuh_layer_id > 0 (alpha NALs present)
```

### 3. Parse VPS for Alpha Layer ID
```bash
# Use ffprobe to dump VPS details
ffprobe -v trace -i J:\test\adGrid_checker_1k__@22.mp4 2>&1 | tee ffprobe_trace.log

# Look for:
# - "Multiple scalability types" (we see this)
# - scalability_mask_flag values
# - dimension_id values
# - layer_id_in_nuh array

grep -E "scalability|dimension|layer.*id" ffprobe_trace.log
```

### 4. Confirm Profile Incompatibility
```bash
# Check what profile x265 used
ffprobe -show_streams -select_streams v:0 J:\test\adGrid_checker_1k__@22.mp4 2>&1 | grep profile

# Current output: "profile=Main 10"
# But with alpha, it should be "profile=Multiview Main" or similar
# This mismatch causes "Unknown profile bitstream" warning
```

### 5. Compare File Sizes
```bash
# Your file with alpha: ~3.6 MB for 11 seconds
# Expected without alpha: ~2.9 MB (base layer only)
# The extra ~700 KB is the alpha layer!

ls -lh J:\test\adGrid_checker_1k__@22.mp4
# Should show: 3657KiB or similar
```

---

## Quick Debug Commands

```bash
# Check x265 alpha support
/mingw64/bin/x265 --version
/mingw64/bin/x265 --fullhelp | grep -i alpha

# Check encoded file profile and layers
ffprobe -v trace J:\test\adGrid_checker_1k__@22.mp4 2>&1 | grep -i "alpha\|layer\|profile"

# Extract raw HEVC bitstream
ffmpeg -i J:\test\adGrid_checker_1k__@22.mp4 -c:v copy -bsf:v hevc_mp4toannexb -f hevc output.hevc

# Analyze NAL unit structure (shows layer IDs)
ffmpeg -i output.hevc -bsf:v trace_headers -f null - 2>&1 | less

# Check file size (alpha adds ~20-25% overhead)
ls -lh J:\test\adGrid_checker_1k__@22.mp4

# Try forcing alpha pixel format on decode (will fail but shows intention)
ffmpeg -i J:\test\adGrid_checker_1k__@22.mp4 -pix_fmt yuva420p10le output.png

# Check libx265 features in FFmpeg
ffmpeg -h encoder=libx265 | grep -i alpha

# Verify x265 alpha encoding flag was used
ffmpeg -i J:/test/adGrid_checker_1k.mov -c:v libx265 -crf 22 -x265-params log-level=full:alpha=1 -f null - 2>&1 | grep -i alpha
```

---

## Next Steps for YOU

1. **Verify alpha presence** using commands above
2. **Extract bitstream** to confirm multi-layer NAL units
3. **Save this README** for when you're ready to implement decoder support
4. **Consider contributing patches** to FFmpeg once working

When you're ready to implement, start with Phase 1 in the Implementation Roadmap!

**Estimated effort**:
- Phase 1-3 (Basic decoder): ~2-3 days for experienced FFmpeg developer
- Phase 4 (Profile): ~1 hour
- Phase 5 (Testing): ~1 day
- **Total**: ~1 week of focused development

---

## Contact / Further Help

- FFmpeg mailing list: ffmpeg-devel@ffmpeg.org
- x265 discussions: https://bitbucket.org/multicoreware/x265_git/issues
- HEVC spec: https://www.itu.int/rec/T-REC-H.265

---

*Last updated: 2026-01-29*
