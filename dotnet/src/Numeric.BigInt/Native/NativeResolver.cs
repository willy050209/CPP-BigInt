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

        // 3. Fallback win-x64 directly if on Windows x64
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && RuntimeInformation.ProcessArchitecture == Architecture.X64)
        {
            string win64Path = Path.Combine(baseDir, "runtimes", "win-x64", "native", filename);
            if (File.Exists(win64Path) && NativeLibrary.TryLoad(win64Path, out handle))
            {
                return handle;
            }
        }

        // 4. Check typical build output paths in dev repo
        string[] devCandidates = [
            Path.Combine(baseDir, "..", "..", "..", "..", "build", "Release", filename),
            Path.Combine(baseDir, "..", "..", "..", "..", "..", "build", "Release", filename),
            Path.Combine(baseDir, "..", "..", "..", "build", "Release", filename),
            Path.Combine(baseDir, "..", "..", "build", "Release", filename),
            Path.Combine(baseDir, "..", "build", "Release", filename)
        ];
        foreach (string devPath in devCandidates)
        {
            if (File.Exists(devPath) && NativeLibrary.TryLoad(Path.GetFullPath(devPath), out handle))
            {
                return handle;
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
