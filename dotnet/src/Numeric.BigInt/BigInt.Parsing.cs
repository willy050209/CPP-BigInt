using System;
using System.Diagnostics.CodeAnalysis;

namespace Numeric;

public sealed partial class BigInt
{
    public static BigInt Parse(string s, int radix = 10)
    {
        ArgumentNullException.ThrowIfNull(s);
        return new BigInt(s, radix);
    }

    public static BigInt Parse(ReadOnlySpan<char> s, int radix = 10)
    {
        return new BigInt(s, radix);
    }

    public static bool TryParse([NotNullWhen(true)] string? s, int radix, [NotNullWhen(true)] out BigInt? result)
    {
        if (s is null)
        {
            result = null;
            return false;
        }
        return TryParse(s.AsSpan(), radix, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, int radix, [NotNullWhen(true)] out BigInt? result)
    {
        result = null;
        if (s.IsEmpty || radix < 2 || radix > 36)
        {
            return false;
        }

        try
        {
            result = new BigInt(s, radix);
            return true;
        }
        catch
        {
            return false;
        }
    }

    public static bool TryParse([NotNullWhen(true)] string? s, [NotNullWhen(true)] out BigInt? result)
    {
        return TryParse(s, 10, out result);
    }

    public static bool TryParse(ReadOnlySpan<char> s, [NotNullWhen(true)] out BigInt? result)
    {
        return TryParse(s, 10, out result);
    }

    /* IParsable<BigInt> & ISpanParsable<BigInt> */

    static BigInt IParsable<BigInt>.Parse(string s, IFormatProvider? provider) => Parse(s, 10);

    static bool IParsable<BigInt>.TryParse([NotNullWhen(true)] string? s, IFormatProvider? provider, [NotNullWhen(true)] out BigInt? result)
        => TryParse(s, 10, out result);

    static BigInt ISpanParsable<BigInt>.Parse(ReadOnlySpan<char> s, IFormatProvider? provider) => Parse(s, 10);

    static bool ISpanParsable<BigInt>.TryParse(ReadOnlySpan<char> s, IFormatProvider? provider, [NotNullWhen(true)] out BigInt? result)
        => TryParse(s, 10, out result);
}
