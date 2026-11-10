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
    $actualObjectHash = $objectHash
    $knownTests = @{
        '2ed6fb337901939d9aa641a6c0ddeb75656365bac47811bf033850b352a70a12' = 'Noclip + invulnerability with console logging'
        'de5cc80287084508b56de27e9a535de7d389022327771c4f96107a327fc4adc8' = 'Invulnerability only'
        'c1cc5b94e336185e4c211c9909da6e7f43e78a970e228c334e6b3a901a47fb48' = 'Noclip test 1 + invulnerability'
        '147bc86f35759995d89fcbc9c679b7879822ab5c667808a4815590bda268c25d' = 'Noclip test 2 + invulnerability'
    }
    if ($knownTests.ContainsKey($objectHash.ToLowerInvariant())) {
        Write-Host ('Recognized server patch: ' + $knownTests[$objectHash.ToLowerInvariant()]) -ForegroundColor Yellow
        $objectHash = '3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b'
    }
    foreach ($server in $builds.servers) {
        if ($server.'dedicated.exe'.sha256 -eq $exeHash -and
            $server.'server.dll'.sha256 -eq $dllHash -and
            $server.'object.lto'.sha256 -eq $objectHash -and
            @($GameProfile.regions | Where-Object { $server.regions -contains $_ }).Count -gt 0) {
            return
        }
    }
    Write-Host ("Dedicated SHA256: {0}" -f $exeHash)
    Write-Host ("server.dll SHA256: {0}" -f $dllHash)
    Write-Host ("object.lto SHA256: {0}" -f $actualObjectHash)
    throw 'Server files do not match a supported installation. Share the three hashes printed above.' 
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
