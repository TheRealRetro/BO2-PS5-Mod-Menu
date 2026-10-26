# BO2 Zombies Mod Menu - PS5 Deployment Script
# Copies the DeadOps folder structure to a USB drive or FTP target
#
# Usage:
#   .\deploy.ps1 -Verify
#   .\deploy.ps1 -Target USB -Drive E
#   .\deploy.ps1 -Target FTP -Host_ 192.168.1.X

param(
    [ValidateSet("USB", "FTP", "")]
    [string]$Target = "",
    [string]$Drive = "",
    [string]$Host_ = "",
    [int]$Port = 2121,
    [switch]$Verify
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DeployDir = Join-Path $ScriptDir "deploy"
$SourceFile = Join-Path $DeployDir "DeadOps\t6_zm\_clientids.gsc"

function Write-Status($msg, $color) {
    Write-Host "  [$color] " -NoNewline
    Write-Host $msg -ForegroundColor $color
}

Write-Host ""
Write-Host "=== BO2 Mod Menu Deployment ===" -ForegroundColor Cyan
Write-Host ""

if (Test-Path $SourceFile) {
    $size = (Get-Item $SourceFile).Length
    Write-Status "_clientids.gsc found ($size bytes)" "Green"
} else {
    Write-Status "_clientids.gsc NOT FOUND at: $SourceFile" "Red"
    Write-Host "  Expected: deploy\DeadOps\t6_zm\_clientids.gsc" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Deploy folder:" -ForegroundColor White
Get-ChildItem -Path $DeployDir -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Replace($DeployDir, "").TrimStart("\")
    Write-Host "  $rel" -ForegroundColor Gray
}

Write-Host ""
Write-Host "PS5 target:" -ForegroundColor White
Write-Host "  /data/DeadOps/DeadOps.sprx          (loader)" -ForegroundColor DarkGray
Write-Host "  /data/DeadOps/t6_zm/_clientids.gsc   (mod menu)" -ForegroundColor Green

if ($Verify) {
    Write-Host ""
    Write-Host "Verification complete." -ForegroundColor Cyan
    Write-Host ""
    exit 0
}

if ($Target -eq "USB") {
    if (-not $Drive) {
        $Drive = Read-Host "Enter USB drive letter (e.g. E)"
    }
    $Drive = $Drive.TrimEnd(":\")
    $UsbDest = "${Drive}:\data\DeadOps\t6_zm"

    Write-Host ""
    Write-Host "Copying to USB ($UsbDest)..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $UsbDest -Force | Out-Null
    Copy-Item -Path $SourceFile -Destination $UsbDest -Force
    Write-Status "Copied _clientids.gsc to $UsbDest" "Green"

    Write-Host ""
    Write-Host "USB deployment complete." -ForegroundColor Cyan
    Write-Host "Next steps:" -ForegroundColor White
    Write-Host "  1. Plug USB into PS5" -ForegroundColor Gray
    Write-Host "  2. Copy /data/DeadOps/ to PS5 internal storage" -ForegroundColor Gray
    Write-Host "  3. Ensure DeadOps.sprx is at /data/DeadOps/" -ForegroundColor Gray
    Write-Host "  4. Launch BO2 with NexusFramework loading DeadOps" -ForegroundColor Gray
    Write-Host "  5. Start Zombies match, double-tap melee to open menu" -ForegroundColor Gray
    Write-Host ""
}

if ($Target -eq "FTP") {
    if (-not $Host_) {
        $Host_ = Read-Host "Enter PS5 IP address"
    }

    $ftpBase = "ftp://${Host_}:${Port}"
    $ftpDest = "$ftpBase/data/DeadOps/t6_zm/"

    Write-Host ""
    Write-Host "Uploading via FTP to $ftpDest ..." -ForegroundColor Yellow

    try {
        foreach ($dir in @("/data/", "/data/DeadOps/", "/data/DeadOps/t6_zm/")) {
            try {
                $req = [System.Net.FtpWebRequest]::Create("$ftpBase$dir")
                $req.Method = [System.Net.WebRequestMethods+Ftp]::MakeDirectory
                $req.Credentials = New-Object System.Net.NetworkCredential("anonymous", "")
                $resp = $req.GetResponse()
                $resp.Close()
            } catch {
                # directory may already exist
            }
        }

        $fileBytes = [System.IO.File]::ReadAllBytes($SourceFile)
        $req = [System.Net.FtpWebRequest]::Create("${ftpDest}_clientids.gsc")
        $req.Method = [System.Net.WebRequestMethods+Ftp]::UploadFile
        $req.Credentials = New-Object System.Net.NetworkCredential("anonymous", "")
        $req.ContentLength = $fileBytes.Length
        $req.UseBinary = $true
        $stream = $req.GetRequestStream()
        $stream.Write($fileBytes, 0, $fileBytes.Length)
        $stream.Close()
        $resp = $req.GetResponse()
        Write-Status "Uploaded _clientids.gsc ($($fileBytes.Length) bytes)" "Green"
        $resp.Close()

        Write-Host ""
        Write-Host "FTP deployment complete." -ForegroundColor Cyan
        Write-Host "Next steps:" -ForegroundColor White
        Write-Host "  1. Ensure DeadOps.sprx is at /data/DeadOps/" -ForegroundColor Gray
        Write-Host "  2. Launch BO2 with NexusFramework loading DeadOps" -ForegroundColor Gray
        Write-Host "  3. Start Zombies match, double-tap melee to open menu" -ForegroundColor Gray
        Write-Host ""
    }
    catch {
        Write-Status "FTP upload failed: $_" "Red"
        Write-Host "  Make sure PS5 FTP server is running on port $Port" -ForegroundColor Yellow
        Write-Host ""
        exit 1
    }
}

if (-not $Target -and -not $Verify) {
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor White
    Write-Host "  .\deploy.ps1 -Verify                          # check files" -ForegroundColor Gray
    Write-Host "  .\deploy.ps1 -Target USB -Drive E              # copy to USB" -ForegroundColor Gray
    Write-Host '  .\deploy.ps1 -Target FTP -Host_ 192.168.1.X    # upload to PS5' -ForegroundColor Gray
    Write-Host ""
}
