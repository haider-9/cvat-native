# Detached update helper for Windows. The Qt app starts this script and quits;
# the script waits for that process to exit before replacing its files.
param(
    [Parameter(Mandatory = $true)][string]$InstallDir,
    [Parameter(Mandatory = $true)][string]$ZipFile,
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [Parameter(Mandatory = $true)][string]$ExePath
)

$ErrorActionPreference = 'Stop'
function Write-UpdateLog([string]$Message) {
    Write-Host "[cvat-native updater] $Message"
}

try {
    for ($i = 0; $i -lt 60; $i++) {
        if ($null -eq (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Milliseconds 500
    }
    if ($null -ne (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) {
        throw 'CVAT Native is still running; update cancelled'
    }
    Start-Sleep -Seconds 1

    if (-not (Test-Path -LiteralPath $ZipFile -PathType Leaf)) {
        throw "Update archive not found: $ZipFile"
    }
    if (-not (Test-Path -LiteralPath $InstallDir -PathType Container)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }
    Expand-Archive -LiteralPath $ZipFile -DestinationPath $InstallDir -Force
    Remove-Item -LiteralPath $ZipFile -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        throw "Updated executable not found: $ExePath"
    }
    Start-Process -FilePath $ExePath
    Write-UpdateLog 'update applied; restarted CVAT Native'
}
catch {
    Write-UpdateLog $_.Exception.Message
    exit 1
}
