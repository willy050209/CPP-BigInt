using System;
using Numeric.Native;

namespace Numeric;

public sealed partial class BigInt
{
    public static BigInt Pow(BigInt baseValue, uint exponent)
    {
        ArgumentNullException.ThrowIfNull(baseValue);
        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_pow(result.Handle, baseValue.Handle, exponent);
        CheckStatus(status);
        return result;
    }

    public static BigInt GreatestCommonDivisor(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_gcd(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt Gcd(BigInt left, BigInt right) => GreatestCommonDivisor(left, right);

    public static BigInt LeastCommonMultiple(BigInt left, BigInt right)
    {
        ArgumentNullException.ThrowIfNull(left);
        ArgumentNullException.ThrowIfNull(right);
        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_lcm(result.Handle, left.Handle, right.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt Lcm(BigInt left, BigInt right) => LeastCommonMultiple(left, right);

    public static BigInt IntegerSquareRoot(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);
        if (value.Sign < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(value), "Square root of negative number is undefined.");
        }

        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_isqrt(result.Handle, value.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt Sqrt(BigInt value) => IntegerSquareRoot(value);

    public static BigInt DivRem(BigInt dividend, BigInt divisor, out BigInt remainder)
    {
        ArgumentNullException.ThrowIfNull(dividend);
        ArgumentNullException.ThrowIfNull(divisor);
        if (divisor.IsZero)
        {
            throw new DivideByZeroException("Division by zero in DivRem.");
        }

        BigInt quotient = CreateRaw();
        BigInt rem = CreateRaw();
        int status = NativeMethods.bigint_div_rem(quotient.Handle, rem.Handle, dividend.Handle, divisor.Handle);
        CheckStatus(status);
        remainder = rem;
        return quotient;
    }

    public static BigInt Abs(BigInt value)
    {
        ArgumentNullException.ThrowIfNull(value);
        BigInt result = CreateRaw();
        int status = NativeMethods.bigint_abs(result.Handle, value.Handle);
        CheckStatus(status);
        return result;
    }

    public static BigInt Min(BigInt val1, BigInt val2)
    {
        ArgumentNullException.ThrowIfNull(val1);
        ArgumentNullException.ThrowIfNull(val2);
        return val1 <= val2 ? val1 : val2;
    }

    public static BigInt Max(BigInt val1, BigInt val2)
    {
        ArgumentNullException.ThrowIfNull(val1);
        ArgumentNullException.ThrowIfNull(val2);
        return val1 >= val2 ? val1 : val2;
    }

    public static int Compare(BigInt? left, BigInt? right)
    {
        if (ReferenceEquals(left, right)) return 0;
        if (left is null) return -1;
        if (right is null) return 1;
        return left.CompareTo(right);
    }
}
