# Quick build and run script
param(
    [switch]$Run
)

Write-Host "Building Blockchain Application..." -ForegroundColor Cyan

# Create build directory if it doesn't exist
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Name "build"
}

Set-Location build

# Configure and build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
    
    if ($Run) {
        Write-Host "Running blockchain application..." -ForegroundColor Yellow
        .\Release\blockchain.exe
    } else {
        Write-Host "To run the application: cd build && .\Release\blockchain.exe" -ForegroundColor Yellow
    }
} else {
    Write-Host "Build failed! Make sure you have:" -ForegroundColor Red
    Write-Host "- Visual Studio 2022 or Visual Studio Build Tools" -ForegroundColor Red
    Write-Host "- CMake installed" -ForegroundColor Red
    Write-Host "- OpenSSL library available" -ForegroundColor Red
    Write-Host "" -ForegroundColor Red
    Write-Host "Try using build-with-vcpkg.ps1 to install dependencies automatically" -ForegroundColor Yellow
}

Set-Location ..
