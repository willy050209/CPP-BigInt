using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Text.Json;

namespace BenchmarkBigInt;

public class Metric
{
    public string Tier { get; set; } = "";
    public int Bits { get; set; }
    public string Operation { get; set; } = "";
    public long Iterations { get; set; }
    public double TotalNs { get; set; }
    public double NsPerOp { get; set; }
    public double OpsPerSec { get; set; }
}

public class BenchmarkResult
{
    public string Target { get; set; } = ".NET 10 (BigInteger)";
    public List<Metric> Metrics { get; set; } = new();
}

public struct TestPair
{
    public string ADec;
    public string BDec;
    public string AHex;
    public string BHex;
}

class Program
{
    static List<TestPair> LoadDataset(string filePath)
    {
        var list = new List<TestPair>();
        if (!File.Exists(filePath)) return list;
        foreach (var line in File.ReadLines(filePath))
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            var parts = line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length >= 4)
            {
                list.Add(new TestPair { ADec = parts[0], BDec = parts[1], AHex = parts[2], BHex = parts[3] });
            }
        }
        return list;
    }

    static double MeasureNs(Action action, int iterations, int warmup = 5)
    {
        for (int i = 0; i < warmup; ++i) action();
        long start = Stopwatch.GetTimestamp();
        for (int i = 0; i < iterations; ++i) action();
        long end = Stopwatch.GetTimestamp();
        double elapsedSeconds = (double)(end - start) / Stopwatch.Frequency;
        return elapsedSeconds * 1e9;
    }

    static void RunTier(
        BenchmarkResult report,
        string dataDir,
        string tier,
        int bits,
        int arithIters,
        int mulDivIters,
        int ioIters,
        int memIters)
    {
        string path = Path.Combine(dataDir, $"{tier}_{bits}.txt");
        var pairs = LoadDataset(path);
        if (pairs.Count == 0) return;

        int N = pairs.Count;
        var aNums = new BigInteger[N];
        var bNums = new BigInteger[N];

        for (int i = 0; i < N; i++)
        {
            aNums[i] = BigInteger.Parse(pairs[i].ADec, CultureInfo.InvariantCulture);
            bNums[i] = BigInteger.Parse(pairs[i].BDec, CultureInfo.InvariantCulture);
        }

        void AddMetric(string op, int iters, double totalNs)
        {
            long totalOps = (long)iters * N;
            double nsPerOp = totalNs / totalOps;
            double opsPerSec = totalNs > 0 ? (totalOps * 1e9 / totalNs) : 0;
            report.Metrics.Add(new Metric
            {
                Tier = tier,
                Bits = bits,
                Operation = op,
                Iterations = totalOps,
                TotalNs = totalNs,
                NsPerOp = nsPerOp,
                OpsPerSec = opsPerSec
            });
            Console.WriteLine($"{report.Target,-22} {tier,-8} {bits,6} {op,-16} {nsPerOp,12:F2} ns/op {opsPerSec,14:N0} ops/s");
        }

        // 1. Add
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= (aNums[i] + bNums[i]);
            }, arithIters);
            AddMetric("Add", arithIters, ns);
        }

        // 2. Sub
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= (aNums[i] - bNums[i]);
            }, arithIters);
            AddMetric("Sub", arithIters, ns);
        }

        // 3. Mul
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= (aNums[i] * bNums[i]);
            }, mulDivIters);
            AddMetric("Mul", mulDivIters, ns);
        }

        // 4. Div
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= (aNums[i] / bNums[i]);
            }, mulDivIters);
            AddMetric("Div", mulDivIters, ns);
        }

        // 5. Mod
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= (aNums[i] % bNums[i]);
            }, mulDivIters);
            AddMetric("Mod", mulDivIters, ns);
        }

        // 6. ToString_10
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink += aNums[i].ToString().Length;
            }, ioIters);
            AddMetric("ToString_10", ioIters, ns);
        }

        // 7. FromString_10
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++) sink ^= BigInteger.Parse(pairs[i].ADec, CultureInfo.InvariantCulture);
            }, ioIters);
            AddMetric("FromString_10", ioIters, ns);
        }

        // 8. MemPressure: temporary variable chained operations
        {
            BigInteger sink = BigInteger.Zero;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger tmp = (aNums[i] + bNums[i]) - (aNums[i] ^ bNums[i]);
                    sink ^= tmp;
                }
            }, memIters);
            AddMetric("MemPressure", memIters, ns);
        }
    }

    static void Main(string[] args)
    {
        string dataDir = args.Length > 0 ? args[0] : "benchmarks/data";
        string outJson = args.Length > 1 ? args[1] : "benchmarks/results/results_dotnet.json";

        Console.WriteLine("========================================================================");
        Console.WriteLine("        Benchmark Target: .NET 10 (System.Numerics.BigInteger)          ");
        Console.WriteLine("========================================================================");

        var report = new BenchmarkResult();

        // Tier 1: Small (64, 128, 256 bits)
        RunTier(report, dataDir, "small", 64,  50, 50, 20, 50);
        RunTier(report, dataDir, "small", 128, 50, 50, 20, 50);
        RunTier(report, dataDir, "small", 256, 50, 50, 20, 50);

        // Tier 2: Medium (512, 1024, 2048, 4096 bits)
        RunTier(report, dataDir, "medium", 512,  20, 20, 10, 20);
        RunTier(report, dataDir, "medium", 1024, 20, 20, 10, 20);
        RunTier(report, dataDir, "medium", 2048, 10, 10, 5,  10);
        RunTier(report, dataDir, "medium", 4096, 5,  5,  2,  5);

        // Tier 3: Large (16384, 32768, 65536 bits)
        RunTier(report, dataDir, "large", 16384, 3, 2, 1, 2);
        RunTier(report, dataDir, "large", 32768, 2, 1, 1, 1);
        RunTier(report, dataDir, "large", 65536, 1, 1, 1, 1);

        string? dir = Path.GetDirectoryName(outJson);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        var options = new JsonSerializerOptions { WriteIndented = true };
        File.WriteAllText(outJson, JsonSerializer.Serialize(report, options));
        Console.WriteLine($"[.NET Benchmark] Results exported to {outJson}");
    }
}
