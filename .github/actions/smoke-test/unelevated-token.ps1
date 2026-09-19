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
    const int TokenGroups = 2, TokenLinkedToken = 19, TokenElevation = 20, TokenIntegrityLevel = 25;
    const uint TOKEN_QUERY = 0x0008, TOKEN_DUPLICATE = 0x0002;
    const uint SAFER_SCOPEID_USER = 2, SAFER_LEVELID_NORMALUSER = 0x20000, SAFER_LEVEL_OPEN = 1;
    const uint SE_GROUP_INTEGRITY = 0x00000020, SE_GROUP_USE_FOR_DENY_ONLY = 0x00000010;
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

    [StructLayout(LayoutKind.Sequential)]
    struct TokenMandatoryLabel { public IntPtr Sid; public uint Attributes; }

    [StructLayout(LayoutKind.Sequential)]
    struct SidAndAttributes { public IntPtr Sid; public uint Attributes; }

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
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool SetTokenInformation(IntPtr token, int cls, IntPtr info, int len);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool SaferCreateLevel(uint scope, uint level, uint open, out IntPtr handle, IntPtr reserved);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool SaferComputeTokenFromLevel(IntPtr level, IntPtr inToken, out IntPtr outToken, uint flags, IntPtr reserved);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern bool SaferCloseLevel(IntPtr level);
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool ConvertStringSidToSid(string sid, out IntPtr result);
    [DllImport("advapi32.dll")] static extern int GetLengthSid(IntPtr sid);
    [DllImport("advapi32.dll")] static extern bool EqualSid(IntPtr a, IntPtr b);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcessWithTokenW(IntPtr token, int logonFlags, string app, StringBuilder cmd,
        uint flags, IntPtr env, string dir, ref StartupInfo si, out ProcessInformation pi);

    static void Check(bool ok, string what)
    {
        if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error(), what + " failed");
    }

    static IntPtr Query(IntPtr token, int cls)
    {
        int size;
        GetTokenInformation(token, cls, IntPtr.Zero, 0, out size);
        if (size <= 0) Check(false, "GetTokenInformation(" + cls + ") sizing");
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
        IntPtr buf = Query(token, TokenElevation);
        try { return Marshal.ReadInt32(buf) != 0; } finally { Marshal.FreeHGlobal(buf); }
    }

    // TokenElevation cannot answer this, because a filtered token keeps the elevation flag of
    // the one it came from. A deny-only admin group grants nothing, and that is what decides it.
    static bool CanAdminister(IntPtr token)
    {
        IntPtr admins;
        Check(ConvertStringSidToSid("S-1-5-32-544", out admins), "ConvertStringSidToSid");
        try
        {
            IntPtr buf = Query(token, TokenGroups);
            try
            {
                int count = Marshal.ReadInt32(buf);
                IntPtr groups = IntPtr.Add(buf, IntPtr.Size);  // the count is padded to a pointer
                int size = Marshal.SizeOf(typeof(SidAndAttributes));
                for (int i = 0; i < count; i++)
                {
                    SidAndAttributes g = (SidAndAttributes)Marshal.PtrToStructure(IntPtr.Add(groups, i * size), typeof(SidAndAttributes));
                    if (EqualSid(g.Sid, admins))
                        return (g.Attributes & SE_GROUP_USE_FOR_DENY_ONLY) == 0;
                }
                return false;
            }
            finally { Marshal.FreeHGlobal(buf); }
        }
        finally { LocalFree(admins); }
    }

    public static bool IsElevated()
    {
        IntPtr token;
        Check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, out token), "OpenProcessToken");
        try { return IsTokenElevated(token); } finally { CloseHandle(token); }
    }

    // What runas /trustlevel:0x20000 builds: the admin group denied and every privilege
    // dropped. Used where the account has no split token, which is the case on a hosted
    // runner, and the integrity level has to be lowered by hand because SAFER leaves it.
    static IntPtr SaferToken()
    {
        // Initialised because C# does not call a variable assigned inside a try definitely assigned.
        IntPtr level = IntPtr.Zero, token = IntPtr.Zero, sid = IntPtr.Zero;
        Check(SaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN, out level, IntPtr.Zero), "SaferCreateLevel");
        try { Check(SaferComputeTokenFromLevel(level, IntPtr.Zero, out token, 0, IntPtr.Zero), "SaferComputeTokenFromLevel"); }
        finally { SaferCloseLevel(level); }

        try
        {
            Check(ConvertStringSidToSid("S-1-16-8192", out sid), "ConvertStringSidToSid");  // medium integrity
            try
            {
                TokenMandatoryLabel label;
                label.Sid = sid;
                label.Attributes = SE_GROUP_INTEGRITY;
                // The kernel reads the SID through the length, so the block has to hold it.
                int size = Marshal.SizeOf(typeof(TokenMandatoryLabel)) + GetLengthSid(sid);
                IntPtr buf = Marshal.AllocHGlobal(size);
                try
                {
                    Marshal.StructureToPtr(label, buf, false);
                    Check(SetTokenInformation(token, TokenIntegrityLevel, buf, size), "SetTokenInformation(integrity)");
                }
                finally { Marshal.FreeHGlobal(buf); }
            }
            finally { LocalFree(sid); }

            // SAFER documents no access rights on what it returns, and CreateProcessWithTokenW
            // needs TOKEN_ASSIGN_PRIMARY. Duplicating asks for them where a failure still names
            // the token API. The duplicate carries the label set above.
            IntPtr primary;
            Check(DuplicateTokenEx(token, MAXIMUM_ALLOWED, IntPtr.Zero, 2, 1, out primary), "DuplicateTokenEx(SAFER)");
            return primary;
        }
        finally { CloseHandle(token); }
    }

    // The UAC-linked token where the account has one, because it is the real standard-user
    // token for this logon. A hosted runner has none, and says so with 1312.
    static IntPtr StandardUserToken(IntPtr token)
    {
        int size = IntPtr.Size;
        IntPtr buf = Marshal.AllocHGlobal(size);
        int need;
        bool ok = GetTokenInformation(token, TokenLinkedToken, buf, size, out need);
        int err = Marshal.GetLastWin32Error();
        IntPtr linked = ok ? Marshal.ReadIntPtr(buf) : IntPtr.Zero;
        Marshal.FreeHGlobal(buf);
        if (!ok)
        {
            Console.Error.WriteLine("no linked token (error " + err + "), falling back to a SAFER normal-user token");
            return SaferToken();
        }
        try
        {
            IntPtr primary;
            Check(DuplicateTokenEx(linked, MAXIMUM_ALLOWED, IntPtr.Zero, 2, 1, out primary), "DuplicateTokenEx");
            return primary;
        }
        finally { CloseHandle(linked); }
    }

    public static int Run(string commandLine, string workingDirectory, uint timeoutMs)
    {
        IntPtr token, primary = IntPtr.Zero;
        Check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, out token), "OpenProcessToken");
        try
        {
            if (!IsTokenElevated(token))
                throw new InvalidOperationException("this process is not elevated, so it has nothing to drop from");

            primary = StandardUserToken(token);

            // The control, and it guards both routes: without it the child would keep
            // administrator rights and every assertion the caller makes would pass for nothing.
            if (CanAdminister(primary))
                throw new InvalidOperationException("the token built here can still administer, so nothing would be a standard-user run");

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
            CloseHandle(token);
        }
    }
}
'@
