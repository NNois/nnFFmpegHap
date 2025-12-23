# VSCode Configuration for FFmpeg Build

This directory contains VSCode workspace configuration for building FFmpeg with HAP codec support.

## Terminal Setup

The integrated terminal is configured to automatically use **MSYS2 MINGW64** bash instead of PowerShell/CMD.

- **Default terminal**: MSYS2 MINGW64 (bash)
- **Location**: `C:\tools\msys64\usr\bin\bash.exe`
- **Environment**: `MINGW64` (for native Windows builds)

When you open a new terminal in VSCode (`Ctrl+` `` ` ``), it will automatically start in MSYS2 MINGW64 mode.

## Build Tasks

Press `Ctrl+Shift+B` to access build tasks, or use `Terminal > Run Task...`:

| Task | Description | Shortcut |
|------|-------------|----------|
| **Build FFmpeg (Shared)** | Builds FFmpeg shared libraries (.dll) | `Ctrl+Shift+B` (default) |
| Build FFmpeg (Static) | Builds FFmpeg static libraries (.a) | - |
| Copy FFmpeg DLLs (Shared) | Deploys shared build, asks to build mpv | - |
| Copy FFmpeg DLLs (Static) | Deploys static build, asks to build mpv | - |
| **Full Build Shared** | Build → Deploy (+ optional mpv build) | - |
| **Full Build Static** | Build → Deploy (+ optional mpv build) | - |
| Apply HAP Patches | Applies HAP codec patches to source | - |
| Verify Build | Verifies build configuration | - |
| Install FFmpeg Dependencies | Installs MSYS2 packages | - |
| Configure FFmpeg | Shows FFmpeg configuration options | - |
| Test FFmpeg HAP Support | Checks HAP encoder/decoder availability | - |

## Directory Structure

```
.vscode/
├── settings.json      # Terminal and editor settings
├── tasks.json         # Build task definitions
├── extensions.json    # Recommended VSCode extensions
└── README.md          # This file
```

## Recommended Extensions

VSCode will prompt you to install recommended extensions when you open this workspace:

- **C/C++ Tools** - Code navigation and IntelliSense
- **ShellCheck** - Bash script linting
- **Shell Format** - Auto-format shell scripts
- **Makefile Tools** - Support for configure/make build system
- **GitLens** - Enhanced Git integration
- **x86-64 Assembly** - Syntax highlighting for FFmpeg asm code

## Workflow

### Initial Setup
1. Run `Install FFmpeg Dependencies` task first
2. Run `Apply HAP Patches` if working with HAP codec
3. Run `Configure FFmpeg` to see available options

### Development Build Cycle
1. Make code changes
2. Press `Ctrl+Shift+B` to build (shared by default)
3. Run `Test FFmpeg HAP Support` to verify
4. Run `Copy FFmpeg DLLs (Shared)` to deploy
   - Script will prompt: "Do you want to build mpv now?"
   - Answer `y` to automatically build mpv with updated FFmpeg
   - Answer `n` to skip mpv build

### Full Deployment
Use `Full Build Shared` or `Full Build Static` for complete build + deployment.
The copy scripts will prompt you to build mpv after deploying FFmpeg.

## Customization

### Change MSYS2 Installation Path

If your MSYS2 is installed elsewhere, edit [settings.json](settings.json):

```json
{
    "terminal.integrated.profiles.windows": {
        "MSYS2 MINGW64": {
            "path": "C:\\your\\path\\to\\msys64\\usr\\bin\\bash.exe",
            ...
        }
    }
}
```

### Add More Build Tasks

Edit [tasks.json](tasks.json) to add custom build tasks.

## Troubleshooting

**Terminal doesn't start in MSYS2:**
1. Reload VSCode (`Ctrl+Shift+P` → "Reload Window")
2. Verify MSYS2 path in `settings.json`
3. Check that `C:\tools\msys64\usr\bin\bash.exe` exists

**Build tasks don't work:**
1. Ensure scripts are executable: `chmod +x *.sh`
2. Make sure you're in the workspace directory
3. Check that MSYS2 packages are installed

**Configure errors:**
- Run the "Install FFmpeg Dependencies" task first
- Or manually: `pacman -S mingw-w64-x86_64-toolchain`

**HAP codec not found:**
- Ensure HAP patches are applied: Run `Apply HAP Patches` task
- Check branch: Should be on `HapWork` branch
- Verify in `libavcodec/hapenc.c` and `hapdec.c`
