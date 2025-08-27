# QuickJS Build Script for Windows
# This script builds QuickJS using CMake with Visual Studio

param(
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Architecture = "x64",
    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Examples,
    [switch]$Static,
    [switch]$Debug,
    [switch]$Fast,
    [switch]$Help
)

function Show-Help {
    Write-Host "QuickJS Build Script" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -BuildType <type>     Build type: Release, Debug, RelWithDebInfo, MinSizeRel (default: Release)"
    Write-Host "  -Generator <gen>      CMake generator (default: 'Visual Studio 17 2022')"
    Write-Host "  -Architecture <arch>  Target architecture: x64, Win32, ARM64 (default: x64)"
    Write-Host "  -Clean               Clean build directory before building"
    Write-Host "  -Rebuild             Clean and rebuild everything"
    Write-Host "  -Examples            Build examples"
    Write-Host "  -Static              Build static executable"
    Write-Host "  -Debug               Build in Debug mode (shortcut for -BuildType Debug)"
    Write-Host "  -Fast                Skip CMake configure step (build only)"
    Write-Host "  -Help                Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1                    # Build Release version"
    Write-Host "  .\build.ps1 -Debug             # Build Debug version"
    Write-Host "  .\build.ps1 -Rebuild           # Clean rebuild"
    Write-Host "  .\build.ps1 -Examples -Static  # Build with examples and static linking"
    Write-Host "  .\build.ps1 -Fast              # Fast rebuild (skip configure)"
    Write-Host "  .\build.ps1 -Clean             # Clean build directory"
    exit 0
}

if ($Help) {
    Show-Help
}

# Handle Debug shortcut
if ($Debug) {
    $BuildType = "Debug"
}

Write-Host "QuickJS Build Script" -ForegroundColor Green
Write-Host "===================" -ForegroundColor Green
Write-Host ""

# Set build directory
$BuildDir = "build"

# Clean build directory if requested
if ($Clean -or $Rebuild) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Host "Build directory cleaned." -ForegroundColor Green
    }
    
    if ($Clean -and -not $Rebuild) {
        Write-Host "Clean completed." -ForegroundColor Green
        exit 0
    }
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "Created build directory." -ForegroundColor Green
}

# Prepare CMake options
$CMakeOptions = @()

if ($Examples) {
    $CMakeOptions += "-DQJS_BUILD_EXAMPLES=ON"
}

if ($Static) {
    $CMakeOptions += "-DQJS_BUILD_CLI_STATIC=ON"
}

# Configure step (skip if Fast mode and build directory exists)
if (-not $Fast -or -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    Write-Host "Configuring CMake..." -ForegroundColor Yellow
    Write-Host "Build Type: $BuildType" -ForegroundColor Cyan
    Write-Host "Generator: $Generator" -ForegroundColor Cyan
    Write-Host "Architecture: $Architecture" -ForegroundColor Cyan

    $ConfigureArgs = @(
        "-S", ".",
        "-B", $BuildDir,
        "-G", $Generator,
        "-A", $Architecture
    )

    if ($CMakeOptions.Count -gt 0) {
        $ConfigureArgs += $CMakeOptions
        Write-Host "CMake Options: $($CMakeOptions -join ' ')" -ForegroundColor Cyan
    }

    Write-Host ""
    Write-Host "Running: cmake $($ConfigureArgs -join ' ')" -ForegroundColor Gray

    try {
        & cmake @ConfigureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }
        Write-Host "CMake configuration completed successfully." -ForegroundColor Green
    } catch {
        Write-Host "Error during CMake configuration: $_" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Skipping CMake configuration (Fast mode)..." -ForegroundColor Yellow
}

# Build step
Write-Host ""
Write-Host "Building QuickJS..." -ForegroundColor Yellow

$BuildArgs = @(
    "--build", $BuildDir,
    "--config", $BuildType,
    "-j", [Environment]::ProcessorCount
)

Write-Host "Running: cmake $($BuildArgs -join ' ')" -ForegroundColor Gray

try {
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    Write-Host "Build completed successfully!" -ForegroundColor Green
} catch {
    Write-Host "Error during build: $_" -ForegroundColor Red
    exit 1
}

# Show results
Write-Host ""
Write-Host "Build Results:" -ForegroundColor Green
Write-Host "==============" -ForegroundColor Green

$OutputDir = if ($Generator -like "*Visual Studio*") { 
    Join-Path $BuildDir $BuildType 
} else { 
    $BuildDir 
}

$Executables = @("qjs.exe", "qjsc.exe", "run-test262.exe", "interrupt-test.exe", "function_source.exe")

foreach ($exe in $Executables) {
    $exePath = Join-Path $OutputDir $exe
    if (Test-Path $exePath) {
        $fileInfo = Get-Item $exePath
        Write-Host "✓ $exe" -ForegroundColor Green -NoNewline
        Write-Host " ($([math]::Round($fileInfo.Length / 1KB, 1)) KB)" -ForegroundColor Gray
    }
}

if ($Examples -and (Test-Path (Join-Path $OutputDir "hello.exe"))) {
    Write-Host "✓ Examples built" -ForegroundColor Green
}

Write-Host ""
Write-Host "Executables location: $OutputDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "Test your build:" -ForegroundColor Yellow
Write-Host "  $OutputDir\qjs.exe test.js" -ForegroundColor Gray
Write-Host "  $OutputDir\qjs.exe --help" -ForegroundColor Gray
