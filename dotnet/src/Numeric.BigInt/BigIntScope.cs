using System;
using System.Runtime.InteropServices;
using System.Text;
using Numeric.Native;

namespace Numeric;

/// <summary>
/// High-performance stack-only arena (ref struct) for zero-GC allocation of SBO BigInt variables.
/// Manages unmanaged/stack buffers and automatically frees all native instances on scope exit.
/// </summary>
public unsafe ref struct BigIntScope : IDisposable
{
    private const int SlotSize = 64; // sizeof(bigint_sbo256_t)
    private const int DefaultCapacity = 16;

    private byte* _buffer;
    private readonly bool _ownsNativeMemory;
    private readonly int _capacity;
    private int _count;

    public BigIntScope()
    {
        _capacity = DefaultCapacity;
        _buffer = (byte*)NativeMemory.AllocZeroed((nuint)(_capacity * SlotSize));
        _ownsNativeMemory = true;
        _count = 0;
    }

    public BigIntScope(Span<byte> stackBuffer)
    {
        if (stackBuffer.Length < SlotSize)
        {
            throw new ArgumentException($"Buffer must be at least {SlotSize} bytes.", nameof(stackBuffer));
        }

        _capacity = stackBuffer.Length / SlotSize;
        fixed (byte* p = stackBuffer)
        {
            _buffer = p;
        }
        _ownsNativeMemory = false;
        _count = 0;
    }

    public IntPtr Create(long value = 0L)
    {
        IntPtr slot = AllocateSlot();
        NativeMethods.bigint_init_sbo256(slot, value);
        return slot;
    }

    public IntPtr Create(ulong value)
    {
        IntPtr slot = AllocateSlot();
        NativeMethods.bigint_init_sbo256_u64(slot, value);
        return slot;
    }

    public IntPtr Create(ReadOnlySpan<char> str, int radix = 10)
    {
        IntPtr slot = AllocateSlot();
        NativeMethods.bigint_init_sbo256_str(slot, str.ToString(), radix);
        return slot;
    }

    private IntPtr AllocateSlot()
    {
        if (_count >= _capacity)
        {
            throw new InvalidOperationException($"BigIntScope capacity ({_capacity} slots) exceeded.");
        }

        byte* ptr = _buffer + (_count * SlotSize);
        _count++;
        return (IntPtr)ptr;
    }

    /* Fast In-Scope Arithmetic */

    public void Add(IntPtr result, IntPtr a, IntPtr b) => NativeMethods.bigint_add(result, a, b);
    public void Subtract(IntPtr result, IntPtr a, IntPtr b) => NativeMethods.bigint_sub(result, a, b);
    public void Multiply(IntPtr result, IntPtr a, IntPtr b) => NativeMethods.bigint_mul(result, a, b);
    public void Divide(IntPtr result, IntPtr a, IntPtr b) => NativeMethods.bigint_div(result, a, b);
    public void Modulus(IntPtr result, IntPtr a, IntPtr b) => NativeMethods.bigint_mod(result, a, b);
    public int Compare(IntPtr a, IntPtr b) => NativeMethods.bigint_cmp(a, b);
    public int Sign(IntPtr a) => NativeMethods.bigint_sign(a);
    public bool IsZero(IntPtr a) => NativeMethods.bigint_is_zero(a) != 0;

    public BigInt ToBigInt(IntPtr handle)
    {
        IntPtr clone = NativeMethods.bigint_clone(handle);
        if (clone == IntPtr.Zero)
        {
            throw new OutOfMemoryException("Failed to clone BigInt from scope handle.");
        }
        return new BigInt(new BigIntHandle(clone));
    }

    public string ToString(IntPtr handle, int radix = 10)
    {
        nuint len = NativeMethods.bigint_to_str_len(handle, radix);
        int bufLen = checked((int)len + 2);
        byte* buf = stackalloc byte[bufLen];
        NativeMethods.bigint_to_str(handle, buf, (nuint)bufLen, radix);
        int actual = 0;
        while (actual < bufLen && buf[actual] != 0) actual++;
        return Encoding.ASCII.GetString(buf, actual);
    }

    public void Dispose()
    {
        if (_buffer != null)
        {
            for (int i = 0; i < _count; i++)
            {
                byte* slot = _buffer + (i * SlotSize);
                NativeMethods.bigint_destroy_sbo256((IntPtr)slot);
            }

            if (_ownsNativeMemory)
            {
                NativeMemory.Free(_buffer);
            }

            _buffer = null;
            _count = 0;
        }
    }
}
