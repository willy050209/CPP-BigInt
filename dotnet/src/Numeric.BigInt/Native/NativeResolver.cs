using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Numeric.Native;

internal static class NativeResolver
{
    private const string LibraryName = "bigint_c";

    static NativeResolver()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeResolver).Assembly, ResolveDll);
    }

    public static void Initialize()
    {
        // Triggers static constructor
    }

    private static IntPtr ResolveDll(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != LibraryName && libraryName != "bigint_c.dll")
        {
            return IntPtr.Zero;
        }

        string baseDir = AppContext.BaseDirectory;
        string ext = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ".dll" :
                     RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? ".so" : ".dylib";
        string prefix = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "" : "lib";
        string filename = $"{prefix}{LibraryName}{ext}";

        // 1. Check AppContext.BaseDirectory
        string candidate = Path.Combine(baseDir, filename);
        if (File.Exists(candidate) && NativeLibrary.TryLoad(candidate, out IntPtr handle))
        {
            return handle;
        }

        // 2. Check runtimes/<rid>/native/
        string rid = RuntimeInformation.RuntimeIdentifier;
        string ridPath = Path.Combine(baseDir, "runtimes", rid, "native", filename);
        if (File.Exists(ridPath) && NativeLibrary.TryLoad(ridPath, out handle))
        {
            return handle;
        }

        // 3. Fallback to portable RID (<os>-<arch>, e.g., win-x64 / linux-x64)
        string? portableOs = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "win" :
                             RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? "linux" :
                             RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "osx" : null;
        string? portableArch = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.X64 => "x64",
            Architecture.Arm64 => "arm64",
            Architecture.X86 => "x86",
            _ => null
        };
        if (portableOs is not null && portableArch is not null)
        {
            string portableRid = $"{portableOs}-{portableArch}";
            if (!string.Equals(portableRid, rid, StringComparison.OrdinalIgnoreCase))
            {
                string portablePath = Path.Combine(baseDir, "runtimes", portableRid, "native", filename);
                if (File.Exists(portablePath) && NativeLibrary.TryLoad(portablePath, out handle))
                {
                    return handle;
                }
            }
        }

        // 4. Check typical build output paths in dev repo (multi-config and single-config)
        string[] buildSubDirs = [
            Path.Combine("build", "Release"),
            "build",
            "build-linux"
        ];
        string[] relativePrefixes = [
            Path.Combine("..", "..", "..", "..", ".."),
            Path.Combine("..", "..", "..", ".."),
            Path.Combine("..", "..", ".."),
            Path.Combine("..", ".."),
            ".."
        ];
        foreach (string relDir in relativePrefixes)
        {
            foreach (string buildSubDir in buildSubDirs)
            {
                string devPath = Path.Combine(baseDir, relDir, buildSubDir, filename);
                if (File.Exists(devPath) && NativeLibrary.TryLoad(Path.GetFullPath(devPath), out handle))
                {
                    return handle;
                }
            }
        }

        // 5. Default system loader fallback
        if (NativeLibrary.TryLoad(libraryName, assembly, searchPath, out handle))
        {
            return handle;
        }

        return IntPtr.Zero;
    }
}
