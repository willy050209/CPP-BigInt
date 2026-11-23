using System;
using System.Diagnostics.CodeAnalysis;
using Numeric.Native;

namespace Numeric;

public sealed partial class BigInt : IComparable<BigInt>, IComparable, IEquatable<BigInt>, IFormattable, IParsable<BigInt>, ISpanParsable<BigInt>, ISpanFormattable
{
    internal readonly BigIntHandle Handle;

    static BigInt()
    {
        NativeResolver.Initialize();
    }

    internal BigInt(BigIntHandle handle)
    {
        ArgumentNullException.ThrowIfNull(handle);
        Handle = handle;
    }

    public BigInt()
    {
        IntPtr ptr = NativeMethods.bigint_create();
        if (ptr == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to allocate native BigInt.");
        }
        Handle = new BigIntHandle(ptr);
    }

    public BigInt(long value)
    {
        IntPtr ptr = NativeMethods.bigint_create_i64(value);
        if (ptr == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to allocate native BigInt from Int64.");
        }
        Handle = new BigIntHandle(ptr);
    }

    public BigInt(ulong value)
    {
        IntPtr ptr = NativeMethods.bigint_create_u64(value);
        if (ptr == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to allocate native BigInt from UInt64.");
        }
        Handle = new BigIntHandle(ptr);
    }

    public BigInt(int value) : this((long)value)
    {
    }

    public BigInt(uint value) : this((ulong)value)
    {
    }

    public BigInt(string value, int radix = 10)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (radix < 2 || radix > 36)
        {
            throw new ArgumentOutOfRangeException(nameof(radix), "Radix must be between 2 and 36.");
        }

        IntPtr ptr = NativeMethods.bigint_create_str(value, radix);
        if (ptr == IntPtr.Zero)
        {
            throw new FormatException($"Invalid BigInt string '{value}' for radix {radix}.");
        }
        Handle = new BigIntHandle(ptr);
    }

    public BigInt(ReadOnlySpan<char> value, int radix = 10) : this(value.ToString(), radix)
    {
    }

    /* Standard Constants */
    public static BigInt Zero { get; } = new(0L);
    public static BigInt One { get; } = new(1L);
    public static BigInt MinusOne { get; } = new(-1L);

    /* Properties */
    public int Sign => NativeMethods.bigint_sign(Handle);
    public bool IsZero => NativeMethods.bigint_is_zero(Handle) != 0;
    public bool IsOne => this == One;
    public bool IsEven => (this & One).IsZero;
    public int BitLength => (int)NativeMethods.bigint_bit_length(Handle);
    public int LimbCount => (int)NativeMethods.bigint_limb_count(Handle);
    public bool IsSbo => NativeMethods.bigint_is_sbo(Handle) != 0;

    internal static BigInt CreateRaw()
    {
        IntPtr ptr = NativeMethods.bigint_create();
        if (ptr == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to allocate native BigInt.");
        }
        return new BigInt(new BigIntHandle(ptr));
    }

    public BigInt Clone()
    {
        IntPtr ptr = NativeMethods.bigint_clone(Handle);
        if (ptr == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to clone BigInt.");
        }
        return new BigInt(new BigIntHandle(ptr));
    }

    /* Type Conversions */
    public static implicit operator BigInt(byte value) => new((long)value);
    public static implicit operator BigInt(sbyte value) => new((long)value);
    public static implicit operator BigInt(short value) => new((long)value);
    public static implicit operator BigInt(ushort value) => new((ulong)value);
    public static implicit operator BigInt(int value) => new((long)value);
    public static implicit operator BigInt(uint value) => new((ulong)value);
    public static implicit operator BigInt(long value) => new(value);
    public static implicit operator BigInt(ulong value) => new(value);

    public static explicit operator long(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);
        long result = NativeMethods.bigint_to_i64(value.Handle, out int status);
        if (status != 0)
        {
            throw new OverflowException("BigInt value is out of range for Int64.");
        }
        return result;
    }

    public static explicit operator ulong(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);
        ulong result = NativeMethods.bigint_to_u64(value.Handle, out int status);
        if (status != 0)
        {
            throw new OverflowException("BigInt value is out of range for UInt64.");
        }
        return result;
    }

    public static explicit operator int(BigInt value) => checked((int)(long)value);
    public static explicit operator uint(BigInt value) => checked((uint)(ulong)value);

    public static explicit operator double(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);
        return double.Parse(value.ToString());
    }

    /* Comparison Interfaces */
    public int CompareTo(BigInt? other)
    {
        if (other is null) return 1;
        return NativeMethods.bigint_cmp(Handle, other.Handle);
    }

    public int CompareTo(object? obj)
    {
        if (obj is null) return 1;
        if (obj is BigInt other) return CompareTo(other);
        throw new ArgumentException("Object must be of type BigInt", nameof(obj));
    }

    public bool Equals(BigInt? other)
    {
        if (other is null) return false;
        if (ReferenceEquals(this, other)) return true;
        return CompareTo(other) == 0;
    }

    public override bool Equals([NotNullWhen(true)] object? obj)
    {
        return obj is BigInt other && Equals(other);
    }

    public override int GetHashCode()
    {
        // Stable 64-bit hash from sign and bit length
        return HashCode.Combine(Sign, BitLength, LimbCount);
    }
}
