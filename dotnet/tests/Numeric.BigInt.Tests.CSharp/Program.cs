using System;
using System.Diagnostics;
using System.Numerics;
using Numeric;

namespace Numeric.BigIntTests.CSharp;

internal static class Program
{
    public static int Main()
    {
        Console.WriteLine("============================================================");
        Console.WriteLine(">>> Starting C# 14 (.NET 10) Numeric.BigInt Test Suite <<<");
        Console.WriteLine("============================================================");

        try
        {
            TestArithmeticOperators();
            TestBitwiseOperators();
            TestComparisons();
            TestMathFunctions();
            TestFormattingAndParsing();
            TestConversions();
            TestSboAndScopeZeroGc();
            TestLargeNumbersAgainstSystemNumerics();

            Console.WriteLine("\n[SUCCESS] All C# tests passed cleanly with 0 errors!");
            return 0;
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"\n[FAIL] Test aborted with exception: {ex}");
            Console.ResetColor();
            return 1;
        }
    }

    private static void AssertEqual<T>(T expected, T actual, string message)
    {
        if (!EqualityComparer<T>.Default.Equals(expected, actual))
        {
            throw new Exception($"Assertion failed: {message}. Expected: {expected}, Got: {actual}");
        }
    }

    private static void TestArithmeticOperators()
    {
        Console.Write("Testing arithmetic operators (+, -, *, /, %, unary -, ++, --)... ");

        BigInt a = new(100L);
        BigInt b = new(30L);

        AssertEqual("130", (a + b).ToString(), "100 + 30");
        AssertEqual("70", (a - b).ToString(), "100 - 30");
        AssertEqual("3000", (a * b).ToString(), "100 * 30");
        AssertEqual("3", (a / b).ToString(), "100 / 30");
        AssertEqual("10", (a % b).ToString(), "100 % 30");

        AssertEqual("-100", (-a).ToString(), "-100");
        AssertEqual("100", (+a).ToString(), "+100");

        BigInt c = new(10L);
        c++;
        AssertEqual("11", c.ToString(), "10++");
        c--;
        AssertEqual("10", c.ToString(), "11--");

        // Negative arithmetic
        BigInt negA = new(-100L);
        BigInt negB = new(30L);
        AssertEqual("-70", (negA + negB).ToString(), "-100 + 30");
        AssertEqual("-130", (negA - negB).ToString(), "-100 - 30");
        AssertEqual("-3000", (negA * negB).ToString(), "-100 * 30");
        AssertEqual("-3", (negA / negB).ToString(), "-100 / 30");

        Console.WriteLine("PASSED");
    }

    private static void TestBitwiseOperators()
    {
        Console.Write("Testing bitwise operators (&, |, ^, ~, <<, >>)... ");

        BigInt a = new(0b1100L);
        BigInt b = new(0b1010L);

        AssertEqual("8", (a & b).ToString(), "0b1100 & 0b1010 == 0b1000 (8)");
        AssertEqual("14", (a | b).ToString(), "0b1100 | 0b1010 == 0b1110 (14)");
        AssertEqual("6", (a ^ b).ToString(), "0b1100 ^ 0b1010 == 0b0110 (6)");

        BigInt shiftVal = new(1L);
        AssertEqual("1024", (shiftVal << 10).ToString(), "1 << 10");
        AssertEqual("1", ((shiftVal << 10) >> 10).ToString(), "(1 << 10) >> 10");

        Console.WriteLine("PASSED");
    }

    private static void TestComparisons()
    {
        Console.Write("Testing comparison operators (==, !=, <, <=, >, >=, Equals)... ");

        BigInt a = new(100L);
        BigInt b = new(200L);
        BigInt a2 = new(100L);

        AssertEqual(true, a < b, "100 < 200");
        AssertEqual(true, a <= b, "100 <= 200");
        AssertEqual(true, a <= a2, "100 <= 100");
        AssertEqual(true, b > a, "200 > 100");
        AssertEqual(true, b >= a, "200 >= 100");
        AssertEqual(true, a == a2, "100 == 100");
        AssertEqual(true, a != b, "100 != 200");

        AssertEqual(true, a.Equals(a2), "100 Equals 100");
        AssertEqual(false, a.Equals(b), "100 Equals 200 is false");
        AssertEqual(0, a.CompareTo(a2), "100 CompareTo 100 == 0");
        AssertEqual(-1, a.CompareTo(b), "100 CompareTo 200 < 0");

        Console.WriteLine("PASSED");
    }

    private static void TestMathFunctions()
    {
        Console.Write("Testing math functions (Pow, Gcd, Lcm, Sqrt, DivRem, Abs, Min, Max)... ");

        BigInt two = new(2L);
        AssertEqual("1024", BigInt.Pow(two, 10).ToString(), "2^10");

        BigInt a = new(54L);
        BigInt b = new(24L);
        AssertEqual("6", BigInt.Gcd(a, b).ToString(), "gcd(54, 24)");
        AssertEqual("216", BigInt.Lcm(a, b).ToString(), "lcm(54, 24)");

        BigInt square = new(144L);
        AssertEqual("12", BigInt.Sqrt(square).ToString(), "sqrt(144)");
        BigInt squareNonExact = new(150L);
        AssertEqual("12", BigInt.Sqrt(squareNonExact).ToString(), "sqrt(150) == 12");

        BigInt divA = new(100L);
        BigInt divB = new(30L);
        BigInt q = BigInt.DivRem(divA, divB, out BigInt rem);
        AssertEqual("3", q.ToString(), "DivRem quotient");
        AssertEqual("10", rem.ToString(), "DivRem remainder");

        BigInt neg = new(-999L);
        AssertEqual("999", BigInt.Abs(neg).ToString(), "Abs(-999)");

        AssertEqual("54", BigInt.Max(a, b).ToString(), "Max(54, 24)");
        AssertEqual("24", BigInt.Min(a, b).ToString(), "Min(54, 24)");

        Console.WriteLine("PASSED");
    }

    private static void TestFormattingAndParsing()
    {
        Console.Write("Testing parsing and formatting (Dec, Hex, Radix 2..36, TryParse)... ");

        string decimalStr = "123456789012345678901234567890";
        BigInt parsed = BigInt.Parse(decimalStr);
        AssertEqual(decimalStr, parsed.ToString(), "Decimal parse roundtrip");

        string hexStr = "DEADBEEFCAFEBABE";
        BigInt hexParsed = BigInt.Parse(hexStr, 16);
        AssertEqual("deadbeefcafebabe", hexParsed.ToString("x"), "Hex parse roundtrip");
        AssertEqual("DEADBEEFCAFEBABE", hexParsed.ToString("X"), "Hex uppercase roundtrip");

        BigInt binVal = new(42L);
        AssertEqual("101010", binVal.ToString("B"), "Binary format");

        BigInt base36Val = BigInt.Parse("HELLO", 36);
        AssertEqual("hello", base36Val.ToString(36), "Base36 parse roundtrip");

        bool trySuccess = BigInt.TryParse("987654321", out BigInt? tryVal);
        AssertEqual(true, trySuccess, "TryParse success");
        AssertEqual("987654321", tryVal?.ToString() ?? "", "TryParse value");

        bool tryFail = BigInt.TryParse("invalid123", out BigInt? failVal);
        AssertEqual(false, tryFail, "TryParse invalid fails gracefully");

        Console.WriteLine("PASSED");
    }

    private static void TestConversions()
    {
        Console.Write("Testing implicit and explicit conversions... ");

        BigInt fromInt = 42;
        BigInt fromLong = 9000000000000000000L;
        BigInt fromUlong = 18000000000000000000UL;

        AssertEqual(42L, (long)fromInt, "Cast to long");
        AssertEqual(42, (int)fromInt, "Cast to int");
        AssertEqual(9000000000000000000L, (long)fromLong, "Cast long roundtrip");
        AssertEqual(18000000000000000000UL, (ulong)fromUlong, "Cast ulong roundtrip");

        Console.WriteLine("PASSED");
    }

    private static void TestSboAndScopeZeroGc()
    {
        Console.Write("Testing BigIntSbo256 and ref struct BigIntScope zero-GC arena... ");

        // Test BigIntSbo256
        using var sboA = new BigIntSbo256(12345L);
        using var sboB = new BigIntSbo256(67890L);
        var sboRes = new BigIntSbo256();
        try
        {
            BigIntSbo256.Add(ref sboRes, in sboA, in sboB);
            AssertEqual("80235", sboRes.ToString(), "BigIntSbo256 Add");

            BigIntSbo256.Multiply(ref sboRes, in sboA, in sboB);
            AssertEqual("838102050", sboRes.ToString(), "BigIntSbo256 Multiply");
        }
        finally
        {
            sboRes.Dispose();
        }

        // Test BigIntScope with stack buffer
        Span<byte> stackBuffer = stackalloc byte[512];
        using (var scope = new BigIntScope(stackBuffer))
        {
            IntPtr a = scope.Create(500L);
            IntPtr b = scope.Create(200L);
            IntPtr addRes = scope.Create();
            IntPtr mulRes = scope.Create();

            scope.Add(addRes, a, b);
            AssertEqual("700", scope.ToString(addRes), "BigIntScope Add");

            scope.Multiply(mulRes, a, b);
            AssertEqual("100000", scope.ToString(mulRes), "BigIntScope Multiply");

            // Convert to managed BigInt
            BigInt managed = scope.ToBigInt(mulRes);
            AssertEqual("100000", managed.ToString(), "BigIntScope ToBigInt");
        }

        // Measure GC allocation during BigIntScope operations
        long gcBefore = GC.GetAllocatedBytesForCurrentThread();
        Span<byte> benchBuffer = stackalloc byte[512];
        for (int i = 0; i < 1000; i++)
        {
            using var scope = new BigIntScope(benchBuffer);
            IntPtr a = scope.Create(100L);
            IntPtr b = scope.Create(200L);
            IntPtr c = scope.Create();
            scope.Add(c, a, b);
        }
        long gcAfter = GC.GetAllocatedBytesForCurrentThread();
        long totalGcBytes = gcAfter - gcBefore;

        AssertEqual(0L, totalGcBytes, "BigIntScope must have strictly 0 bytes GC allocation");

        Console.WriteLine("PASSED");
    }

    private static void TestLargeNumbersAgainstSystemNumerics()
    {
        Console.Write("Testing 1024-bit arithmetic against System.Numerics.BigInteger... ");

        string strA = "2987132987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987123987";
        string strB = "1287319287319827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827391827";

        BigInt ourA = BigInt.Parse(strA);
        BigInt ourB = BigInt.Parse(strB);

        BigInteger dotA = BigInteger.Parse(strA);
        BigInteger dotB = BigInteger.Parse(strB);

        AssertEqual((dotA + dotB).ToString(), (ourA + ourB).ToString(), "1024-bit Add");
        AssertEqual((dotA - dotB).ToString(), (ourA - ourB).ToString(), "1024-bit Sub");
        AssertEqual((dotA * dotB).ToString(), (ourA * ourB).ToString(), "1024-bit Mul (Karatsuba)");
        AssertEqual((dotA / dotB).ToString(), (ourA / ourB).ToString(), "1024-bit Div");
        AssertEqual((dotA % dotB).ToString(), (ourA % ourB).ToString(), "1024-bit Mod");

        Console.WriteLine("PASSED");
    }
}
