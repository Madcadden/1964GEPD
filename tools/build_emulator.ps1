param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
if (!$OutputDirectory) { $OutputDirectory = Join-Path $SourceRoot 'build-vs2022\verification' }
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio 2022 with Desktop development with C++ is required.' }
$msbuild = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild/VS2022 x86 C++ tools were not found. Install the Desktop development with C++ workload.' }
$project = Join-Path $SourceRoot '1964-modern.vcxproj'
if (!(Test-Path -LiteralPath $project)) { throw "Missing project: $project" }
Push-Location $SourceRoot
try {
    & $msbuild $project /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /nologo /verbosity:minimal "/bl:$OutputDirectory\build.binlog"
    if ($LASTEXITCODE -ne 0) { throw "MSBuild failed with exit code $LASTEXITCODE" }
    $exe = Join-Path $SourceRoot 'build-vs2022\Release\1964.exe'
    if (!(Test-Path -LiteralPath $exe)) { throw "Build succeeded without expected output: $exe" }
    $data = [System.IO.File]::ReadAllBytes($exe)
    $peOffset = [BitConverter]::ToInt32($data, 0x3c)
    if ([BitConverter]::ToUInt32($data, $peOffset) -ne 0x4550 -or [BitConverter]::ToUInt16($data, $peOffset + 4) -ne 0x14c) {
        throw 'Output is not a 32-bit Intel Windows PE executable.'
    }
    Copy-Item -LiteralPath $exe -Destination (Join-Path $OutputDirectory '1964.exe') -Force
    $fileHash = Get-FileHash -LiteralPath $exe -Algorithm SHA256
    $sources = [ordered]@{}
    Get-ChildItem -LiteralPath $SourceRoot -File -Recurse |
        Where-Object { $_.Extension -in '.c', '.h', '.rc', '.vcxproj', '.lib', '.ico', '.bmp' -and $_.FullName -notmatch '[\\/]build-vs2022[\\/]' } |
        Sort-Object FullName | ForEach-Object {
            $relative = $_.FullName.Substring($SourceRoot.Length + 1).Replace('\', '/')
            $sources[$relative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    $manifest = [ordered]@{
        target = 'Release|Win32'; project = '1964-modern.vcxproj';
        msbuild = $msbuild; output_sha256 = $fileHash.Hash;
        output_bytes = $data.Length; sources_sha256 = $sources;
        validation = 'Built with VS2022; requires emulator gameplay validation before release.'
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build-manifest.json') -Encoding utf8
    "$($fileHash.Hash)  1964.exe" | Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ascii
    Write-Host "Built: $OutputDirectory\1964.exe"
    Write-Host "SHA256: $($fileHash.Hash)"
} finally { Pop-Location }
