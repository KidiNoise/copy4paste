# copy4paste for Windows: automated check on a real Windows machine.
#
#   powershell -ExecutionPolicy Bypass -File windows/test-windows.ps1 -Exe build\copy4paste.exe
#
# Nothing here touches the real %APPDATA%: every run uses a throwaway profile,
# so a personal configuration can never be deleted or overwritten by a test.
param(
    [string]$Exe = "build\\copy4paste.exe",
    [string]$PasteTarget = "build\\paste_target.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Host "FAIL: missing $Exe"
    exit 2
}
$exe = (Resolve-Path $Exe).Path
$targetExe = if (Test-Path $PasteTarget) { (Resolve-Path $PasteTarget).Path } else { $null }

# --- isolation ---------------------------------------------------------
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("c4p-test-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $work | Out-Null
$realAppData = $env:APPDATA
$env:APPDATA = Join-Path $work "AppData"
New-Item -ItemType Directory -Path $env:APPDATA | Out-Null

# Remember the real configuration, to prove at the end that it was not touched.
$realConfig = Join-Path $realAppData "copy4paste"
$realBefore = if (Test-Path $realConfig) {
    (Get-ChildItem $realConfig -Recurse -File | Get-FileHash | ForEach-Object { $_.Hash }) -join ","
} else { "<absent>" }

$log = Join-Path $work "verify.log"
$selftest = Join-Path $work "selftest.log"
$import = Join-Path $work "import.log"
$status = Join-Path $work "status.log"
$targetLog = Join-Path $work "target.log"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class C4P {
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public InputUnion u; }
    [StructLayout(LayoutKind.Explicit)]
    public struct InputUnion { [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);

    public const uint INPUT_KEYBOARD = 1;
    public const uint KEYEVENTF_KEYUP = 2;

    public static void Key(ushort vk) {
        INPUT[] down = new INPUT[1];
        down[0].type = INPUT_KEYBOARD; down[0].u.ki.wVk = vk;
        INPUT[] up = new INPUT[1];
        up[0].type = INPUT_KEYBOARD; up[0].u.ki.wVk = vk; up[0].u.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, down, Marshal.SizeOf(typeof(INPUT)));
        System.Threading.Thread.Sleep(30);
        SendInput(1, up, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void CtrlV() {
        INPUT[] inputs = new INPUT[4];
        inputs[0].type = INPUT_KEYBOARD; inputs[0].u.ki.wVk = 0x11;
        inputs[1].type = INPUT_KEYBOARD; inputs[1].u.ki.wVk = 0x56;
        inputs[2].type = INPUT_KEYBOARD; inputs[2].u.ki.wVk = 0x56; inputs[2].u.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[3].type = INPUT_KEYBOARD; inputs[3].u.ki.wVk = 0x11; inputs[3].u.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, inputs, Marshal.SizeOf(typeof(INPUT)));
    }

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr extra);
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr extra);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);

    public static int CountVisible(string title) {
        int found = 0;
        EnumWindows(delegate(IntPtr window, IntPtr extra) {
            if (!IsWindowVisible(window)) return true;
            StringBuilder text = new StringBuilder(256);
            GetWindowTextW(window, text, 256);
            if (text.ToString() == title) found++;
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@

function Fail($message) {
    Write-Host "FAIL: $message"
    exit 1
}

function Run($arguments) {
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    return $process.ExitCode
}

function Wait-Window($title, $seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        if ([C4P]::CountVisible($title) -gt 0) { return $true }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

function Stop-All {
    Get-Process -Name "copy4paste" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process -Name "paste_target" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400
}

try {
    Write-Host "1. self-test: panel, skins and history (isolated profile)"
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
    if (-not (Test-Path (Join-Path $env:APPDATA "copy4paste"))) { Fail "the test profile was not used" }

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
        foreach ($window in (Get-Process -Name "copy4paste" -ErrorAction SilentlyContinue)) {
            if ($window.MainWindowTitle -like "*Settings*") { $found = $true }
        }
        if ($found) { break }
    }
    Stop-Process -Id $settings.Id -Force -ErrorAction SilentlyContinue
    if (-not $found) { Fail "settings window missing" }
    Stop-All

    # --- the new checks, added after the independent review ------------
    if (-not $targetExe) {
        Write-Host "6-8. SKIPPED: paste_target.exe not built (pass -PasteTarget)"
    } else {
        Write-Host "6. the paste really lands in another application's window"
        Set-Clipboard -Value "first copy"
        Start-Sleep -Milliseconds 600
        Set-Clipboard -Value "second copy"
        Start-Sleep -Milliseconds 600
        $daemonStatus = Join-Path $work "daemon.log"
        $daemon = Start-Process -FilePath $exe -ArgumentList @("--test-accept-injected", "--status-file", $daemonStatus) -PassThru
        Start-Sleep -Seconds 2
        $target = Start-Process -FilePath $targetExe -ArgumentList @($targetLog, "25") -PassThru
        if (-not $target) { Fail "the paste target did not start" }
        Start-Sleep -Seconds 2
        # Wait until the instance has actually collected both copies: the
        # clipboard is polled, so a fixed sleep is not evidence.
        $deadline = (Get-Date).AddSeconds(10)
        $entries = 0
        while ((Get-Date) -lt $deadline) {
            if (Test-Path $daemonStatus) {
                $text = Get-Content $daemonStatus -Raw
                if ($text -match "ENTRIES=(\d+)") { $entries = [int]$Matches[1] }
            }
            if ($entries -ge 2) { break }
            Start-Sleep -Milliseconds 150
        }
        $state = if (Test-Path $daemonStatus) { (Get-Content $daemonStatus -Raw) -replace "`r`n", "; " } else { "<no status>" }
        if ($entries -lt 2) { Fail "the instance collected $entries copies: *** $state" }
        [C4P]::CtrlV()
        if (-not (Wait-Window "copy4paste" 5)) {
            $state = (Get-Content $daemonStatus -Raw) -replace "`r`n", "; "
            Fail "Ctrl+V did not open the picker: *** $state"
        }
        [C4P]::Key(0x32)          # "2": the second copy, "first copy"
        Start-Sleep -Seconds 2
        if ([C4P]::CountVisible("copy4paste") -gt 0) { Fail "the picker stayed open after choosing" }
        $pasted = if (Test-Path $targetLog) { (Get-Content $targetLog -Raw) } else { "<no log>" }
        if ($pasted -notmatch "first copy") {
            Fail "the text did not arrive in the other application (got: '$pasted')"
        }
        Write-Host "   the other application received: '$($pasted.Trim())'"
        $clip = Get-Clipboard -Raw
        if ($clip -notmatch "first copy") { Fail "clipboard is '$clip'" }
        Stop-Process -Id $target.Id -Force -ErrorAction SilentlyContinue
        Stop-Process -Id $daemon.Id -Force -ErrorAction SilentlyContinue
        Stop-All

        Write-Host "7. a second --select does not start a second instance"
        $daemon = Start-Process -FilePath $exe -ArgumentList @("--test-accept-injected") -PassThru
        Start-Sleep -Seconds 2
        Set-Clipboard -Value "alpha"
        Start-Sleep -Milliseconds 600
        Set-Clipboard -Value "beta"
        Start-Sleep -Milliseconds 800
        $before = @(Get-Process -Name "copy4paste" -ErrorAction SilentlyContinue).Count
        Run @("--select") | Out-Null
        Start-Sleep -Milliseconds 800
        $after = @(Get-Process -Name "copy4paste" -ErrorAction SilentlyContinue).Count
        if ($after -ne $before) { Fail "a second instance started ($before -> $after)" }
        if (-not (Wait-Window "copy4paste" 5)) { Fail "--select did not open the picker of the running instance" }
        [C4P]::Key(0x1B)   # Escape
        Stop-All
    }

    Write-Host "8. when the keyboard hook cannot be installed, the app says so and stays usable"
    Remove-Item $status -ErrorAction SilentlyContinue
    $noHook = Start-Process -FilePath $exe -ArgumentList @("--no-hook", "--status-file", $status) -PassThru
    Start-Sleep -Seconds 2
    if ($noHook.HasExited) { Fail "the app quit instead of degrading" }
    if (-not (Test-Path $status)) { Fail "no status file" }
    $statusText = Get-Content $status -Raw
    if ($statusText -notmatch "HOOK=unavailable") { Fail "the status does not report the missing hook: $statusText" }
    Set-Clipboard -Value "alpha"
    Start-Sleep -Milliseconds 600
    Set-Clipboard -Value "beta"
    Start-Sleep -Milliseconds 800
    Run @("--select") | Out-Null
    if (-not (Wait-Window "copy4paste" 5)) { Fail "the picker does not open at all without the hook" }
    [C4P]::Key(0x1B)
    Stop-All

    Write-Host "9. the real configuration was never touched"
    $realAfter = if (Test-Path $realConfig) {
        (Get-ChildItem $realConfig -Recurse -File | Get-FileHash | ForEach-Object { $_.Hash }) -join ","
    } else { "<absent>" }
    if ($realBefore -ne $realAfter) { Fail "the real %APPDATA%\copy4paste changed during the tests" }

    Write-Host "PASS: windows build (panel, skins, import, Ctrl+V, real paste, settings, hook failure, isolation)"
} finally {
    Stop-All
    $env:APPDATA = $realAppData
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
