using System;
using System.Runtime.CompilerServices;
using Numeric.Native;

namespace Numeric;

public sealed partial class BigInt
{
    /* Arithmetic Operators */

    public static BigInt operator +(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_add(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator -(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_sub(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator *(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_mul(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator /(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        if (right.IsZero) throw new DivideByZeroException("Division by zero in BigInt.");

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_div(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator %(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        if (right.IsZero) throw new DivideByZeroException("Modulo by zero in BigInt.");

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_mod(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator -(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_neg(result.Handle, value.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator +(BigInt value) => value;

    public static BigInt operator ++(BigInt value) => value + One;

    public static BigInt operator --(BigInt value) => value - One;

    /* Bitwise Operators */

    public static BigInt operator &(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_and(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator |(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_or(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator ^(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_xor(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator ~(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_not(result.Handle, value.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator <<(BigInt value, int shift)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (shift < 0) return value >> -shift;

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_shl(result.Handle, value.Handle, (nuint)shift);
        CheckStatus(status);
        return result;
    }

    public static BigInt operator >>(BigInt value, int shift)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (shift < 0) return value << -shift;

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_shr(result.Handle, value.Handle, (nuint)shift);
        CheckStatus(status);
        return result;
    }

    /* Comparison Operators */

    public static bool operator ==(BigInt? left, BigInt? right)
    {
        if (ReferenceEquals(left, right)) return true;
        if (left is null || right is null) return false;
        return left.CompareTo(right) == 0;
    }

    public static bool operator !=(BigInt? left, BigInt? right) => !(left == right);

    public static bool operator <(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        return left.CompareTo(right) < 0;
    }

    public static bool operator <=(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        return left.CompareTo(right) <= 0;
    }

    public static bool operator >(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        return left.CompareTo(right) > 0;
    }

    public static bool operator >=(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        return left.CompareTo(right) >= 0;
    }

    /* VB.NET Special Operator (\: Integer Division) */

    [SpecialName]
    public static BigInt op_IntegerDivision(BigInt left, BigInt right) => left / right;

    /* Named Helper Methods (for VB.NET, F# and other languages) */

    public static BigInt Add(BigInt left, BigInt right) => left + right;
    public static BigInt Subtract(BigInt left, BigInt right) => left - right;
    public static BigInt Multiply(BigInt left, BigInt right) => left * right;
    public static BigInt Divide(BigInt left, BigInt right) => left / right;
    public static BigInt Modulus(BigInt left, BigInt right) => left % right;
    public static BigInt Negate(BigInt value) => -value;
    public static BigInt BitwiseAnd(BigInt left, BigInt right) => left & right;
    public static BigInt BitwiseOr(BigInt left, BigInt right) => left | right;
    public static BigInt Xor(BigInt left, BigInt right) => left ^ right;
    public static BigInt OnesComplement(BigInt value) => ~value;
    public static BigInt LeftShift(BigInt value, int shift) => value << shift;
    public static BigInt RightShift(BigInt value, int shift) => value >> shift;

    private static void CheckStatus(int status)
    {
        if (status == 0) return;
        throw status switch
        {
            1 => new ArgumentException("Invalid argument in native BigInt operation."),
            2 => new DivideByZeroException("Division by zero in native BigInt operation."),
            3 => new ArgumentException("Buffer too small in native BigInt operation."),
            4 => new ArgumentNullException(nameof(status), "Null pointer in native BigInt operation."),
            5 => new OutOfMemoryException("Out of memory in native BigInt operation."),
            6 => new OverflowException("Overflow in native BigInt operation."),
            _ => new InvalidOperationException($"Native BigInt error with status code {status}.")
        };
    }
}
