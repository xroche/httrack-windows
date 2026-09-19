# Install and uninstall WinHTTrack with the runner's NON-elevated token.
#
# Every other install in the smoke test inherits the runner's elevated token, so none of
# them can say whether setup.exe finishes without asking for elevation. That is the question
# the Microsoft Store's silent-install check asks, and it is what issue #184 reports.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Setup,
    [Parameter(Mandatory)][string]$UserExe,
    [Parameter(Mandatory)][string]$LogDir
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

// Runs a program with the UAC-filtered token linked to this elevated one. Same user and
// same environment, so %LOCALAPPDATA% and HKCU still name the paths the caller asserts on.
public static class Unelevated
{
    const int TokenElevation = 20, TokenLinkedToken = 19;
    const uint TOKEN_QUERY = 0x0008, TOKEN_DUPLICATE = 0x0002;
    const uint MAXIMUM_ALLOWED = 0x02000000, CREATE_UNICODE_ENVIRONMENT = 0x00000400;
    const uint INFINITE = 0xFFFFFFFF;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct StartupInfo
    {
        public int cb;
        public string lpReserved, lpDesktop, lpTitle;
        public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
        public short wShowWindow, cbReserved2;
        public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct ProcessInformation { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }

    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError = true)] static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
    [DllImport("advapi32.dll", SetLastError = true)] static extern bool OpenProcessToken(IntPtr p, uint access, out IntPtr token);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool GetTokenInformation(IntPtr token, int cls, IntPtr info, int len, out int need);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool DuplicateTokenEx(IntPtr token, uint access, IntPtr attrs, int impersonation, int type, out IntPtr dup);
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcessWithTokenW(IntPtr token, int logonFlags, string app, StringBuilder cmd,
        uint flags, IntPtr env, string dir, ref StartupInfo si, out ProcessInformation pi);

    static void Check(bool ok, string what)
    {
        if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error(), what + " failed");
    }

    static IntPtr Query(IntPtr token, int cls, int size)
    {
        IntPtr buf = Marshal.AllocHGlobal(size);
        int need;
        if (!GetTokenInformation(token, cls, buf, size, out need))
        {
            Marshal.FreeHGlobal(buf);
            throw new Win32Exception(Marshal.GetLastWin32Error(), "GetTokenInformation(" + cls + ") failed");
        }
        return buf;
    }

    static bool IsTokenElevated(IntPtr token)
    {
        IntPtr buf = Query(token, TokenElevation, sizeof(int));
        try { return Marshal.ReadInt32(buf) != 0; } finally { Marshal.FreeHGlobal(buf); }
    }

    public static bool IsElevated()
    {
        IntPtr token;
        Check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, out token), "OpenProcessToken");
        try { return IsTokenElevated(token); } finally { CloseHandle(token); }
    }

    public static int Run(string commandLine, string workingDirectory)
    {
        IntPtr token, linked = IntPtr.Zero, primary = IntPtr.Zero;
        Check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, out token), "OpenProcessToken");
        try
        {
            if (!IsTokenElevated(token))
                throw new InvalidOperationException("this process is not elevated, so it has no filtered token to drop to");

            IntPtr buf = Query(token, TokenLinkedToken, IntPtr.Size);
            try { linked = Marshal.ReadIntPtr(buf); } finally { Marshal.FreeHGlobal(buf); }

            // The control. Without it this class would silently run the child elevated and
            // every assertion the caller makes afterwards would pass for the wrong reason.
            if (IsTokenElevated(linked))
                throw new InvalidOperationException("the linked token is elevated too, so nothing here is a standard-user run");

            Check(DuplicateTokenEx(linked, MAXIMUM_ALLOWED, IntPtr.Zero, 2, 1, out primary), "DuplicateTokenEx");

            StartupInfo si = new StartupInfo();
            si.cb = Marshal.SizeOf(si);
            si.lpDesktop = @"winsta0\default";
            ProcessInformation pi;
            // CreateProcessWithTokenW may write to the command line, so it gets a buffer.
            Check(CreateProcessWithTokenW(primary, 0, null, new StringBuilder(commandLine),
                CREATE_UNICODE_ENVIRONMENT, IntPtr.Zero, workingDirectory, ref si, out pi), "CreateProcessWithTokenW");
            try
            {
                WaitForSingleObject(pi.hProcess, INFINITE);
                uint code;
                Check(GetExitCodeProcess(pi.hProcess, out code), "GetExitCodeProcess");
                return (int)code;
            }
            finally { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        }
        finally
        {
            if (primary != IntPtr.Zero) CloseHandle(primary);
            if (linked != IntPtr.Zero) CloseHandle(linked);
            CloseHandle(token);
        }
    }
}
'@

# A premise, not a skip: if the runner ever stops being elevated, every install above is
# already a standard-user run and this leg has to be rewritten rather than quietly pass.
if (-not [Unelevated]::IsElevated()) {
    throw 'the runner is no longer elevated, so this leg cannot tell the two token kinds apart'
}
if (Test-Path $UserExe) { throw "$UserExe is already there, so the install below would prove nothing" }

$code = [Unelevated]::Run("`"$Setup`" /CURRENTUSER /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=$LogDir\unelevated.log", $LogDir)
if ($code -ne 0) { throw "a standard-user install exited $code" }
if (-not (Test-Path $UserExe)) { throw "a standard-user install placed no $UserExe" }

# The uninstaller has to manage without elevation too, or Add/Remove Programs strands it.
$unins = Get-ChildItem (Join-Path (Split-Path $UserExe) 'unins*.exe') | Select-Object -First 1
if (-not $unins) { throw 'a standard-user install left no uninstaller' }
$code = [Unelevated]::Run("`"$($unins.FullName)`" /VERYSILENT /NORESTART", $LogDir)
if ($code -ne 0) { throw "a standard-user uninstall exited $code" }
Start-Sleep -Seconds 3
if (Test-Path $UserExe) { throw "a standard-user uninstall left $UserExe behind" }

Write-Host "::notice::setup.exe installs and uninstalls with a standard-user token, no elevation asked"
