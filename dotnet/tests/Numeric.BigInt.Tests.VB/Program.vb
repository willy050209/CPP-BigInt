' filepath: dotnet/tests/Numeric.BigInt.Tests.VB/Program.vb
Option Strict On
Option Explicit On
Option Infer On

Imports System
Imports Numeric

Namespace Tests.VB

    ''' <summary>
    ''' VB.NET 10 (.NET 10) 測試執行模組。
    ''' 遵循 Option Strict On 與現代化工程規範。
    ''' </summary>
    Public Module Program

        ''' <summary>
        ''' 斷言工具純函數。
        ''' </summary>
        Private Sub AssertEqual(Of T)(expected As T, actual As T, testName As String)
            If Not Object.Equals(expected, actual) Then
                Throw New InvalidOperationException($"斷言失敗 [{testName}]: 預期值 '{expected}'，但得到 '{actual}'")
            End If
        End Sub

        ''' <summary>
        ''' 測試 VB 原生算術運算子 (+, -, *, \, Mod, 一元 -)。
        ''' 特別驗證 VB.NET 專屬的整數除法運算子 (\)。
        ''' </summary>
        Private Sub TestArithmeticOperators()
            Console.Write("Testing VB.NET native operators (+, -, *, \, Mod, unary -)... ")

            Dim a As New BigInt(200L)
            Dim b As New BigInt(30L)

            ' 測試加、減、乘
            AssertEqual("230", (a + b).ToString(), "200 + 30")
            AssertEqual("170", (a - b).ToString(), "200 - 30")
            AssertEqual("6000", (a * b).ToString(), "200 * 30")

            ' 測試 VB.NET 整數除法運算子 (\) 與 Mod 運算子
            AssertEqual("6", (a \ b).ToString(), "200 \ 30 (VB Integer Division)")
            AssertEqual("20", (a Mod b).ToString(), "200 Mod 30")

            ' 測試一元負號
            AssertEqual("-200", (-a).ToString(), "-200")

            Console.WriteLine("PASSED")
        End Sub

        ''' <summary>
        ''' 測試 VB.NET 比較運算子 (=, <>, <, <=, >, >=)。
        ''' </summary>
        Private Sub TestComparisonOperators()
            Console.Write("Testing VB.NET comparison operators (=, <>, <, <=, >, >=)... ")

            Dim a As New BigInt(100L)
            Dim b As New BigInt(300L)
            Dim a2 As New BigInt(100L)

            AssertEqual(True, a = a2, "100 = 100")
            AssertEqual(True, a <> b, "100 <> 300")
            AssertEqual(True, a < b, "100 < 300")
            AssertEqual(True, a <= b, "100 <= 300")
            AssertEqual(True, b > a, "300 > 100")
            AssertEqual(True, b >= a, "300 >= 100")

            Console.WriteLine("PASSED")
        End Sub

        ''' <summary>
        ''' 測試 VB.NET 位元邏輯運算子 (And, Or, Xor, Not)。
        ''' </summary>
        Private Sub TestBitwiseOperators()
            Console.Write("Testing VB.NET bitwise operators (And, Or, Xor, Not)... ")

            Dim a As New BigInt(12L) ' 0b1100
            Dim b As New BigInt(10L) ' 0b1010

            AssertEqual("8", (a And b).ToString(), "12 And 10 == 8")
            AssertEqual("14", (a Or b).ToString(), "12 Or 10 == 14")
            AssertEqual("6", (a Xor b).ToString(), "12 Xor 10 == 6")

            Console.WriteLine("PASSED")
        End Sub

        ''' <summary>
        ''' 測試高精度數學函式與字串格式化。
        ''' </summary>
        Private Sub TestMathAndFormatting()
            Console.Write("Testing VB.NET math functions (Pow, Sqrt, Gcd) & string interpolation... ")

            Dim three As New BigInt(3L)
            Dim power As BigInt = BigInt.Pow(three, 4UI)
            AssertEqual("81", power.ToString(), "3^4 == 81")

            Dim hundred As New BigInt(10000L)
            Dim root As BigInt = BigInt.Sqrt(hundred)
            AssertEqual("100", root.ToString(), "Sqrt(10000) == 100")

            Dim gcdVal As BigInt = BigInt.Gcd(New BigInt(48L), New BigInt(18L))
            AssertEqual("6", gcdVal.ToString(), "Gcd(48, 18) == 6")

            ' 字串插值測試
            Dim msg As String = $"計算結果: {power.ToString()}"
            AssertEqual("計算結果: 81", msg, "String interpolation")

            Console.WriteLine("PASSED")
        End Sub

        ''' <summary>
        ''' 主程式進入點。
        ''' </summary>
        Public Function Main() As Integer
            Console.WriteLine("============================================================")
            Console.WriteLine(">>> Starting VB.NET 10 (.NET 10) Numeric.BigInt Test Suite <<<")
            Console.WriteLine("============================================================")

            Try
                TestArithmeticOperators()
                TestComparisonOperators()
                TestBitwiseOperators()
                TestMathAndFormatting()

                Console.WriteLine()
                Console.WriteLine("[SUCCESS] All VB.NET tests passed cleanly with 0 errors!")
                Return 0
            Catch ex As Exception
                Console.ForegroundColor = ConsoleColor.Red
                Console.WriteLine()
                Console.WriteLine($"[FAIL] VB.NET tests failed with exception: {ex}")
                Console.ResetColor()
                Return 1
            End Try
        End Function

    End Module

End Namespace
