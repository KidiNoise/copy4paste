# copy4paste for Windows: automated check on a real Windows machine.
#
#   powershell -ExecutionPolicy Bypass -File windows/test-windows.ps1 -Exe build\copy4paste.exe
#
# It exercises the real product path: --self-test proves the panel is built and
# the skins resolve, --verify pushes a real Ctrl+V through the low-level hook
# with SendInput and checks that the chosen copy lands on the clipboard.
param(
    [string]$Exe = "build\\copy4paste.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Host "FAIL: missing $Exe"
    exit 2
}
$exe = (Resolve-Path $Exe).Path
$log = Join-Path $env:TEMP "c4p-verify.log"
$selftest = Join-Path $env:TEMP "c4p-selftest.log"
$import = Join-Path $env:TEMP "c4p-import.log"

function Fail($message) {
    Write-Host "FAIL: $message"
    exit 1
}

function Run($arguments) {
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    return $process.ExitCode
}

# A clean profile, so the skin count is deterministic.
Remove-Item (Join-Path $env:APPDATA "copy4paste") -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "1. self-test: panel, skins and history"
Remove-Item $selftest -ErrorAction SilentlyContinue
Run @("--self-test", "--test-seconds", "3", "--log", $selftest) | Out-Null
Start-Sleep -Seconds 1
if (-not (Test-Path $selftest)) { Fail "no self-test log" }
$text = Get-Content $selftest -Raw
if ($text -notmatch "VERSION=1\.3") { Fail "version" }
if ($text -notmatch "SKINS=4") { Fail "four built-in skins: $text" }
if ($text -notmatch "ENTRIES=4") { Fail "seeded history" }
if ($text -notmatch "PANEL_VISIBLE=1") { Fail "panel not visible" }
if ($text -notmatch "PANEL_HEIGHT=152") { Fail "panel geometry" }

Write-Host "2. import a community skin"
Remove-Item $import -ErrorAction SilentlyContinue
$skin = (Resolve-Path "skins\\examples\\ocean.json").Path
Run @("--import-skin", $skin, "--log", $import) | Out-Null
if (-not (Test-Path $import)) { Fail "no import log" }
if ((Get-Content $import -Raw) -notmatch "IMPORTED=ocean") { Fail "import did not report the id" }
$skinFile = Join-Path $env:APPDATA "copy4paste\\skins\\ocean.json"
if (-not (Test-Path $skinFile)) { Fail "skin file not copied" }

Write-Host "3. the imported skin renders"
Remove-Item $selftest -ErrorAction SilentlyContinue
Run @("--self-test", "--skin", "ocean", "--test-seconds", "3", "--log", $selftest) | Out-Null
Start-Sleep -Seconds 1
$text = Get-Content $selftest -Raw
if ($text -notmatch "SKIN=ocean") { Fail "ocean skin not applied" }
if ($text -notmatch "SKINS=5") { Fail "imported skin not listed" }

Write-Host "4. Ctrl+V through the hook, digit 2 pastes the second copy"
Remove-Item $log -ErrorAction SilentlyContinue
Run @("--verify", "--log", $log) | Out-Null
Start-Sleep -Seconds 1
if (-not (Test-Path $log)) { Fail "no verify log" }
$text = Get-Content $log -Raw
if ($text -notmatch "PANEL_OPENED_BY_CTRL_V=1") { Fail "Ctrl+V did not open the picker: $text" }
if ($text -notmatch "PANEL_VISIBLE_AFTER_DIGIT=0") { Fail "picker stayed open" }
if ($text -notmatch "RESULT=PASS") { Fail "verify failed: $text" }

Write-Host "5. the settings window exists"
$settings = Start-Process -FilePath $exe -ArgumentList @("--settings") -PassThru
$found = $false
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 200
    $windows = Get-Process -Name "copy4paste" -ErrorAction SilentlyContinue
    foreach ($window in $windows) {
        if ($window.MainWindowTitle -like "*Settings*") { $found = $true }
    }
    if ($found) { break }
}
Stop-Process -Id $settings.Id -Force -ErrorAction SilentlyContinue
if (-not $found) { Fail "settings window missing" }

Write-Host "PASS: windows build (panel, skins, import, Ctrl+V, digit, settings)"
