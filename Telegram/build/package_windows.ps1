[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CompilerPath,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$BinaryDirectory,
    [string]$PackageDirectory,
    [string]$SignToolCommand,
    [switch]$WithoutUpdater
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $BinaryDirectory) { $BinaryDirectory = Join-Path $repoRoot "out\$Configuration" }
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $repoRoot 'out\packages' }
$CompilerPath = (Resolve-Path -LiteralPath $CompilerPath).Path
$BinaryDirectory = (Resolve-Path -LiteralPath $BinaryDirectory).Path
$binary = Join-Path $BinaryDirectory 'AxGram.exe'
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing client: $binary" }
$versionMatch = Select-String -LiteralPath (Join-Path $PSScriptRoot 'version') -Pattern '^AppVersionStrSmall\s+(\d+\.\d+\.\d+)\s*$'
if (-not $versionMatch) { throw 'Cannot read AxGram version.' }
$version = $versionMatch.Matches[0].Groups[1].Value
$arguments = @("/DMyAppVersion=$version", "/DMyAppVersionFull=$version", '/DMyBuildTarget=win64')
if ($Configuration -eq 'Debug') {
    $arguments += '/DDebugBuild'
    $fileName = "AxGram-Debug-Setup-x64-$version"
} else {
    $fileName = "AxGram-Setup-x64-$version"
}
if ($SignToolCommand) {
    $arguments += "/Ssha256=$SignToolCommand"
} else {
    $arguments += '/DUnsignedBuild'
}
$cache = Join-Path $repoRoot 'out\CMakeCache.txt'
$autoUpdateDisabled = (Test-Path -LiteralPath $cache) -and [bool](Select-String -LiteralPath $cache -Pattern '^DESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=ON$')
$includeUpdater = -not ($WithoutUpdater -or $autoUpdateDisabled)
if ($includeUpdater -and -not (Test-Path -LiteralPath (Join-Path $BinaryDirectory 'Updater.exe') -PathType Leaf)) {
    throw 'Updater.exe is missing. Use -WithoutUpdater only for a client built without the native updater.'
}
if (-not $includeUpdater) { $arguments += '/DWithoutUpdater' }
New-Item -ItemType Directory -Path $PackageDirectory -Force | Out-Null
$PackageDirectory = (Resolve-Path -LiteralPath $PackageDirectory).Path
$arguments += "/DReleasePath=$BinaryDirectory", "/O$PackageDirectory", "/F$fileName", (Join-Path $PSScriptRoot 'setup.iss')
& $CompilerPath @arguments
if ($LASTEXITCODE -ne 0) { throw "Installer compiler failed: $LASTEXITCODE" }
$package = Join-Path $PackageDirectory "$fileName.exe"
if (-not (Test-Path -LiteralPath $package -PathType Leaf)) { throw 'Installer output is missing.' }
$packageHash = Get-FileHash -LiteralPath $package -Algorithm SHA256
$manifest = [ordered]@{
    createdUtc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    version = $version
    nativeUpdaterIncluded = $includeUpdater
    binary = $binary
    binarySha256 = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
    package = $package
    packageSha256 = $packageHash.Hash
    signatureStatus = (Get-AuthenticodeSignature -LiteralPath $package).Status.ToString()
    compiler = $CompilerPath
    compilerSha256 = (Get-FileHash -LiteralPath $CompilerPath -Algorithm SHA256).Hash
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath "$package.manifest.json" -Encoding utf8
$packageHash
