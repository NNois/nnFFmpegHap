FFmpeg submission notes (local)

FFmpeg does not use GitHub pull requests. Send patches to ffmpeg-devel using
git format-patch or git send-email.

Quick checklist:
- Keep commits focused and small.
- Run tests or at least build before sending.
- Mention third-party code origins and licenses in the cover letter or commit.
- Keep original license headers in imported files.

Basic workflow:
1) git checkout -b hap-pr
2) git commit -s -m "..."    (one logical change per commit)
3) git format-patch -o /tmp/ffmpeg-patches origin/master
4) git send-email /tmp/ffmpeg-patches/*.patch --to ffmpeg-devel@ffmpeg.org

Notes for this fork:
- bc7enc.c/.h are from https://github.com/richgel999/bc7enc (MIT or Public Domain).
- Keep the license header in those files.
