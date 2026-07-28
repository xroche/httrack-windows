# Log this runner into Certum SimplySign, so signtool can use the cloud key.
#
# SimplySign Desktop is a tray-only .NET application: no command line, no window, and
# UI Automation cannot see inside the notification area (it enumerates the panes and
# stops). The login dialog is therefore reachable only by clicking the tray icon at a
# measured coordinate. Credentials come from the environment: CERTUM_EMAIL and
# CERTUM_OTP_URI, the otpauth:// URI from the activation QR code.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$Thumbprint,
    # Measured on the runner's 1024x768 desktop; x depends on how many icons are there.
    [int]$IconX = 842,
    [int]$IconY = 747
)

$ErrorActionPreference = 'Stop'

if (-not $env:CERTUM_EMAIL) { throw 'CERTUM_EMAIL is empty' }
if (-not $env:CERTUM_OTP_URI) { throw 'CERTUM_OTP_URI is empty' }

function ConvertFrom-Base32([string]$s) {
    $alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567'
    $bits = -join (($s -replace '[=\s]', '').ToUpper().ToCharArray() | ForEach-Object {
            $i = $alphabet.IndexOf($_)
            if ($i -lt 0) { throw 'the seed is not valid base32' }
            [Convert]::ToString($i, 2).PadLeft(5, '0')
        })
    [byte[]]@(for ($i = 0; $i + 8 -le $bits.Length; $i += 8) { [Convert]::ToByte($bits.Substring($i, 8), 2) })
}

# Honour the URI's own algorithm: Certum issues SHA-256 seeds, and a snippet that
# assumes SHA-1 emits a wrong code silently, which looks exactly like a typing failure.
$q = @{}
foreach ($kv in (($env:CERTUM_OTP_URI -replace '^[^?]*\??', '') -split '&')) {
    $p = $kv -split '=', 2
    if ($p.Count -eq 2) { $q[$p[0].ToLower()] = [uri]::UnescapeDataString($p[1]) }
}
$secret = if ($q['secret']) { $q['secret'] } else { $env:CERTUM_OTP_URI }
Write-Host "::add-mask::$secret"
$alg = if ($q['algorithm']) { $q['algorithm'].ToUpper() } else { 'SHA1' }
$digits = if ($q['digits']) { [int]$q['digits'] } else { 6 }
$period = if ($q['period']) { [int]$q['period'] } else { 30 }

function Get-Otp {
    $key = ConvertFrom-Base32 $secret
    $counter = [long][math]::Floor([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() / $period)
    $cb = [BitConverter]::GetBytes($counter); [array]::Reverse($cb)
    $hmac = switch ($alg) {
        'SHA256' { [System.Security.Cryptography.HMACSHA256]::new($key) }
        'SHA512' { [System.Security.Cryptography.HMACSHA512]::new($key) }
        default { [System.Security.Cryptography.HMACSHA1]::new($key) }
    }
    $h = $hmac.ComputeHash($cb)
    $o = $h[$h.Length - 1] -band 0x0f
    $bin = (($h[$o] -band 0x7f) -shl 24) -bor (($h[$o + 1] -band 0xff) -shl 16) -bor `
    (($h[$o + 2] -band 0xff) -shl 8) -bor ($h[$o + 3] -band 0xff)
    ($bin % [int][math]::Pow(10, $digits)).ToString().PadLeft($digits, '0')
}

# SendKeys reads +^%~(){}[] as modifiers and grouping, so an address holding one of
# them would be typed as something else entirely.
function Protect-SendKeys([string]$s) {
    -join ($s.ToCharArray() | ForEach-Object { if ('+^%~(){}[]'.Contains($_)) { "{$_}" } else { "$_" } })
}

$cs = @(
    'using System; using System.Runtime.InteropServices;'
    'public static class M {'
    '  [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);'
    '  [DllImport("user32.dll")] static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);'
    '  static void Down() { mouse_event(2, 0, 0, 0, IntPtr.Zero); mouse_event(4, 0, 0, 0, IntPtr.Zero); }'
    '  public static void Double(int x, int y) { SetCursorPos(x, y); System.Threading.Thread.Sleep(300); Down(); System.Threading.Thread.Sleep(80); Down(); }'
    '}'
) -join "`n"
Add-Type -TypeDefinition $cs
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes

$root = [System.Windows.Automation.AutomationElement]::RootElement
$kids = [System.Windows.Automation.TreeScope]::Children
$any = [System.Windows.Automation.Condition]::TrueCondition

# Screenshots are the obvious diagnostic and are deliberately not taken: a build
# artifact of a public repository is public, and the login form holds the address.
function Show-TopLevel([string]$when) {
    Write-Host "--- top-level windows ($when) ---"
    foreach ($e in $root.FindAll($kids, $any)) {
        Write-Host "  [$($e.Current.ClassName)] '$($e.Current.Name)' pid=$($e.Current.ProcessId)"
    }
}

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds 25
if ($proc.HasExited) { throw "SimplySign Desktop exited immediately (code $($proc.ExitCode))" }

$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
Write-Host "desktop is $($b.Width)x$($b.Height), tray icon expected at $IconX,$IconY"
if ($b.Width -ne 1024 -or $b.Height -ne 768) {
    throw "the tray coordinate was measured on a 1024x768 desktop, and this one is $($b.Width)x$($b.Height)"
}

# The icon is visible rather than folded behind the chevron.
[M]::Double($IconX, $IconY)
Start-Sleep -Seconds 6
Show-TopLevel 'after the tray click'

$dlg = $root.FindAll($kids, $any) | Where-Object { $_.Current.ProcessId -eq $proc.Id } | Select-Object -First 1
if (-not $dlg) { throw "the tray click opened no window: nothing to log into (is $IconX,$IconY still the icon?)" }
Write-Host "login window: [$($dlg.Current.ClassName)] '$($dlg.Current.Name)'"

# A code minted in the last seconds of its period expires while it is being typed.
$left = $period - ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() % $period)
if ($left -lt 5) { Start-Sleep -Seconds ($left + 1) }
$otp = Get-Otp
Write-Host "::add-mask::$otp"

$wshell = New-Object -ComObject WScript.Shell
for ($try = 1; -not $wshell.AppActivate($proc.Id); $try++) {
    if ($try -ge 3) { throw 'could not focus the login window: the keystrokes would go to whatever has focus' }
    Start-Sleep -Seconds 2
}
Start-Sleep -Milliseconds 800
$wshell.SendKeys("$(Protect-SendKeys $env:CERTUM_EMAIL){TAB}$otp{ENTER}")

# The virtual card appears in the user's store within seconds of a good login.
$deadline = (Get-Date).AddSeconds(90)
do {
    Start-Sleep -Seconds 5
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object Thumbprint -eq $Thumbprint
} while (-not $cert -and (Get-Date) -lt $deadline)

if (-not $cert) {
    Write-Host "certificates in the store: $((Get-ChildItem Cert:\CurrentUser\My).Thumbprint -join ', ')"
    throw "$Thumbprint never appeared: the login was refused, or the code was wrong"
}
if (-not $cert.HasPrivateKey) { throw 'the certificate arrived without its private key: signing would fail' }
Write-Host "::notice::signed in to SimplySign as $($cert.Subject)"
