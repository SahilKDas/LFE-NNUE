param(
  [string]$Configuration = "Release",
  [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "native/build"
$stage = Join-Path $root "dist/LFE-NNUE-v1.0.0-win64"
$archive = Join-Path $root "dist/LFE-NNUE-v1.0.0-win64.zip"

if (-not $SkipBuild) {
  cmake -S (Join-Path $root "native") -B $build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=$Configuration
  if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
  cmake --build $build --target life_engine life_engine_native_tests -j 4
  if ($LASTEXITCODE -ne 0) { throw "Native build failed." }
  ctest --test-dir $build --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw "Native tests failed." }
}

if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $build "life_engine.exe") -Destination (Join-Path $stage "LFE-NNUE.exe")
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $stage
Copy-Item -LiteralPath (Join-Path $root "RELEASE_NOTES.md") -Destination $stage
Copy-Item -LiteralPath (Join-Path $root "native/vendor/skia/LICENSE.md") -Destination (Join-Path $stage "SKIA_LICENSE.md")

# The release intentionally omits the sidecar DLL. Successful self-test here proves
# that the exact packaged executable can load its embedded renderer fallback.
$process = Start-Process -FilePath (Join-Path $stage "LFE-NNUE.exe") -ArgumentList "--self-test" -Wait -PassThru -WindowStyle Hidden
if ($process.ExitCode -ne 0) { throw "Packaged executable self-test failed with exit code $($process.ExitCode)." }
if (-not (Select-String -LiteralPath (Join-Path $stage "self-test.txt") -Pattern "self-test PASS" -Quiet)) { throw "Packaged executable did not produce a passing smoke report." }
Remove-Item -LiteralPath (Join-Path $stage "self-test.txt") -Force

if (Test-Path $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$archive.sha256" -Value "$hash  LFE-NNUE-v1.0.0-win64.zip" -Encoding ascii
Write-Host "Release ready: $archive"
Write-Host "SHA-256: $hash"
