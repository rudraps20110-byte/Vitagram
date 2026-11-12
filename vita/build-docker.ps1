$ErrorActionPreference = "Stop"
$VitaDir = Split-Path -Parent $MyInvocation.MyCommand.Path

docker run --rm `
  -v "${VitaDir}:/src" `
  -w /src `
  vitasdk/vitasdk:latest `
  bash -c 'mkdir -p build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && make -j$(nproc)'

Write-Host "Built: $VitaDir\build\VitaGram.vpk" -ForegroundColor Green

