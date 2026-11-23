using System;
using System.Runtime.InteropServices;
using System.Text;
using Numeric.Native;

namespace Numeric;

/// <summary>
/// 256-bit (4 limbs) Stack-allocated arbitrary precision integer buffer.
/// Provides strictly 0-GC allocations for high-frequency operations.
/// </summary>
[StructLayout(LayoutKind.Sequential, Size = 64)]
public ref struct BigIntSbo256 : IDisposable
{
    private ulong _d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7;
    private bool _initialized;

    public BigIntSbo256()
    {
        Initialize(0L);
    }

    public BigIntSbo256(long val)
    {
        Initialize(val);
    }

    public BigIntSbo256(ulong val)
    {
        Initialize(val);
    }

    public BigIntSbo256(ReadOnlySpan<char> str, int radix = 10)
    {
        Initialize(str, radix);
    }

    public unsafe void Initialize(long val = 0L)
    {
        if (_initialized) Dispose();
        fixed (ulong* ptr = &_d0)
        {
            NativeMethods.bigint_init_sbo256((IntPtr)ptr, val);
        }
        _initialized = true;
    }

    public unsafe void Initialize(ulong val)
    {
        if (_initialized) Dispose();
        fixed (ulong* ptr = &_d0)
        {
            NativeMethods.bigint_init_sbo256_u64((IntPtr)ptr, val);
        }
        _initialized = true;
    }

    public unsafe void Initialize(ReadOnlySpan<char> str, int radix = 10)
    {
        if (_initialized) Dispose();
        fixed (ulong* ptr = &_d0)
        {
            NativeMethods.bigint_init_sbo256_str((IntPtr)ptr, str.ToString(), radix);
        }
        _initialized = true;
    }

    public unsafe void Dispose()
    {
        if (_initialized)
        {
            fixed (ulong* ptr = &_d0)
            {
                NativeMethods.bigint_destroy_sbo256((IntPtr)ptr);
            }
            _initialized = false;
        }
    }

    public unsafe BigInt ToBigInt()
    {
        if (!_initialized) Initialize(0L);
        fixed (ulong* ptr = &_d0)
        {
            IntPtr clone = NativeMethods.bigint_clone((IntPtr)ptr);
            if (clone == IntPtr.Zero)
            {
                throw new OutOfMemoryException("Failed to clone BigIntSbo256 into BigInt.");
            }
            return new BigInt(new BigIntHandle(clone));
        }
    }

    public static unsafe void Add(ref BigIntSbo256 result, in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pRes = &result._d0)
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            NativeMethods.bigint_add((IntPtr)pRes, (IntPtr)pA, (IntPtr)pB);
        }
        result._initialized = true;
    }

    public static unsafe void Subtract(ref BigIntSbo256 result, in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pRes = &result._d0)
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            NativeMethods.bigint_sub((IntPtr)pRes, (IntPtr)pA, (IntPtr)pB);
        }
        result._initialized = true;
    }

    public static unsafe void Multiply(ref BigIntSbo256 result, in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pRes = &result._d0)
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            NativeMethods.bigint_mul((IntPtr)pRes, (IntPtr)pA, (IntPtr)pB);
        }
        result._initialized = true;
    }

    public static unsafe void Divide(ref BigIntSbo256 result, in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pRes = &result._d0)
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            NativeMethods.bigint_div((IntPtr)pRes, (IntPtr)pA, (IntPtr)pB);
        }
        result._initialized = true;
    }

    public static unsafe void Modulus(ref BigIntSbo256 result, in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pRes = &result._d0)
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            NativeMethods.bigint_mod((IntPtr)pRes, (IntPtr)pA, (IntPtr)pB);
        }
        result._initialized = true;
    }

    public static unsafe int Compare(in BigIntSbo256 a, in BigIntSbo256 b)
    {
        fixed (ulong* pA = &a._d0)
        fixed (ulong* pB = &b._d0)
        {
            return NativeMethods.bigint_cmp((IntPtr)pA, (IntPtr)pB);
        }
    }

    public static unsafe int Sign(in BigIntSbo256 a)
    {
        fixed (ulong* pA = &a._d0)
        {
            return NativeMethods.bigint_sign((IntPtr)pA);
        }
    }

    public static unsafe bool IsZero(in BigIntSbo256 a)
    {
        fixed (ulong* pA = &a._d0)
        {
            return NativeMethods.bigint_is_zero((IntPtr)pA) != 0;
        }
    }

    public override unsafe string ToString()
    {
        fixed (ulong* ptr = &_d0)
        {
            nuint len = NativeMethods.bigint_to_str_len((IntPtr)ptr, 10);
            int bufLen = checked((int)len + 2);
            byte* buf = stackalloc byte[bufLen];
            NativeMethods.bigint_to_str((IntPtr)ptr, buf, (nuint)bufLen, 10);
            int actual = 0;
            while (actual < bufLen && buf[actual] != 0) actual++;
            return Encoding.ASCII.GetString(buf, actual);
        }
    }
}
