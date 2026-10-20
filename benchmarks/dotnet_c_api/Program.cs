using System;
using System.Diagnostics;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace BenchmarkDotnetCApi;

[StructLayout(LayoutKind.Sequential, Size = 64)]
public struct BigIntSbo256
{
    private ulong _d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7;
}

internal static class Native
{
    private const string Dll = "bigint_c.dll";

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_create();

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_create_i64(long val);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_create_str(string str, int radix);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void bigint_destroy(IntPtr b);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_init_sbo256(ref BigIntSbo256 storage, long val);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_init_sbo256_str(ref BigIntSbo256 storage, string str, int radix);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void bigint_destroy_sbo256(ref BigIntSbo256 storage);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_add(IntPtr outVal, IntPtr a, IntPtr b);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_sub(IntPtr outVal, IntPtr a, IntPtr b);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_mul(IntPtr outVal, IntPtr a, IntPtr b);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_div(IntPtr outVal, IntPtr a, IntPtr b);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_to_str(IntPtr a, byte[] buf, nuint bufLen, int radix);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int bigint_is_sbo(IntPtr a);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr bigint_version();
}

public class Program
{
    public static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        Console.WriteLine("==========================================================================================");
        Console.WriteLine("           .NET 10 [DllImport] Performance & GC Allocation Benchmark for C-ABI            ");
        Console.WriteLine($"                      Native C-ABI DLL: bigint_c.dll (v{Marshal.PtrToStringAnsi(Native.bigint_version())})");
        Console.WriteLine("==========================================================================================\n");

        // 1. Correctness Check
        VerifyCorrectness();

        // 2. Performance Benchmarks
        Console.WriteLine("Running benchmarks (Latency in ns/op, Managed Allocation in Bytes/op)...\n");

        RunBenchmark256Bit();
        RunBenchmark1024Bit();
        RunBenchmarkChained();

        Console.WriteLine("\n[DONE] All C-ABI [DllImport] benchmarks finished successfully!");
    }

    private static void VerifyCorrectness()
    {
        Console.Write(">>> [Verification] Checking C-ABI correctness against System.Numerics.BigInteger... ");

        string sa = "1234567890123456789012345678901234567890";
        string sb = "9876543210987654321098765432109876543210";

        BigInteger netA = BigInteger.Parse(sa);
        BigInteger netB = BigInteger.Parse(sb);
        BigInteger netSum = netA + netB;
        BigInteger netProd = netA * netB;

        IntPtr cA = Native.bigint_create_str(sa, 10);
        IntPtr cB = Native.bigint_create_str(sb, 10);
        IntPtr cSum = Native.bigint_create();
        IntPtr cProd = Native.bigint_create();

        Native.bigint_add(cSum, cA, cB);
        Native.bigint_mul(cProd, cA, cB);

        byte[] buf = new byte[256];
        Native.bigint_to_str(cSum, buf, (nuint)buf.Length, 10);
        string sumStr = Encoding.ASCII.GetString(buf).TrimEnd('\0');

        Native.bigint_to_str(cProd, buf, (nuint)buf.Length, 10);
        string prodStr = Encoding.ASCII.GetString(buf).TrimEnd('\0');

        if (sumStr != netSum.ToString() || prodStr != netProd.ToString())
        {
            Console.WriteLine("[FAILED]");
            Console.WriteLine($"  Expected Sum:  {netSum}");
            Console.WriteLine($"  Actual Sum:    {sumStr}");
            Console.WriteLine($"  Expected Prod: {netProd}");
            Console.WriteLine($"  Actual Prod:   {prodStr}");
            Environment.Exit(1);
        }

        Native.bigint_destroy(cProd);
        Native.bigint_destroy(cSum);
        Native.bigint_destroy(cB);
        Native.bigint_destroy(cA);

        Console.WriteLine("[PASSED] (Results identical!)\n");
    }

    private static void RunBenchmark256Bit()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 1: 256-Bit Numbers (4 Limbs, SBO In-Range)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        string s1 = "115792089237316195423570985008687907853269984665640564039457584007913129639935"; // ~256-bit max
        string s2 = "57896044618658097711785492504343953926634992332820282019728792003956564819967";

        BigInteger net1 = BigInteger.Parse(s1);
        BigInteger net2 = BigInteger.Parse(s2);

        IntPtr h1 = Native.bigint_create_str(s1, 10);
        IntPtr h2 = Native.bigint_create_str(s2, 10);
        IntPtr hOut = Native.bigint_create();

        BigIntSbo256 sbo1 = default, sbo2 = default, sboOut = default;
        IntPtr s1Ptr = Native.bigint_init_sbo256_str(ref sbo1, s1, 10);
        IntPtr s2Ptr = Native.bigint_init_sbo256_str(ref sbo2, s2, 10);
        IntPtr sOutPtr = Native.bigint_init_sbo256(ref sboOut, 0);

        const int iterations = 1_000_000;

        // --- 1. Addition ---
        // Warmup
        for (int i = 0; i < 10_000; i++)
        {
            _ = net1 + net2;
            Native.bigint_add(hOut, h1, h2);
            Native.bigint_add(sOutPtr, s1Ptr, s2Ptr);
        }

        // Measure .NET BigInteger Add
        long allocBefore = GC.GetAllocatedBytesForCurrentThread();
        var sw = Stopwatch.StartNew();
        BigInteger netRes = BigInteger.Zero;
        for (int i = 0; i < iterations; i++)
        {
            netRes = net1 + net2;
        }
        sw.Stop();
        long allocNetAdd = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsNetAdd = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Handle Add
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_add(hOut, h1, h2);
        }
        sw.Stop();
        long allocHandleAdd = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsHandleAdd = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Stack SBO Add
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_add(sOutPtr, s1Ptr, s2Ptr);
        }
        sw.Stop();
        long allocSboAdd = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsSboAdd = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // --- 2. Multiplication ---
        // Measure .NET BigInteger Mul
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            netRes = net1 * net2;
        }
        sw.Stop();
        long allocNetMul = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsNetMul = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Handle Mul
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_mul(hOut, h1, h2);
        }
        sw.Stop();
        long allocHandleMul = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsHandleMul = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Stack SBO Mul
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_mul(sOutPtr, s1Ptr, s2Ptr);
        }
        sw.Stop();
        long allocSboMul = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsSboMul = (double)sw.Elapsed.TotalNanoseconds / iterations;

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-18} | {"C-API (Handle)",-18} | {"C-API (Stack SBO)",-20} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|--------------------|--------------------|----------------------|------------------|");
        Console.WriteLine($"| {"Add",-10} | {nsNetAdd,7:F1} ns ({allocNetAdd}B GC) | {nsHandleAdd,7:F1} ns ({allocHandleAdd}B GC) | {nsSboAdd,7:F1} ns ({allocSboAdd}B GC)   | 🚀 {nsNetAdd / nsSboAdd,5:F2}x        |");
        Console.WriteLine($"| {"Mul",-10} | {nsNetMul,7:F1} ns ({allocNetMul}B GC) | {nsHandleMul,7:F1} ns ({allocHandleMul}B GC) | {nsSboMul,7:F1} ns ({allocSboMul}B GC)   | 🚀 {nsNetMul / nsSboMul,5:F2}x        |");
        Console.WriteLine();

        Native.bigint_destroy_sbo256(ref sboOut);
        Native.bigint_destroy_sbo256(ref sbo2);
        Native.bigint_destroy_sbo256(ref sbo1);

        Native.bigint_destroy(hOut);
        Native.bigint_destroy(h2);
        Native.bigint_destroy(h1);
    }

    private static void RunBenchmark1024Bit()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 2: 1024-Bit Numbers (16 Limbs, Karatsuba Multiplication)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        // Generate 1024-bit test numbers
        StringBuilder sb1 = new StringBuilder("9");
        StringBuilder sb2 = new StringBuilder("7");
        for (int i = 0; i < 308; i++) { sb1.Append((i * 7) % 10); sb2.Append((i * 3) % 10); }

        string s1 = sb1.ToString();
        string s2 = sb2.ToString();

        BigInteger net1 = BigInteger.Parse(s1);
        BigInteger net2 = BigInteger.Parse(s2);

        IntPtr h1 = Native.bigint_create_str(s1, 10);
        IntPtr h2 = Native.bigint_create_str(s2, 10);
        IntPtr hOut = Native.bigint_create();

        const int iterations = 100_000;

        // Warmup
        for (int i = 0; i < 1_000; i++)
        {
            _ = net1 * net2;
            Native.bigint_mul(hOut, h1, h2);
        }

        // Measure .NET BigInteger Mul
        long allocBefore = GC.GetAllocatedBytesForCurrentThread();
        var sw = Stopwatch.StartNew();
        BigInteger netRes;
        for (int i = 0; i < iterations; i++)
        {
            netRes = net1 * net2;
        }
        sw.Stop();
        long allocNetMul = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsNetMul = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Mul (Karatsuba)
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_mul(hOut, h1, h2);
        }
        sw.Stop();
        long allocHandleMul = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsHandleMul = (double)sw.Elapsed.TotalNanoseconds / iterations;

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-22} | {"C-API Handle (CPP-BigInt)",-26} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|------------------------|----------------------------|------------------|");
        Console.WriteLine($"| {"Mul 1024b",-10} | {nsNetMul,8:F1} ns ({allocNetMul}B GC) | {nsHandleMul,8:F1} ns ({allocHandleMul}B GC)       | 🚀 {nsNetMul / nsHandleMul,5:F2}x        |");
        Console.WriteLine();

        Native.bigint_destroy(hOut);
        Native.bigint_destroy(h2);
        Native.bigint_destroy(h1);
    }

    private static void RunBenchmarkChained()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 3: Chained Expression: (a + b) * (a - b)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        string s1 = "9876543210987654321098765432109876543210";
        string s2 = "1234567890123456789012345678901234567890";

        BigInteger net1 = BigInteger.Parse(s1);
        BigInteger net2 = BigInteger.Parse(s2);

        IntPtr a = Native.bigint_create_str(s1, 10);
        IntPtr b = Native.bigint_create_str(s2, 10);
        IntPtr t1 = Native.bigint_create();
        IntPtr t2 = Native.bigint_create();
        IntPtr res = Native.bigint_create();

        BigIntSbo256 sboA = default, sboB = default, sboT1 = default, sboT2 = default, sboRes = default;
        IntPtr sA = Native.bigint_init_sbo256_str(ref sboA, s1, 10);
        IntPtr sB = Native.bigint_init_sbo256_str(ref sboB, s2, 10);
        IntPtr sT1 = Native.bigint_init_sbo256(ref sboT1, 0);
        IntPtr sT2 = Native.bigint_init_sbo256(ref sboT2, 0);
        IntPtr sRes = Native.bigint_init_sbo256(ref sboRes, 0);

        const int iterations = 500_000;

        // Warmup
        for (int i = 0; i < 5_000; i++)
        {
            _ = (net1 + net2) * (net1 - net2);
            Native.bigint_add(t1, a, b);
            Native.bigint_sub(t2, a, b);
            Native.bigint_mul(res, t1, t2);
        }

        // Measure .NET Chained
        long allocBefore = GC.GetAllocatedBytesForCurrentThread();
        var sw = Stopwatch.StartNew();
        BigInteger netRes;
        for (int i = 0; i < iterations; i++)
        {
            netRes = (net1 + net2) * (net1 - net2);
        }
        sw.Stop();
        long allocNet = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsNet = (double)sw.Elapsed.TotalNanoseconds / iterations;

        // Measure C-API Stack SBO Chained
        allocBefore = GC.GetAllocatedBytesForCurrentThread();
        sw.Restart();
        for (int i = 0; i < iterations; i++)
        {
            Native.bigint_add(sT1, sA, sB);
            Native.bigint_sub(sT2, sA, sB);
            Native.bigint_mul(sRes, sT1, sT2);
        }
        sw.Stop();
        long allocSbo = (GC.GetAllocatedBytesForCurrentThread() - allocBefore) / iterations;
        double nsSbo = (double)sw.Elapsed.TotalNanoseconds / iterations;

        Console.WriteLine($"| {"Expression",-15} | {".NET BigInteger",-20} | {"C-API Stack SBO",-20} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|-----------------|----------------------|----------------------|------------------|");
        Console.WriteLine($"| {"(a+b)*(a-b)",-15} | {nsNet,8:F1} ns ({allocNet}B GC) | {nsSbo,8:F1} ns ({allocSbo}B GC) | 🚀 {nsNet / nsSbo,5:F2}x        |");
        Console.WriteLine();

        Native.bigint_destroy_sbo256(ref sboRes);
        Native.bigint_destroy_sbo256(ref sboT2);
        Native.bigint_destroy_sbo256(ref sboT1);
        Native.bigint_destroy_sbo256(ref sboB);
        Native.bigint_destroy_sbo256(ref sboA);

        Native.bigint_destroy(res);
        Native.bigint_destroy(t2);
        Native.bigint_destroy(t1);
        Native.bigint_destroy(b);
        Native.bigint_destroy(a);
    }
}
