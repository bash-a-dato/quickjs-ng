# Quick rebuild script for development
# Skips all CMake configuration and just builds

param(
    [switch]$Force,
    [switch]$Help
)

function Show-Help {
    Write-Host "Quick QuickJS Rebuild" -ForegroundColor Green
    Write-Host "Usage: .\quick-build.ps1 [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -Force    Force rebuild of all source files (touch sources)"
    Write-Host "  -Help     Show this help message"
    exit 0
}

if ($Help) {
    Show-Help
}

Write-Host "Quick QuickJS Rebuild" -ForegroundColor Green
Write-Host "=====================" -ForegroundColor Green

$BuildDir = "build"

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Host "Build directory doesn't exist. Run .\build.ps1 first." -ForegroundColor Red
    exit 1
}

# Check if CMakeCache.txt exists
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    Write-Host "CMake not configured. Run .\build.ps1 first." -ForegroundColor Red
    exit 1
}

# Force rebuild if requested
if ($Force) {
    Write-Host "Force mode: Touching source files to trigger rebuild..." -ForegroundColor Yellow
    $SourceFiles = @(
        "quickjs.c",
        "quickjs-debugger.c", 
        "quickjs-debugger-transport-win.c",
        "quickjs-libc.c",
        "qjs.c"
    )
    
    foreach ($file in $SourceFiles) {
        if (Test-Path $file) {
            (Get-Item $file).LastWriteTime = Get-Date
            Write-Host "  → Touched $file" -ForegroundColor Gray
        }
    }
    Write-Host "Building all source files..." -ForegroundColor Yellow
} else {
    Write-Host "Building changed files only..." -ForegroundColor Yellow
}

$BuildArgs = @(
    "--build", $BuildDir,
    "--config", "Release",
    "-j", [Environment]::ProcessorCount
)

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    
    $stopwatch.Stop()
    Write-Host "✓ Quick build completed in $($stopwatch.Elapsed.TotalSeconds.ToString('F1')) seconds!" -ForegroundColor Green
    
    # Show qjs.exe location
    $qjsPath = "build\Release\qjs.exe"
    if (Test-Path $qjsPath) {
        Write-Host "→ $qjsPath ready for testing" -ForegroundColor Cyan
    }
    
} catch {
    $stopwatch.Stop()
    Write-Host "✗ Build failed after $($stopwatch.Elapsed.TotalSeconds.ToString('F1')) seconds: $_" -ForegroundColor Red
    exit 1
}
