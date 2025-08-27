# Debug-focused rebuild script
# Forces rebuild of debugger-related files and shows debug output

Write-Host "QuickJS Debugger Rebuild" -ForegroundColor Green
Write-Host "========================" -ForegroundColor Green

$BuildDir = "build"

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Host "Build directory doesn't exist. Run .\build.ps1 first." -ForegroundColor Red
    exit 1
}

# Force rebuild debugger files by touching them
Write-Host "Forcing rebuild of debugger files..." -ForegroundColor Yellow
$DebuggerFiles = @(
    "quickjs-debugger.c",
    "quickjs-debugger-transport-win.c",
    "quickjs.c"  # Contains debugger integration
)

foreach ($file in $DebuggerFiles) {
    if (Test-Path $file) {
        (Get-Item $file).LastWriteTime = Get-Date
        Write-Host "  → Touched $file" -ForegroundColor Gray
    }
}

# Build with timing
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$BuildArgs = @(
    "--build", $BuildDir,
    "--config", "Release", 
    "-j", [Environment]::ProcessorCount
)

try {
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    
    $stopwatch.Stop()
    Write-Host "✓ Debugger rebuild completed in $($stopwatch.Elapsed.TotalSeconds.ToString('F1')) seconds!" -ForegroundColor Green
    
    # Test debugger functionality
    Write-Host ""
    Write-Host "Testing debugger functionality..." -ForegroundColor Yellow
    
    $qjsPath = "build\Release\qjs.exe"
    if (Test-Path $qjsPath) {
        Write-Host "→ $qjsPath ready" -ForegroundColor Cyan
        
        # Test basic execution
        Write-Host "→ Testing basic execution..." -ForegroundColor Gray
        & $qjsPath -e "console.log('QuickJS working!')"
        
        # Test debugger environment detection
        Write-Host "→ Testing debugger detection..." -ForegroundColor Gray
        $env:QUICKJS_DEBUG_LISTEN_ADDRESS = "127.0.0.1:64960"
        $testProcess = Start-Process -FilePath $qjsPath -ArgumentList "test.js" -PassThru -NoNewWindow
        Start-Sleep -Seconds 2
        if (!$testProcess.HasExited) {
            Write-Host "✓ Debugger is waiting for connection (process hanging as expected)" -ForegroundColor Green
            $testProcess.Kill()
        } else {
            Write-Host "⚠ Debugger might not be working (process exited immediately)" -ForegroundColor Yellow
        }
        Remove-Item Env:QUICKJS_DEBUG_LISTEN_ADDRESS
    }
    
} catch {
    $stopwatch.Stop()
    Write-Host "✗ Build failed after $($stopwatch.Elapsed.TotalSeconds.ToString('F1')) seconds: $_" -ForegroundColor Red
    exit 1
}
