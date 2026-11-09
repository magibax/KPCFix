# Check the game folder next to this script and identify the supported 1.13 build.
function Read-KPCSignature([string]$Name) {
    $file = Join-Path $PSScriptRoot $Name
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing $Name in the game directory." }
    return (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
}

function Assert-KPCGameVersion {
    $builds = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'SUPPORTED_BUILDS.json') | ConvertFrom-Json
    if ((Read-KPCSignature 'client.exe') -ne $builds.client.sha256) {
        throw 'KPCFix requires the original client.exe from official game version 1.13. Apply the update for your edition first.'
    }
    $shellHash = Read-KPCSignature 'cshell.dll'
    $profile = $builds.shells | Where-Object { $_.sha256 -eq $shellHash } | Select-Object -First 1
    if (!$profile) { throw 'The cshell.dll in this folder does not match a supported official 1.13 edition.' }
    Write-Host ("KPCFix game version 1.13 ({0})." -f ($profile.regions -join ' / '))
    return $profile
}

function Assert-KPCServerVersion([string]$Exe,$GameProfile) {
    $builds = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'SUPPORTED_BUILDS.json') | ConvertFrom-Json
    $exeHash = Read-KPCSignature $Exe
    $dllHash = Read-KPCSignature 'server.dll'
    $objectHash = Read-KPCSignature 'object.lto'
    foreach ($server in $builds.servers) {
        if ($server.'dedicated.exe'.sha256 -eq $exeHash -and
            $server.'server.dll'.sha256 -eq $dllHash -and
            $server.'object.lto'.sha256 -eq $objectHash -and
            @($GameProfile.regions | Where-Object { $server.regions -contains $_ }).Count -gt 0) {
            return
        }
    }
    throw 'The dedicated executable, server.dll and object.lto must belong to the matching official 1.13 server installation.'
}

# Dot-sourcing imports the functions for Setup-KPCFix.ps1. Running this file
# directly performs the check and reports a useful result instead of silently
# defining functions and exiting.
if ($MyInvocation.InvocationName -ne '.') {
    try {
        Write-Host 'Checking KPCFix compatibility...' -ForegroundColor Cyan
        $result = Assert-KPCGameVersion
        Write-Host ("PASS: official 1.13 client and shell recognized ({0})." -f ($result.regions -join ' / ')) -ForegroundColor Green
        Write-Host 'The game files are supported by this KPCFix release.'
        exit 0
    }
    catch {
        Write-Host ("FAIL: {0}" -f $_.Exception.Message) -ForegroundColor Red
        exit 1
    }
}
