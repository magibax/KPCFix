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
    Write-Host ("[KPCFix Server Tools] Game version 1.13 ({0})." -f ($profile.regions -join ' / '))
    return $profile
}

function Assert-KPCServerVersion([string]$Exe,$GameProfile) {
    $builds = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'SUPPORTED_BUILDS.json') | ConvertFrom-Json
    $exeHash = Read-KPCSignature $Exe
    $dllHash = Read-KPCSignature 'server.dll'
    $objectHash = Read-KPCSignature 'object.lto'
    $actualObjectHash = $objectHash
    $knownTests = @{
        '054c78e8655d17c0445d53a259a4c5939e123416763a678954c88da6e21aaa45' = 'Live console cheats revocation fix; initially enabled'
        'e85d78b4fd2561277de0ec66d779c9667abc69073cb2832239a9c799b9aa7986' = 'Live console cheats revocation fix; initially disabled'
        'cd28762a9fd9eda2d61de4b300e678cd1db77407ca4065c07a7d504042eda6d0' = 'Live console cheats; initially enabled'
        'cf68a1312078693a4e932de02485ef95f3af7b87087b48eff2daa0c370ae3326' = 'Live console cheats; initially disabled'
        '1d7fe7a6a8e19f62de73e65270447ecdb9735cedc7395584142a9c62da303596' = 'Cheats disabled; chat logging only'
        'f35d386377f0bc0485504071cac87ee853a75ff393bb9716ba42f610d3be1051' = 'Chat announcements test 3 (single console entry)'
        'c666b3f12202d9704ef9e5aa66c449f4409fe482867da2c66f52cbc44a748e96' = 'Chat and toggle announcements test 2'
        '6a1d6a7804b7223e846a4b826983afee559e5305f13b1bf2dc350bd1a79eb5ad' = 'Chat logging and cheat announcement test'
        'a7a266fbb3066b579b1e4e8dabf5ac497ac569b915b5dcaf98c5a58ec22d40ab' = 'Earlier client-name logging test'
        '2ed6fb337901939d9aa641a6c0ddeb75656365bac47811bf033850b352a70a12' = 'Noclip + invulnerability with console logging'
        'de5cc80287084508b56de27e9a535de7d389022327771c4f96107a327fc4adc8' = 'Invulnerability only'
        'c1cc5b94e336185e4c211c9909da6e7f43e78a970e228c334e6b3a901a47fb48' = 'Noclip test 1 + invulnerability'
        '147bc86f35759995d89fcbc9c679b7879822ab5c667808a4815590bda268c25d' = 'Noclip test 2 + invulnerability'
    }
    if ($knownTests.ContainsKey($objectHash.ToLowerInvariant())) {
        Write-Host ('[KPCFix Server Tools] Recognized server patch: ' + $knownTests[$objectHash.ToLowerInvariant()]) -ForegroundColor Yellow
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
    Write-Host ("[KPCFix Server Tools] Dedicated SHA256: {0}" -f $exeHash)
    Write-Host ("[KPCFix Server Tools] server.dll SHA256: {0}" -f $dllHash)
    Write-Host ("[KPCFix Server Tools] object.lto SHA256: {0}" -f $actualObjectHash)
    throw 'Server files do not match a supported installation. Share the three hashes printed above.' 
}

# Dot-sourcing imports the functions for Setup-KPCFix.ps1. Running this file
# directly performs the check and reports a useful result instead of silently
# defining functions and exiting.
if ($MyInvocation.InvocationName -ne '.') {
    try {
        Write-Host '[KPCFix Server Tools] Checking KPCFix compatibility...' -ForegroundColor Cyan
        $result = Assert-KPCGameVersion
        Write-Host ("[KPCFix Server Tools] PASS: official 1.13 client and shell recognized ({0})." -f ($result.regions -join ' / ')) -ForegroundColor Green
        Write-Host '[KPCFix Server Tools] The game files are supported by this KPCFix release.'
        exit 0
    }
    catch {
        Write-Host ("[KPCFix Server Tools] FAIL: {0}" -f $_.Exception.Message) -ForegroundColor Red
        exit 1
    }
}
