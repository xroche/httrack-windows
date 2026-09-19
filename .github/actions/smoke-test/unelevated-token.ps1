# Defines [Unelevated], which runs a program with the runner's NON-elevated token.
#
# Every install in the smoke test goes through Start-Process and inherits the runner's
# elevated token, so none of them can say whether setup.exe finishes without asking for
# elevation. That is the question the Microsoft Store's silent-install check asks (#184).
# Dot-sourced rather than run, so the caller keeps its own assertions.

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

// Runs a program with the UAC-filtered token linked to this elevated one. Same user, so
// %LOCALAPPDATA% and HKCU still name the paths the caller asserts on.
public static class Unelevated
{
    const int TokenElevation = 20, TokenLinkedToken = 19;
    const uint TOKEN_QUERY = 0x0008, TOKEN_DUPLICATE = 0x0002;
    const uint MAXIMUM_ALLOWED = 0x02000000, CREATE_UNICODE_ENVIRONMENT = 0x00000400;
    const uint WAIT_TIMEOUT = 0x102, WAIT_FAILED = 0xFFFFFFFF;

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
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool TerminateProcess(IntPtr h, uint code);
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
            int err = Marshal.GetLastWin32Error();
            Marshal.FreeHGlobal(buf);
            throw new Win32Exception(err, "GetTokenInformation(" + cls + ") failed");
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

    public static int Run(string commandLine, string workingDirectory, uint timeoutMs)
    {
        IntPtr token, linked = IntPtr.Zero, primary = IntPtr.Zero;
        Check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, out token), "OpenProcessToken");
        try
        {
            if (!IsTokenElevated(token))
                throw new InvalidOperationException("this process is not elevated, so it has no filtered token to drop to");

            // Error 1312 here means the job token came from a service or S4U logon, which has
            // no linked token. The way out is CreateRestrictedToken with a medium integrity
            // level, which is what runas /trustlevel:0x20000 does. Unwritten until it fires,
            // because a fallback nothing exercises is worth less than a clear failure.

            IntPtr buf = Query(token, TokenLinkedToken, IntPtr.Size);
            try { linked = Marshal.ReadIntPtr(buf); } finally { Marshal.FreeHGlobal(buf); }

            // The control. Without it this class would silently run the child elevated and
            // every assertion the caller makes afterwards would pass for the wrong reason.
            if (IsTokenElevated(linked))
                throw new InvalidOperationException("the linked token is elevated too, so nothing here is a standard-user run");

            Check(DuplicateTokenEx(linked, MAXIMUM_ALLOWED, IntPtr.Zero, 2, 1, out primary), "DuplicateTokenEx");

            StartupInfo si = new StartupInfo();
            si.cb = Marshal.SizeOf(si);
            ProcessInformation pi;
            // CreateProcessWithTokenW may write to the command line, so it gets a buffer.
            Check(CreateProcessWithTokenW(primary, 0, null, new StringBuilder(commandLine, commandLine.Length + 1),
                CREATE_UNICODE_ENVIRONMENT, IntPtr.Zero, workingDirectory, ref si, out pi), "CreateProcessWithTokenW");
            try
            {
                // Bounded: a build that started demanding elevation would otherwise sit on a
                // prompt nobody can answer until the job times out, which reads as flakiness.
                uint waited = WaitForSingleObject(pi.hProcess, timeoutMs);
                if (waited == WAIT_FAILED) Check(false, "WaitForSingleObject");
                if (waited == WAIT_TIMEOUT)
                {
                    TerminateProcess(pi.hProcess, 1);
                    throw new TimeoutException("the standard-user run did not finish in " + timeoutMs + " ms");
                }
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
