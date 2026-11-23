using System;
using System.Text;
using Numeric.Native;

namespace Numeric;

public sealed partial class BigInt
{
    public override string ToString() => ToString(10);

    public string ToString(int radix)
    {
        if (radix < 2 || radix > 36)
        {
            throw new ArgumentOutOfRangeException(nameof(radix), "Radix must be between 2 and 36.");
        }

        nuint requiredLen = NativeMethods.bigint_to_str_len(Handle, radix);
        if (requiredLen == 0) return "0";

        // Add 2 bytes safety margin for null terminator
        int bufLen = checked((int)requiredLen + 2);
        if (bufLen <= 256)
        {
            unsafe
            {
                byte* pBuf = stackalloc byte[bufLen];
                int status = NativeMethods.bigint_to_str(Handle, pBuf, (nuint)bufLen, radix);
                CheckStatus(status);
                int len = 0;
                while (len < bufLen && pBuf[len] != 0) len++;
                return Encoding.ASCII.GetString(pBuf, len);
            }
        }

        byte[] heapBuf = new byte[bufLen];
        int heapStatus = NativeMethods.bigint_to_str(Handle, heapBuf, (nuint)bufLen, radix);
        CheckStatus(heapStatus);
        int nullIdx = Array.IndexOf(heapBuf, (byte)0);
        int actualLen = nullIdx >= 0 ? nullIdx : heapBuf.Length;
        return Encoding.ASCII.GetString(heapBuf, 0, actualLen);
    }

    public string ToString(string? format, IFormatProvider? formatProvider = null)
    {
        if (string.IsNullOrEmpty(format)) return ToString(10);

        char fmt = format[0];
        int radix = fmt switch
        {
            'X' or 'x' => 16,
            'B' or 'b' => 2,
            'O' or 'o' => 8,
            'D' or 'd' => 10,
            _ => 10
        };

        string s = ToString(radix);
        return char.IsUpper(fmt) ? s.ToUpperInvariant() : s.ToLowerInvariant();
    }

    public bool TryFormat(Span<char> destination, out int charsWritten, ReadOnlySpan<char> format = default, IFormatProvider? provider = null)
    {
        string formatted = ToString(format.ToString(), provider);
        if (formatted.Length > destination.Length)
        {
            charsWritten = 0;
            return false;
        }

        formatted.AsSpan().CopyTo(destination);
        charsWritten = formatted.Length;
        return true;
    }
}
