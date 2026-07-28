# Log this runner into Certum SimplySign, so signtool can use the cloud key.
#
# SimplySign Desktop ships as one executable with no command line, and shows no
# window until asked. Launching it a second time is what asks: the running instance
# answers by opening its login window. Failing that, the tray icon is located by
# asking the notification area which process owns each button, because a fixed
# coordinate does not survive (runners differ in what else sits there, and the icon
# may be folded behind the chevron). Credentials come from the environment:
# CERTUM_EMAIL and CERTUM_OTP_URI, the otpauth:// URI from the activation QR code.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$Thumbprint,
    # Stop once the window is open, before any credential is typed. Opening it is
    # the fragile half and needs no secret, so it can be exercised without the
    # approval gate that guards them.
    [switch]$ProbeOnly
)

$ErrorActionPreference = 'Stop'

if (-not $ProbeOnly) {
    if (-not $env:CERTUM_EMAIL) { throw 'CERTUM_EMAIL is empty' }
    if (-not $env:CERTUM_OTP_URI) { throw 'CERTUM_OTP_URI is empty' }
}

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
if ($secret) { Write-Host "::add-mask::$secret" }
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

$src = @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

public static class Tray
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr FindWindow(string c, string w);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr p, IntPtr after, string cls, string win);
    [DllImport("user32.dll")] static extern int GetWindowThreadProcessId(IntPtr h, out int pid);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern bool MapWindowPoints(IntPtr from, IntPtr to, ref RECT r, uint n);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int max);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
    [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr h, IntPtr a, IntPtr size, uint type, uint protect);
    [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr h, IntPtr a, IntPtr size, uint type);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr a, byte[] buf, IntPtr size, out IntPtr read);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);

    [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }

    const uint TB_BUTTONCOUNT = 0x0418, TB_GETBUTTON = 0x0417, TB_GETITEMRECT = 0x041D;

    static void Down() { mouse_event(2, 0, 0, 0, IntPtr.Zero); mouse_event(4, 0, 0, 0, IntPtr.Zero); }
    public static void Click(int x, int y) { SetCursorPos(x, y); System.Threading.Thread.Sleep(300); Down(); }
    public static void DoubleClick(int x, int y) { SetCursorPos(x, y); System.Threading.Thread.Sleep(300); Down(); System.Threading.Thread.Sleep(80); Down(); }

    // The visible icons and the ones folded behind the chevron live in two separate
    // toolbars, and only the visible one can be clicked without opening the chevron.
    static List<object[]> Toolbars()
    {
        var list = new List<object[]>();
        IntPtr tray = FindWindow("Shell_TrayWnd", null);
        IntPtr notify = FindWindowEx(tray, IntPtr.Zero, "TrayNotifyWnd", null);
        IntPtr pager = FindWindowEx(notify, IntPtr.Zero, "SysPager", null);
        IntPtr tb = FindWindowEx(pager != IntPtr.Zero ? pager : notify, IntPtr.Zero, "ToolbarWindow32", null);
        if (tb != IntPtr.Zero) list.Add(new object[] { tb, "notification area", false });
        IntPtr of = FindWindow("NotifyIconOverflowWindow", null);
        IntPtr oftb = FindWindowEx(of, IntPtr.Zero, "ToolbarWindow32", null);
        if (oftb != IntPtr.Zero) list.Add(new object[] { oftb, "hidden (chevron)", true });
        return list;
    }

    // Where the chevron is, so a hidden icon can be brought on screen.
    public static bool Chevron(out int x, out int y)
    {
        x = y = -1;
        IntPtr tray = FindWindow("Shell_TrayWnd", null);
        IntPtr notify = FindWindowEx(tray, IntPtr.Zero, "TrayNotifyWnd", null);
        IntPtr btn = FindWindowEx(notify, IntPtr.Zero, "Button", null);
        if (btn == IntPtr.Zero) return false;
        RECT r; if (!GetWindowRect(btn, out r)) return false;
        x = (r.Left + r.Right) / 2; y = (r.Top + r.Bottom) / 2;
        return true;
    }

    // Every tray button, with the process that owns it. The icon we want is the one
    // whose owning window belongs to SimplySign; matching on the tooltip would break
    // the moment Certum translates it.
    public static string Find(int wantPid, out int x, out int y, out bool hidden)
    {
        x = y = -1; hidden = false;
        var sb = new StringBuilder();
        foreach (var t in Toolbars())
        {
            IntPtr tb = (IntPtr)t[0];
            int trayPid; GetWindowThreadProcessId(tb, out trayPid);
            IntPtr proc = OpenProcess(0x0008 | 0x0010 | 0x0020, false, trayPid);
            if (proc == IntPtr.Zero) { sb.AppendLine("  " + t[1] + ": cannot open process " + trayPid); continue; }
            IntPtr buf = VirtualAllocEx(proc, IntPtr.Zero, (IntPtr)1024, 0x1000 | 0x2000, 0x04);
            int count = (int)SendMessage(tb, TB_BUTTONCOUNT, IntPtr.Zero, IntPtr.Zero);
            sb.AppendLine("  " + t[1] + ": " + count + " icon(s)");
            for (int i = 0; i < count; i++)
            {
                IntPtr got;
                SendMessage(tb, TB_GETBUTTON, (IntPtr)i, buf);
                byte[] btn = new byte[32];
                if (!ReadProcessMemory(proc, buf, btn, (IntPtr)32, out got)) continue;
                // TBBUTTON.dwData, at offset 16 on x64, points at the tray's own record
                // for the icon; its first field is the window that registered it.
                IntPtr data = (IntPtr)BitConverter.ToInt64(btn, 16);
                byte[] rec = new byte[8];
                if (!ReadProcessMemory(proc, data, rec, (IntPtr)8, out got)) continue;
                IntPtr owner = (IntPtr)BitConverter.ToInt64(rec, 0);
                int pid; GetWindowThreadProcessId(owner, out pid);
                var cls = new StringBuilder(256); GetClassName(owner, cls, 256);
                var txt = new StringBuilder(256); GetWindowText(owner, txt, 256);
                sb.AppendLine("    [" + i + "] pid=" + pid + " [" + cls + "] '" + txt + "'");
                if (pid == wantPid && x < 0)
                {
                    SendMessage(tb, TB_GETITEMRECT, (IntPtr)i, buf);
                    byte[] rb = new byte[16];
                    if (ReadProcessMemory(proc, buf, rb, (IntPtr)16, out got))
                    {
                        RECT r;
                        r.Left = BitConverter.ToInt32(rb, 0); r.Top = BitConverter.ToInt32(rb, 4);
                        r.Right = BitConverter.ToInt32(rb, 8); r.Bottom = BitConverter.ToInt32(rb, 12);
                        MapWindowPoints(tb, IntPtr.Zero, ref r, 2);
                        x = (r.Left + r.Right) / 2; y = (r.Top + r.Bottom) / 2;
                        hidden = (bool)t[2];
                        sb.AppendLine("      ^ SimplySign, at " + x + "," + y + (hidden ? " (behind the chevron)" : ""));
                    }
                }
            }
            VirtualFreeEx(proc, buf, IntPtr.Zero, 0x8000);
            CloseHandle(proc);
        }
        return sb.ToString();
    }
}
'@
Add-Type -TypeDefinition $src
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes

$root = [System.Windows.Automation.AutomationElement]::RootElement
$kids = [System.Windows.Automation.TreeScope]::Children
$any = [System.Windows.Automation.Condition]::TrueCondition

# Screenshots are the obvious diagnostic and are deliberately not taken: a build
# artifact of a public repository is public, and the login form holds the address.
# By name rather than by one process id: a second launch may either signal the
# running instance or become a process of its own, and either can own the window.
function Get-SsdPids {
    @(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($Exe)) -ErrorAction SilentlyContinue).Id
}

function Get-AppWindow([int]$seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    do {
        $pids = Get-SsdPids
        $w = $root.FindAll($kids, $any) | Where-Object { $_.Current.ProcessId -in $pids } | Select-Object -First 1
        if ($w) { return $w }
        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $deadline)
    return $null
}

function Show-TopLevel {
    Write-Host '--- top-level windows ---'
    foreach ($e in $root.FindAll($kids, $any)) {
        Write-Host "  [$($e.Current.ClassName)] pid=$($e.Current.ProcessId)"
    }
}

# If the vendor ever ships a command-line companion, driving a GUI stops being
# necessary; list what is actually installed so we find out.
Write-Host "--- $(Split-Path $Exe -Parent) ---"
Get-ChildItem (Split-Path $Exe -Parent) -File | ForEach-Object { Write-Host "  $($_.Name)" }

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds 20
if ($proc.HasExited) { throw "SimplySign Desktop exited immediately (code $($proc.ExitCode))" }
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
Write-Host "desktop is $($b.Width)x$($b.Height), SimplySign is pid $($proc.Id)"

$dlg = Get-AppWindow 10

# The tray is where this application lives, so report it whatever happens: when
# nothing else works, this listing is the only evidence of why.
Write-Host '--- notification area ---'
$x = -1; $y = -1; $hidden = $false
foreach ($p in Get-SsdPids) {
    Write-Host (([Tray]::Find($p, [ref]$x, [ref]$y, [ref]$hidden)).TrimEnd())
    if ($x -ge 0) { break }
}

# No coordinate involved, so this comes first. The window it opens can belong to a
# different process than the one launched, which is why windows are matched by name.
if (-not $dlg) {
    Write-Host 'no window yet; launching it again to make the running instance show itself'
    Start-Process -FilePath $Exe | Out-Null
    $dlg = Get-AppWindow 20
}

if (-not $dlg -and $x -ge 0) {
    if ($hidden) {
        # A hidden icon has no on-screen position until the chevron is opened.
        $cx = 0; $cy = 0
        if ([Tray]::Chevron([ref]$cx, [ref]$cy)) {
            Write-Host "opening the chevron at $cx,$cy"
            [Tray]::Click($cx, $cy)
            Start-Sleep -Seconds 2
            foreach ($p in Get-SsdPids) {
                Write-Host (([Tray]::Find($p, [ref]$x, [ref]$y, [ref]$hidden)).TrimEnd())
                if ($x -ge 0) { break }
            }
        }
    }
    Write-Host "double-clicking the icon at $x,$y"
    [Tray]::DoubleClick($x, $y)
    $dlg = Get-AppWindow 20
}

if (-not $dlg) {
    Show-TopLevel
    throw 'SimplySign never showed a window: neither its tray icon nor a second launch opened one'
}
Write-Host "login window: [$($dlg.Current.ClassName)] '$($dlg.Current.Name)'"
if ($ProbeOnly) {
    Write-Host '::notice::the window opened; stopping before the credentials'
    exit 0
}

# A code minted in the last seconds of its period expires while it is being typed.
$left = $period - ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() % $period)
if ($left -lt 5) { Start-Sleep -Seconds ($left + 1) }
$otp = Get-Otp
Write-Host "::add-mask::$otp"

$wshell = New-Object -ComObject WScript.Shell
$owner = $dlg.Current.ProcessId
for ($try = 1; -not $wshell.AppActivate($owner); $try++) {
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
