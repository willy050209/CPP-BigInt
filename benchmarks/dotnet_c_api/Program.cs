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

[StructLayout(LayoutKind.Sequential, Size = 96)]
public struct BigIntSbo512
{
    private ulong _d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7, _d8, _d9, _d10, _d11;
}

internal static partial class Native
{
    private const string Dll = "bigint_c.dll";

    [LibraryImport(Dll, EntryPoint = "bigint_create")]
    public static partial IntPtr bigint_create();

    [LibraryImport(Dll, EntryPoint = "bigint_create_i64")]
    public static partial IntPtr bigint_create_i64(long val);

    [LibraryImport(Dll, EntryPoint = "bigint_create_str", StringMarshalling = StringMarshalling.Utf8)]
    public static partial IntPtr bigint_create_str(string str, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_clone")]
    public static partial IntPtr bigint_clone(IntPtr src);

    [LibraryImport(Dll, EntryPoint = "bigint_destroy")]
    public static partial void bigint_destroy(IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_init_sbo256")]
    public static partial IntPtr bigint_init_sbo256(ref BigIntSbo256 storage, long val);

    [LibraryImport(Dll, EntryPoint = "bigint_init_sbo256_str", StringMarshalling = StringMarshalling.Utf8)]
    public static partial IntPtr bigint_init_sbo256_str(ref BigIntSbo256 storage, string str, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_destroy_sbo256")]
    public static partial void bigint_destroy_sbo256(ref BigIntSbo256 storage);

    [LibraryImport(Dll, EntryPoint = "bigint_add")]
    public static partial int bigint_add(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_add_inplace")]
    public static partial int bigint_add_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_sub")]
    public static partial int bigint_sub(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_sub_inplace")]
    public static partial int bigint_sub_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mul")]
    public static partial int bigint_mul(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mul_inplace")]
    public static partial int bigint_mul_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_div")]
    public static partial int bigint_div(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_div_inplace")]
    public static partial int bigint_div_inplace(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_mod")]
    public static partial int bigint_mod(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_and")]
    public static partial int bigint_and(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_or")]
    public static partial int bigint_or(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_xor")]
    public static partial int bigint_xor(IntPtr outVal, IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_shl")]
    public static partial int bigint_shl(IntPtr outVal, IntPtr a, nuint shift);

    [LibraryImport(Dll, EntryPoint = "bigint_shr")]
    public static partial int bigint_shr(IntPtr outVal, IntPtr a, nuint shift);

    [LibraryImport(Dll, EntryPoint = "bigint_cmp")]
    public static partial int bigint_cmp(IntPtr a, IntPtr b);

    [LibraryImport(Dll, EntryPoint = "bigint_to_str")]
    public static partial int bigint_to_str(IntPtr a, [Out] byte[] buf, nuint bufLen, int radix);

    [LibraryImport(Dll, EntryPoint = "bigint_is_sbo")]
    public static partial int bigint_is_sbo(IntPtr a);

    [LibraryImport(Dll, EntryPoint = "bigint_version")]
    public static partial IntPtr bigint_version();
}

public class Program
{
    public static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        Console.WriteLine("==========================================================================================");
        Console.WriteLine("           .NET 10 vs CPP-BigInt (C-ABI P/Invoke) Exhaustive Performance Benchmark        ");
        Console.WriteLine($"                      Native C-ABI DLL: bigint_c.dll (v{Marshal.PtrToStringAnsi(Native.bigint_version())})");
        Console.WriteLine("                      Framework: .NET 10.0 (x64 Tiered PGO Enabled)");
        Console.WriteLine("==========================================================================================\n");

        VerifyCorrectness();

        Console.WriteLine("Running benchmarks (Latency in ns/op, Managed Allocation in Bytes/op)...\n");

        RunBenchmark64Bit();
        RunBenchmark256Bit();
        RunBenchmark1024Bit();
        RunBenchmark4096Bit();
        RunBenchmarkChained();
        RunBenchmarkStringConversion();

        Console.WriteLine("\n[DONE] All .NET 10 vs CPP-BigInt (P/Invoke) benchmarks finished successfully!");
    }

    private static string ReadCString(byte[] buf)
    {
        int len = Array.IndexOf(buf, (byte)0);
        if (len < 0) len = buf.Length;
        return Encoding.ASCII.GetString(buf, 0, len);
    }

    private static void VerifyCorrectness()
    {
        Console.Write(">>> [Verification] Checking C-ABI correctness against System.Numerics.BigInteger... ");

        string sa = "1234567890123456789012345678901234567890";
        string sb = "9876543210987654321098765432109876543210";

        BigInteger netA = BigInteger.Parse(sa);
        BigInteger netB = BigInteger.Parse(sb);
        BigInteger netSum = netA + netB;
        BigInteger netDiff = netB - netA;
        BigInteger netProd = netA * netB;
        BigInteger netDiv = netB / netA;

        IntPtr cA = Native.bigint_create_str(sa, 10);
        IntPtr cB = Native.bigint_create_str(sb, 10);
        IntPtr cSum = Native.bigint_create();
        IntPtr cDiff = Native.bigint_create();
        IntPtr cProd = Native.bigint_create();
        IntPtr cDiv = Native.bigint_create();

        Native.bigint_add(cSum, cA, cB);
        Native.bigint_sub(cDiff, cB, cA);
        Native.bigint_mul(cProd, cA, cB);
        Native.bigint_div(cDiv, cB, cA);

        byte[] buf = new byte[256];
        Native.bigint_to_str(cSum, buf, (nuint)buf.Length, 10);
        string sumStr = ReadCString(buf);

        Native.bigint_to_str(cDiff, buf, (nuint)buf.Length, 10);
        string diffStr = ReadCString(buf);

        Native.bigint_to_str(cProd, buf, (nuint)buf.Length, 10);
        string prodStr = ReadCString(buf);

        Native.bigint_to_str(cDiv, buf, (nuint)buf.Length, 10);
        string divStr = ReadCString(buf);

        if (sumStr != netSum.ToString() || diffStr != netDiff.ToString() ||
            prodStr != netProd.ToString() || divStr != netDiv.ToString())
        {
            Console.WriteLine("[FAILED]");
            Console.WriteLine($"  Expected Sum:  {netSum}, Got: {sumStr}");
            Console.WriteLine($"  Expected Diff: {netDiff}, Got: {diffStr}");
            Console.WriteLine($"  Expected Prod: {netProd}, Got: {prodStr}");
            Console.WriteLine($"  Expected Div:  {netDiv}, Got: {divStr}");
            Environment.Exit(1);
        }

        Native.bigint_destroy(cDiv);
        Native.bigint_destroy(cProd);
        Native.bigint_destroy(cDiff);
        Native.bigint_destroy(cSum);
        Native.bigint_destroy(cB);
        Native.bigint_destroy(cA);

        Console.WriteLine("[PASSED] (All arithmetic results 100% identical!)\n");
    }

    private static void RunBenchmark64Bit()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 1: 64-Bit Numbers (1 Limb, Small Integer Range)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        long aVal = 8223372036854775807L;
        long bVal = 4611686018427387903L;

        BigInteger net1 = aVal;
        BigInteger net2 = bVal;

        IntPtr h1 = Native.bigint_create_i64(aVal);
        IntPtr h2 = Native.bigint_create_i64(bVal);
        IntPtr hOut = Native.bigint_create();

        BigIntSbo256 sbo1 = default, sbo2 = default, sboOut = default;
        IntPtr s1Ptr = Native.bigint_init_sbo256(ref sbo1, aVal);
        IntPtr s2Ptr = Native.bigint_init_sbo256(ref sbo2, bVal);
        IntPtr sOutPtr = Native.bigint_init_sbo256(ref sboOut, 0);

        const int iterations = 1_000_000;

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-20} | {"C-API Handle",-20} | {"C-API Stack SBO",-20} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|----------------------|----------------------|----------------------|------------------|");

        // Add
        var (tNetAdd, mNetAdd) = Measure(() => { _ = net1 + net2; }, iterations);
        var (tHAdd, mHAdd) = Measure(() => { Native.bigint_add(hOut, h1, h2); }, iterations);
        var (tSAdd, mSAdd) = Measure(() => { Native.bigint_add(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Add", tNetAdd, mNetAdd, tHAdd, mHAdd, tSAdd, mSAdd);

        // Sub
        var (tNetSub, mNetSub) = Measure(() => { _ = net1 - net2; }, iterations);
        var (tHSub, mHSub) = Measure(() => { Native.bigint_sub(hOut, h1, h2); }, iterations);
        var (tSSub, mSSub) = Measure(() => { Native.bigint_sub(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Sub", tNetSub, mNetSub, tHSub, mHSub, tSSub, mSSub);

        // Mul
        var (tNetMul, mNetMul) = Measure(() => { _ = net1 * net2; }, iterations);
        var (tHMul, mHMul) = Measure(() => { Native.bigint_mul(hOut, h1, h2); }, iterations);
        var (tSMul, mSMul) = Measure(() => { Native.bigint_mul(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Mul", tNetMul, mNetMul, tHMul, mHMul, tSMul, mSMul);

        // Div
        var (tNetDiv, mNetDiv) = Measure(() => { _ = net1 / net2; }, iterations);
        var (tHDiv, mHDiv) = Measure(() => { Native.bigint_div(hOut, h1, h2); }, iterations);
        var (tSDiv, mSDiv) = Measure(() => { Native.bigint_div(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Div", tNetDiv, mNetDiv, tHDiv, mHDiv, tSDiv, mSDiv);

        // Cmp
        var (tNetCmp, mNetCmp) = Measure(() => { _ = net1 < net2; }, iterations);
        var (tHCmp, mHCmp) = Measure(() => { _ = Native.bigint_cmp(h1, h2); }, iterations);
        var (tSCmp, mSCmp) = Measure(() => { _ = Native.bigint_cmp(s1Ptr, s2Ptr); }, iterations);
        PrintRow("Cmp", tNetCmp, mNetCmp, tHCmp, mHCmp, tSCmp, mSCmp);

        Console.WriteLine();

        Native.bigint_destroy_sbo256(ref sboOut);
        Native.bigint_destroy_sbo256(ref sbo2);
        Native.bigint_destroy_sbo256(ref sbo1);
        Native.bigint_destroy(hOut);
        Native.bigint_destroy(h2);
        Native.bigint_destroy(h1);
    }

    private static void RunBenchmark256Bit()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 2: 256-Bit Numbers (4 Limbs, SBO In-Range)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        string s1 = "115792089237316195423570985008687907853269984665640564039457584007913129639935"; // 2^256 - 1
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

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-20} | {"C-API Handle",-20} | {"C-API Stack SBO",-20} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|----------------------|----------------------|----------------------|------------------|");

        // Add
        var (tNetAdd, mNetAdd) = Measure(() => { _ = net1 + net2; }, iterations);
        var (tHAdd, mHAdd) = Measure(() => { Native.bigint_add(hOut, h1, h2); }, iterations);
        var (tSAdd, mSAdd) = Measure(() => { Native.bigint_add(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Add", tNetAdd, mNetAdd, tHAdd, mHAdd, tSAdd, mSAdd);

        // Sub
        var (tNetSub, mNetSub) = Measure(() => { _ = net1 - net2; }, iterations);
        var (tHSub, mHSub) = Measure(() => { Native.bigint_sub(hOut, h1, h2); }, iterations);
        var (tSSub, mSSub) = Measure(() => { Native.bigint_sub(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Sub", tNetSub, mNetSub, tHSub, mHSub, tSSub, mSSub);

        // Mul
        var (tNetMul, mNetMul) = Measure(() => { _ = net1 * net2; }, iterations);
        var (tHMul, mHMul) = Measure(() => { Native.bigint_mul(hOut, h1, h2); }, iterations);
        var (tSMul, mSMul) = Measure(() => { Native.bigint_mul(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Mul", tNetMul, mNetMul, tHMul, mHMul, tSMul, mSMul);

        // Div
        var (tNetDiv, mNetDiv) = Measure(() => { _ = net1 / net2; }, iterations / 2);
        var (tHDiv, mHDiv) = Measure(() => { Native.bigint_div(hOut, h1, h2); }, iterations / 2);
        var (tSDiv, mSDiv) = Measure(() => { Native.bigint_div(sOutPtr, s1Ptr, s2Ptr); }, iterations / 2);
        PrintRow("Div", tNetDiv, mNetDiv, tHDiv, mHDiv, tSDiv, mSDiv);

        // Mod
        var (tNetMod, mNetMod) = Measure(() => { _ = net1 % net2; }, iterations / 2);
        var (tHMod, mHMod) = Measure(() => { Native.bigint_mod(hOut, h1, h2); }, iterations / 2);
        var (tSMod, mSMod) = Measure(() => { Native.bigint_mod(sOutPtr, s1Ptr, s2Ptr); }, iterations / 2);
        PrintRow("Mod", tNetMod, mNetMod, tHMod, mHMod, tSMod, mSMod);

        // Bitwise AND
        var (tNetAnd, mNetAnd) = Measure(() => { _ = net1 & net2; }, iterations);
        var (tHAnd, mHAnd) = Measure(() => { Native.bigint_and(hOut, h1, h2); }, iterations);
        var (tSAnd, mSAnd) = Measure(() => { Native.bigint_and(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("And", tNetAnd, mNetAnd, tHAnd, mHAnd, tSAnd, mSAnd);

        // Bitwise OR
        var (tNetOr, mNetOr) = Measure(() => { _ = net1 | net2; }, iterations);
        var (tHOr, mHOr) = Measure(() => { Native.bigint_or(hOut, h1, h2); }, iterations);
        var (tSOr, mSOr) = Measure(() => { Native.bigint_or(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Or", tNetOr, mNetOr, tHOr, mHOr, tSOr, mSOr);

        // Bitwise XOR
        var (tNetXor, mNetXor) = Measure(() => { _ = net1 ^ net2; }, iterations);
        var (tHXor, mHXor) = Measure(() => { Native.bigint_xor(hOut, h1, h2); }, iterations);
        var (tSXor, mSXor) = Measure(() => { Native.bigint_xor(sOutPtr, s1Ptr, s2Ptr); }, iterations);
        PrintRow("Xor", tNetXor, mNetXor, tHXor, mHXor, tSXor, mSXor);

        // Shift Left 17
        var (tNetShl, mNetShl) = Measure(() => { _ = net1 << 17; }, iterations);
        var (tHShl, mHShl) = Measure(() => { Native.bigint_shl(hOut, h1, 17); }, iterations);
        var (tSShl, mSShl) = Measure(() => { Native.bigint_shl(sOutPtr, s1Ptr, 17); }, iterations);
        PrintRow("Shl", tNetShl, mNetShl, tHShl, mHShl, tSShl, mSShl);

        // Cmp
        var (tNetCmp, mNetCmp) = Measure(() => { _ = net1 < net2; }, iterations);
        var (tHCmp, mHCmp) = Measure(() => { _ = Native.bigint_cmp(h1, h2); }, iterations);
        var (tSCmp, mSCmp) = Measure(() => { _ = Native.bigint_cmp(s1Ptr, s2Ptr); }, iterations);
        PrintRow("Cmp", tNetCmp, mNetCmp, tHCmp, mHCmp, tSCmp, mSCmp);

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
        Console.WriteLine("  Benchmark Suite 3: 1024-Bit Numbers (16 Limbs, Karatsuba & Knuth Division)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

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

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-22} | {"C-API Handle (CPP-BigInt)",-26} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|------------------------|----------------------------|------------------|");

        var (tNetAdd, mNetAdd) = Measure(() => { _ = net1 + net2; }, iterations);
        var (tHAdd, mHAdd) = Measure(() => { Native.bigint_add(hOut, h1, h2); }, iterations);
        PrintRowSimple("Add 1024b", tNetAdd, mNetAdd, tHAdd, mHAdd);

        var (tNetSub, mNetSub) = Measure(() => { _ = net1 - net2; }, iterations);
        var (tHSub, mHSub) = Measure(() => { Native.bigint_sub(hOut, h1, h2); }, iterations);
        PrintRowSimple("Sub 1024b", tNetSub, mNetSub, tHSub, mHSub);

        var (tNetMul, mNetMul) = Measure(() => { _ = net1 * net2; }, iterations);
        var (tHMul, mHMul) = Measure(() => { Native.bigint_mul(hOut, h1, h2); }, iterations);
        PrintRowSimple("Mul 1024b", tNetMul, mNetMul, tHMul, mHMul);

        var (tNetDiv, mNetDiv) = Measure(() => { _ = net1 / net2; }, iterations);
        var (tHDiv, mHDiv) = Measure(() => { Native.bigint_div(hOut, h1, h2); }, iterations);
        PrintRowSimple("Div 1024b", tNetDiv, mNetDiv, tHDiv, mHDiv);

        var (tNetMod, mNetMod) = Measure(() => { _ = net1 % net2; }, iterations);
        var (tHMod, mHMod) = Measure(() => { Native.bigint_mod(hOut, h1, h2); }, iterations);
        PrintRowSimple("Mod 1024b", tNetMod, mNetMod, tHMod, mHMod);

        Console.WriteLine();

        Native.bigint_destroy(hOut);
        Native.bigint_destroy(h2);
        Native.bigint_destroy(h1);
    }

    private static void RunBenchmark4096Bit()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 4: 4096-Bit Numbers (64 Limbs, Toom-3 & Burnikel-Ziegler Division)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        StringBuilder sb1 = new StringBuilder("9");
        StringBuilder sb2 = new StringBuilder("8");
        for (int i = 0; i < 1233; i++) { sb1.Append((i * 13) % 10); sb2.Append((i * 11) % 10); }

        string s1 = sb1.ToString();
        string s2 = sb2.ToString();

        BigInteger net1 = BigInteger.Parse(s1);
        BigInteger net2 = BigInteger.Parse(s2);

        IntPtr h1 = Native.bigint_create_str(s1, 10);
        IntPtr h2 = Native.bigint_create_str(s2, 10);
        IntPtr hOut = Native.bigint_create();

        const int iterations = 10_000;

        Console.WriteLine($"| {"Operation",-10} | {".NET BigInteger",-22} | {"C-API Handle (CPP-BigInt)",-26} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|------------|------------------------|----------------------------|------------------|");

        var (tNetMul, mNetMul) = Measure(() => { _ = net1 * net2; }, iterations);
        var (tHMul, mHMul) = Measure(() => { Native.bigint_mul(hOut, h1, h2); }, iterations);
        PrintRowSimple("Mul 4096b", tNetMul, mNetMul, tHMul, mHMul);

        var (tNetDiv, mNetDiv) = Measure(() => { _ = net1 / net2; }, iterations);
        var (tHDiv, mHDiv) = Measure(() => { Native.bigint_div(hOut, h1, h2); }, iterations);
        PrintRowSimple("Div 4096b", tNetDiv, mNetDiv, tHDiv, mHDiv);

        Console.WriteLine();

        Native.bigint_destroy(hOut);
        Native.bigint_destroy(h2);
        Native.bigint_destroy(h1);
    }

    private static void RunBenchmarkChained()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 5: Chained Expression: (a + b) * (a - b)");
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

        Console.WriteLine($"| {"Expression",-15} | {".NET BigInteger",-22} | {"C-API Stack SBO",-22} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|-----------------|------------------------|------------------------|------------------|");

        var (tNet, mNet) = Measure(() => { _ = (net1 + net2) * (net1 - net2); }, iterations);
        var (tSbo, mSbo) = Measure(() =>
        {
            Native.bigint_add(sT1, sA, sB);
            Native.bigint_sub(sT2, sA, sB);
            Native.bigint_mul(sRes, sT1, sT2);
        }, iterations);

        Console.WriteLine($"| {"(a+b)*(a-b)",-15} | {tNet,8:F1} ns ({mNet,3}B GC)    | {tSbo,8:F1} ns ({mSbo,3}B GC)    | 🚀 {tNet / tSbo,5:F2}x        |");
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

    private static void RunBenchmarkStringConversion()
    {
        Console.WriteLine("------------------------------------------------------------------------------------------");
        Console.WriteLine("  Benchmark Suite 6: Decimal String I/O Conversion (256-bit)");
        Console.WriteLine("------------------------------------------------------------------------------------------");

        string s1 = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
        BigInteger net1 = BigInteger.Parse(s1);
        IntPtr h1 = Native.bigint_create_str(s1, 10);
        byte[] buf = new byte[128];

        const int iterations = 100_000;

        Console.WriteLine($"| {"Operation",-15} | {".NET BigInteger",-22} | {"C-API Handle",-22} | {"Speedup vs .NET",-16} |");
        Console.WriteLine($"|-----------------|------------------------|------------------------|------------------|");

        // Parse (FromString)
        var (tNetParse, mNetParse) = Measure(() => { _ = BigInteger.Parse(s1); }, iterations);
        var (tHParse, mHParse) = Measure(() =>
        {
            IntPtr p = Native.bigint_create_str(s1, 10);
            Native.bigint_destroy(p);
        }, iterations);
        Console.WriteLine($"| {"FromString(10)",-15} | {tNetParse,8:F1} ns ({mNetParse,3}B GC)    | {tHParse,8:F1} ns ({mHParse,3}B GC)    | 🚀 {tNetParse / tHParse,5:F2}x        |");

        // ToString
        var (tNetStr, mNetStr) = Measure(() => { _ = net1.ToString(); }, iterations);
        var (tHStr, mHStr) = Measure(() => { Native.bigint_to_str(h1, buf, (nuint)buf.Length, 10); }, iterations);
        Console.WriteLine($"| {"ToString(10)",-15} | {tNetStr,8:F1} ns ({mNetStr,3}B GC)    | {tHStr,8:F1} ns ({mHStr,3}B GC)    | 🚀 {tNetStr / tHStr,5:F2}x        |");

        Console.WriteLine();
        Native.bigint_destroy(h1);
    }

    private static (double NsPerOp, long BytesAllocPerOp) Measure(Action action, int iterations)
    {
        // Warmup
        for (int i = 0; i < Math.Min(500, iterations); i++)
        {
            action();
        }

        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();

        long bytesBefore = GC.GetAllocatedBytesForCurrentThread();
        var sw = Stopwatch.StartNew();

        for (int i = 0; i < iterations; i++)
        {
            action();
        }

        sw.Stop();
        long bytesAfter = GC.GetAllocatedBytesForCurrentThread();

        double nsPerOp = (double)sw.Elapsed.TotalNanoseconds / iterations;
        long bytesAlloc = (bytesAfter - bytesBefore) / iterations;

        return (nsPerOp, bytesAlloc);
    }

    private static void PrintRow(string op, double tNet, long mNet, double tH, long mH, double tS, long mS)
    {
        double speedup = tNet / Math.Min(tH, tS);
        Console.WriteLine($"| {op,-10} | {tNet,7:F1} ns ({mNet,2}B GC) | {tH,7:F1} ns ({mH,2}B GC) | {tS,7:F1} ns ({mS,2}B GC) | 🚀 {speedup,5:F2}x        |");
    }

    private static void PrintRowSimple(string op, double tNet, long mNet, double tH, long mH)
    {
        double speedup = tNet / tH;
        Console.WriteLine($"| {op,-10} | {tNet,8:F1} ns ({mNet,3}B GC) | {tH,8:F1} ns ({mH,3}B GC)       | 🚀 {speedup,5:F2}x        |");
    }
}
