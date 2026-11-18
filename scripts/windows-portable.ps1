# Run from the source root on Windows x64 after npm ci. No admin rights needed.
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$version=(Get-Content -Raw package.json | ConvertFrom-Json).version
npm run bundle
if($LASTEXITCODE){throw 'Studio build failed'}
node node_modules/electron/install.js
if($LASTEXITCODE){throw 'Official Electron runtime installation failed'}
$arch=node -p 'process.arch'
$dir="out/PetDesk-$version-Windows-$arch"
if(Test-Path -LiteralPath $dir){throw "Output directory already exists: $dir"}
Copy-Item -Recurse -LiteralPath 'node_modules/electron/dist' -Destination $dir
Copy-Item -Recurse -LiteralPath "out/PetDesk-$version/app" -Destination "$dir/resources/app"
Rename-Item -LiteralPath "$dir/electron.exe" -NewName 'PetDesk.exe'
Copy-Item -LiteralPath "out/PetDesk-$version/使用说明.txt" -Destination $dir
Copy-Item -LiteralPath 'docs/ART.md' -Destination $dir
Compress-Archive -Path $dir -DestinationPath "$dir.zip" -CompressionLevel Optimal
$hash=(Get-FileHash -LiteralPath "$dir.zip" -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path "$dir.zip" -Leaf)" | Set-Content -Encoding ascii "$dir.zip.sha256"
