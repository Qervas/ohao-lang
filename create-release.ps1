# Ohao Lang Release Packager
# Creates a complete standalone release package with all dependencies

param(
    [string]$Version = "1.0.3",
    [switch]$CreateZip = $true
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Ohao Lang Release Packager v$Version" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Paths
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\Release"
$ReleaseDir = Join-Path $ProjectRoot "OhaoLang-Release"
$TesseractSource = "C:\Users\djmax\scoop\apps\tesseract\current"

# Check if build exists
if (-not (Test-Path $BuildDir\ohao-lang.exe)) {
    Write-Host "ERROR: Release build not found at $BuildDir\ohao-lang.exe" -ForegroundColor Red
    Write-Host "Please run build.sh first to create the Release build" -ForegroundColor Yellow
    exit 1
}

# Clean existing release directory
Write-Host "[1/8] Cleaning release directory..." -ForegroundColor Yellow
if (Test-Path $ReleaseDir) {
    Remove-Item $ReleaseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $ReleaseDir | Out-Null
Write-Host "      Created: $ReleaseDir" -ForegroundColor Green

# Copy main executable and Qt dependencies (already deployed by windeployqt)
Write-Host "[2/8] Copying application and Qt dependencies..." -ForegroundColor Yellow
Copy-Item "$BuildDir\*" -Destination $ReleaseDir -Recurse -Force
Write-Host "      Copied: ohao-lang.exe and all Qt DLLs/plugins" -ForegroundColor Green

# Copy Tesseract executables and DLLs
Write-Host "[3/8] Copying Tesseract OCR..." -ForegroundColor Yellow
$TesseractDir = Join-Path $ReleaseDir "tesseract"
New-Item -ItemType Directory -Path $TesseractDir -Force | Out-Null

if (Test-Path $TesseractSource) {
    # Copy all Tesseract executables
    Copy-Item "$TesseractSource\*.exe" -Destination $TesseractDir -Force
    # Copy all DLLs
    Copy-Item "$TesseractSource\*.dll" -Destination $TesseractDir -Force
    Write-Host "      Copied: Tesseract executables and DLLs" -ForegroundColor Green
} else {
    Write-Host "      WARNING: Tesseract not found at $TesseractSource" -ForegroundColor Yellow
    Write-Host "      Skipping Tesseract binaries..." -ForegroundColor Yellow
}

# Copy Tesseract language data
Write-Host "[4/8] Copying Tesseract language packs..." -ForegroundColor Yellow
$TessdataSource = "$TesseractSource\tessdata"
$TessdataTarget = Join-Path $TesseractDir "tessdata"

if (Test-Path $TessdataSource) {
    New-Item -ItemType Directory -Path $TessdataTarget -Force | Out-Null
    Copy-Item "$TessdataSource\*" -Destination $TessdataTarget -Recurse -Force

    # Count language packs
    $langCount = (Get-ChildItem "$TessdataTarget\*.traineddata" -File).Count
    $size = (Get-ChildItem $TessdataTarget -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
    Write-Host "      Copied: $langCount language packs (~$([math]::Round($size, 0))MB)" -ForegroundColor Green
} else {
    Write-Host "      WARNING: tessdata not found at $TessdataSource" -ForegroundColor Yellow
}

# Copy Edge-TTS setup script
Write-Host "[5/8] Copying Edge-TTS setup..." -ForegroundColor Yellow
$EdgeTTSDir = Join-Path $ReleaseDir "edge-tts"
New-Item -ItemType Directory -Path $EdgeTTSDir -Force | Out-Null

if (Test-Path "$ProjectRoot\setup_edge_tts.bat") {
    Copy-Item "$ProjectRoot\setup_edge_tts.bat" -Destination $ReleaseDir -Force
    Write-Host "      Copied: setup_edge_tts.bat" -ForegroundColor Green
} else {
    Write-Host "      WARNING: setup_edge_tts.bat not found" -ForegroundColor Yellow
}

# Copy resources
Write-Host "[6/8] Copying resources..." -ForegroundColor Yellow
if (Test-Path "$ProjectRoot\resources") {
    Copy-Item "$ProjectRoot\resources" -Destination $ReleaseDir -Recurse -Force

    # Count Hunspell dictionaries
    $dictCount = 0
    if (Test-Path "$ReleaseDir\resources\dictionaries") {
        $dictCount = (Get-ChildItem "$ReleaseDir\resources\dictionaries" -Directory).Count
    }

    Write-Host "      Copied: resources folder (test_sentences.json, $dictCount Hunspell dictionaries)" -ForegroundColor Green
} else {
    Write-Host "      WARNING: resources folder not found" -ForegroundColor Yellow
}

# Copy documentation
Write-Host "[7/8] Copying documentation..." -ForegroundColor Yellow
$Docs = @("README.md", "CHANGELOG.md", "LICENSE")
foreach ($doc in $Docs) {
    if (Test-Path "$ProjectRoot\$doc") {
        Copy-Item "$ProjectRoot\$doc" -Destination $ReleaseDir -Force
        Write-Host "      Copied: $doc" -ForegroundColor Green
    } else {
        Write-Host "      WARNING: $doc not found" -ForegroundColor Yellow
    }
}

# Calculate package size
Write-Host "[8/8] Calculating package size..." -ForegroundColor Yellow
$TotalSize = (Get-ChildItem $ReleaseDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host "      Total package size: $([math]::Round($TotalSize, 0))MB" -ForegroundColor Green

# Create ZIP archive
if ($CreateZip) {
    Write-Host ""
    Write-Host "Creating ZIP archive..." -ForegroundColor Yellow
    $ZipName = "OhaoLang-v$Version-Windows-x64.zip"
    $ZipPath = Join-Path $ProjectRoot $ZipName

    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }

    # Use .NET compression (faster than Compress-Archive for large files)
    Add-Type -Assembly "System.IO.Compression.FileSystem"
    [System.IO.Compression.ZipFile]::CreateFromDirectory($ReleaseDir, $ZipPath, "Optimal", $false)

    $ZipSize = (Get-Item $ZipPath).Length / 1MB
    Write-Host "      Created: $ZipName ($([math]::Round($ZipSize, 0))MB)" -ForegroundColor Green
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Release Package Created Successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Package Details:" -ForegroundColor White
Write-Host "  Version:        $Version" -ForegroundColor White
Write-Host "  Location:       $ReleaseDir" -ForegroundColor White
Write-Host "  Package Size:   $([math]::Round($TotalSize, 0))MB" -ForegroundColor White
if ($CreateZip) {
    Write-Host "  Archive:        $ZipName ($([math]::Round($ZipSize, 0))MB)" -ForegroundColor White
}
Write-Host ""
Write-Host "Contents:" -ForegroundColor White
Write-Host "  ✓ Ohao Lang Application (Release build, no console)" -ForegroundColor White
Write-Host "  ✓ Qt 6.9.2 DLLs and plugins" -ForegroundColor White
Write-Host "  ✓ Tesseract OCR with language packs" -ForegroundColor White
Write-Host "  ✓ Edge-TTS setup script" -ForegroundColor White
Write-Host "  ✓ Documentation (README, CHANGELOG, LICENSE)" -ForegroundColor White
Write-Host "  ✓ Resources (test_sentences.json)" -ForegroundColor White
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Test the release by running: $ReleaseDir\ohao-lang.exe" -ForegroundColor White
Write-Host "  2. Upload $ZipName to GitHub releases" -ForegroundColor White
Write-Host ""
