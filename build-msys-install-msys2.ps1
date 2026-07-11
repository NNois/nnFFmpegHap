#Requires -Version 5.1
<#
.SYNOPSIS
    Bootstrap MSYS2 itself on a fresh Windows PC, so the repo is ready for
    build-msys-install-dependencies.sh + build-msys-shared.sh.

.DESCRIPTION
    Run this FIRST, from a normal Windows PowerShell (not MSYS2), elevated
    (Run as Administrator) if Chocolatey is not yet installed.

    Installs MSYS2 via Chocolatey to the default Chocolatey tools location
    (C:\tools\msys64), matching nnWork/msys2-terminal-config.json, then runs
    the initial pacman core update.

    Also configures VS Code's global User settings so the MSYS2 MINGW64 bash
    becomes its default integrated terminal everywhere, mirroring this repo's
    own .vscode/settings.json.

    After this script finishes, open "MSYS2 MINGW64" and run:
        ./build-msys-install-dependencies.sh
        ./build-msys-shared.sh
#>

$ErrorActionPreference = 'Stop'

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p  = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

Write-Host "=========================================="
Write-Host "FFmpeg -  ♥♥ Alternative Development Broadcast Edition ♥♥ - 8.1.2"
Write-Host "MSYS2 Bootstrap"
Write-Host "=========================================="
Write-Host ""

# --- Step 1: Chocolatey ---
if (Get-Command choco -ErrorAction SilentlyContinue) {
    Write-Host "Step 1: Chocolatey already installed, skipping."
} else {
    Write-Host "Step 1: Installing Chocolatey..."
    if (-not (Test-Admin)) {
        throw "Chocolatey is not installed and this shell is not elevated. Re-run this script from an Administrator PowerShell."
    }
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    # choco.exe is added to PATH by the installer but not to this process's env
    $env:Path = "$env:Path;C:\ProgramData\chocolatey\bin"
}

# --- Step 2: MSYS2 (default choco tools location: C:\tools\msys64) ---
$Msys2Root = 'C:\tools\msys64'   # Chocolatey's Get-ToolsLocation default; matches nnWork/msys2-terminal-config.json

if (Test-Path "$Msys2Root\msys2_shell.cmd") {
    Write-Host "Step 2: MSYS2 already installed at $Msys2Root, skipping."
} else {
    Write-Host "Step 2: Installing MSYS2 via Chocolatey (defaults to $Msys2Root)..."
    if (-not (Test-Admin)) {
        throw "Installing packages with Chocolatey requires an elevated PowerShell. Re-run as Administrator."
    }
    choco install msys2 -y --params "/NoUpdate"
    if (-not (Test-Path "$Msys2Root\msys2_shell.cmd")) {
        throw "MSYS2 was installed but not found at expected path $Msys2Root. Check the choco install output above and adjust `$Msys2Root in this script if Chocolatey used a different tools location."
    }
}

# --- Step 3: initial pacman core update (two passes: MSYS2 replaces its own runtime mid-update) ---
Write-Host ""
Write-Host "Step 3: Updating MSYS2 core packages (pass 1/2)..."
& "$Msys2Root\usr\bin\bash.exe" -lc "pacman -Syu --noconfirm"

Write-Host "Step 3: Updating MSYS2 core packages (pass 2/2)..."
& "$Msys2Root\usr\bin\bash.exe" -lc "pacman -Su --noconfirm"

# --- Step 4: VS Code default terminal (global User settings, mirrors .vscode/settings.json) ---
Write-Host ""
Write-Host "Step 4: Configuring VS Code default terminal (MSYS2 MINGW64)..."

$VSCodeUserDir = @(
    "$env:APPDATA\Code\User",
    "$env:APPDATA\Code - Insiders\User"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $VSCodeUserDir) {
    Write-Host "  VS Code user profile not found, skipping (install VS Code first if you want this)."
} else {
    $SettingsPath = Join-Path $VSCodeUserDir 'settings.json'

    $Msys2Profile = [ordered]@{
        path = "$Msys2Root\usr\bin\bash.exe"
        args = @('--login', '-i')
        env  = [ordered]@{ MSYSTEM = 'MINGW64'; CHERE_INVOKING = '1' }
        icon = 'terminal-bash'
    }

    $existed = Test-Path $SettingsPath
    $settings = [PSCustomObject]@{}
    if ($existed) {
        try {
            $settings = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        } catch {
            $settings = $null
        }
    }

    if ($null -eq $settings) {
        Write-Host "  WARNING: $SettingsPath has comments/trailing commas and could not be parsed automatically."
        Write-Host "  Add this manually under 'terminal.integrated.profiles.windows' and set"
        Write-Host "  'terminal.integrated.defaultProfile.windows' to 'MSYS2 MINGW64':"
        Write-Host ('  "MSYS2 MINGW64": ' + ($Msys2Profile | ConvertTo-Json -Depth 5))
    } else {
        if ($existed) {
            $backupPath = "$SettingsPath.bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
            Copy-Item $SettingsPath $backupPath
            Write-Host "  Backed up existing settings to $backupPath"
        }

        if (-not $settings.PSObject.Properties['terminal.integrated.profiles.windows']) {
            $settings | Add-Member -NotePropertyName 'terminal.integrated.profiles.windows' -NotePropertyValue ([PSCustomObject]@{}) -Force
        }
        $settings.'terminal.integrated.profiles.windows' | Add-Member -NotePropertyName 'MSYS2 MINGW64' -NotePropertyValue $Msys2Profile -Force

        $settings | Add-Member -NotePropertyName 'terminal.integrated.defaultProfile.windows' -NotePropertyValue 'MSYS2 MINGW64' -Force
        $settings | Add-Member -NotePropertyName 'terminal.integrated.automationProfile.windows' -NotePropertyValue $Msys2Profile -Force

        ($settings | ConvertTo-Json -Depth 10) | Set-Content -Path $SettingsPath -Encoding UTF8
        Write-Host "  Updated $SettingsPath"
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "MSYS2 ready at $Msys2Root"
Write-Host "=========================================="
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1) Open 'MSYS2 MINGW64' (or run: $Msys2Root\msys2_shell.cmd -mingw64)"
Write-Host "     - or just open VS Code: its integrated terminal now defaults to it"
Write-Host "  2) cd into this repo"
Write-Host "  3) ./build-msys-install-dependencies.sh"
Write-Host "  4) ./build-msys-shared.sh"
Write-Host ""
Write-Host "Optional: merge nnWork/msys2-terminal-config.json's profiles into your"
Write-Host "Windows Terminal settings.json to get the MSYS2 MINGW64/UCRT64/MSYS profiles there too."
Write-Host ""
