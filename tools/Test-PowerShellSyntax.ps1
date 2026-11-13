# Parse every tracked PowerShell script in full with the interpreter running this
# test. PowerShell parses a whole file before running any of it, so one parse
# error stops the script from starting; a test of an extracted block cannot
# stand in for this check. Game files are not required.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$files=@(& git -C $root ls-files -- '*.ps1')
if($LASTEXITCODE -ne 0 -or $files.Count -eq 0){throw 'No tracked PowerShell scripts found'}
$failed=$false
foreach($relative in $files){
 $tokens=$null;$errors=$null
 [void][System.Management.Automation.Language.Parser]::ParseFile((Join-Path $root $relative),[ref]$tokens,[ref]$errors)
 if($errors.Count -eq 0){Write-Host "ok   $relative";continue}
 $failed=$true
 foreach($e in $errors){Write-Host ("FAIL {0}:{1}: {2}" -f $relative,$e.Extent.StartLineNumber,$e.Message)}
}
if($failed){throw 'PowerShell parse errors found'}
Write-Host ("PASS: {0} scripts parse under PowerShell {1}" -f $files.Count,$PSVersionTable.PSVersion)
