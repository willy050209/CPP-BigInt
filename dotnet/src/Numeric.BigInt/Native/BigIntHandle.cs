using System;
using Microsoft.Win32.SafeHandles;

namespace Numeric.Native;

internal sealed class BigIntHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public BigIntHandle() : base(true)
    {
    }

    public BigIntHandle(IntPtr preexistingHandle, bool ownsHandle = true) : base(ownsHandle)
    {
        SetHandle(preexistingHandle);
    }

    protected override bool ReleaseHandle()
    {
        if (!IsInvalid)
        {
            NativeMethods.bigint_destroy(handle);
            handle = IntPtr.Zero;
        }
        return true;
    }
}
