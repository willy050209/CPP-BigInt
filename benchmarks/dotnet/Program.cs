using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Numerics;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace BenchmarkBigInt;

public class Metric
{
    [JsonPropertyName("layer")]
    public string Layer { get; set; } = "layer2_user";

    [JsonPropertyName("tier")]
    public string Tier { get; set; } = "";

    [JsonPropertyName("bits")]
    public int Bits { get; set; }

    [JsonPropertyName("operation")]
    public string Operation { get; set; } = "";

    [JsonPropertyName("iterations")]
    public long Iterations { get; set; }

    [JsonPropertyName("total_ns")]
    public double TotalNs { get; set; }

    [JsonPropertyName("ns_per_op")]
    public double NsPerOp { get; set; }

    [JsonPropertyName("ops_per_sec")]
    public double OpsPerSec { get; set; }
}

public class BenchmarkResult
{
    [JsonPropertyName("target")]
    public string Target { get; set; } = ".NET 10 (BigInteger)";

    [JsonPropertyName("metrics")]
    public List<Metric> Metrics { get; set; } = new();
}

[JsonSerializable(typeof(BenchmarkResult))]
[JsonSerializable(typeof(Metric))]
[JsonSerializable(typeof(List<Metric>))]
internal partial class BenchmarkJsonContext : JsonSerializerContext
{
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
    // Volatile sink to guarantee dead-code elimination prevention across RyuJIT and Native AOT
    private static volatile object? s_sink;

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

        void AddMetric(string op, int iters, double totalNs, string layer = "layer2_user")
        {
            long totalOps = (long)iters * N;
            double nsPerOp = totalNs / totalOps;
            double opsPerSec = totalNs > 0 ? (totalOps * 1e9 / totalNs) : 0;
            report.Metrics.Add(new Metric
            {
                Layer = layer,
                Tier = tier,
                Bits = bits,
                Operation = op,
                Iterations = totalOps,
                TotalNs = totalNs,
                NsPerOp = nsPerOp,
                OpsPerSec = opsPerSec
            });
            Console.WriteLine($"{report.Target,-22} {layer,-14} {tier,-8} {bits,6} {op,-18} {nsPerOp,12:F2} ns/op {opsPerSec,14:N0} ops/s");
        }

        // 1. Add (c = a + b) - Fresh Return
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger c = aNums[i] + bNums[i];
                    sink += c.Sign;
                }
            }, arithIters);
            s_sink = sink;
            AddMetric("Add", arithIters, ns);
        }

        // 1b. Add In-Place (a += b) - Illustrating immutable struct allocation
        {
            var aCopy = (BigInteger[])aNums.Clone();
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    aCopy[i] += bNums[i];
                    sink += aCopy[i].Sign;
                }
            }, arithIters);
            s_sink = sink;
            AddMetric("Add_InPlace", arithIters, ns);
        }

        // 2. Sub (c = a - b)
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger c = aNums[i] - bNums[i];
                    sink += c.Sign;
                }
            }, arithIters);
            s_sink = sink;
            AddMetric("Sub", arithIters, ns);
        }

        // 3. Mul (c = a * b)
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger c = aNums[i] * bNums[i];
                    sink += c.Sign;
                }
            }, mulDivIters);
            s_sink = sink;
            AddMetric("Mul", mulDivIters, ns);
        }

        // 4. Div (c = a / b)
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger c = aNums[i] / bNums[i];
                    sink += c.Sign;
                }
            }, mulDivIters);
            s_sink = sink;
            AddMetric("Div", mulDivIters, ns);
        }

        // 5. Mod (c = a % b)
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger c = aNums[i] % bNums[i];
                    sink += c.Sign;
                }
            }, mulDivIters);
            s_sink = sink;
            AddMetric("Mod", mulDivIters, ns);
        }

        // 6. ToString_10
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    sink += aNums[i].ToString().Length;
                }
            }, ioIters);
            s_sink = sink;
            AddMetric("ToString_10", ioIters, ns);
        }

        // 7. FromString_10
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger val = BigInteger.Parse(pairs[i].ADec, CultureInfo.InvariantCulture);
                    sink += val.Sign;
                }
            }, ioIters);
            s_sink = sink;
            AddMetric("FromString_10", ioIters, ns);
        }

        // 8. MemPressure: temporary variable chained operations
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger tmp = (aNums[i] + bNums[i]) - (aNums[i] ^ bNums[i]);
                    sink += tmp.Sign;
                }
            }, memIters);
            s_sink = sink;
            AddMetric("MemPressure", memIters, ns);
        }

        // 9. Chained temporary expression: (a + b) * (a - b)
        {
            int sink = 0;
            double ns = MeasureNs(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    BigInteger d = (aNums[i] + bNums[i]) * (aNums[i] - bNums[i]);
                    sink += d.Sign;
                }
            }, mulDivIters);
            s_sink = sink;
            AddMetric("Chained_Expr", mulDivIters, ns);
        }
    }

    static void Main(string[] args)
    {
        string dataDir = "benchmarks/data";
        string outJson = "benchmarks/results/results_dotnet.json";
        string targetName = ".NET 10 (BigInteger)";

        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--target" && i + 1 < args.Length)
            {
                targetName = args[++i];
            }
            else if (args[i] == "--data-dir" && i + 1 < args.Length)
            {
                dataDir = args[++i];
            }
            else if (args[i] == "--out" && i + 1 < args.Length)
            {
                outJson = args[++i];
            }
            else if (!args[i].StartsWith("--"))
            {
                if (i == 0) dataDir = args[i];
                else if (i == 1) outJson = args[i];
                else if (i == 2) targetName = args[i];
            }
        }

        Console.WriteLine("========================================================================");
        Console.WriteLine($"        Benchmark Target: {targetName}                                  ");
        Console.WriteLine("========================================================================");

        var report = new BenchmarkResult { Target = targetName };

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

        var jsonOptions = new JsonSerializerOptions
        {
            WriteIndented = true,
            TypeInfoResolver = BenchmarkJsonContext.Default
        };
        string jsonText = JsonSerializer.Serialize(report, typeof(BenchmarkResult), BenchmarkJsonContext.Default);
        File.WriteAllText(outJson, jsonText);
        Console.WriteLine($"[{targetName}] Results exported to {outJson}");
    }
}
