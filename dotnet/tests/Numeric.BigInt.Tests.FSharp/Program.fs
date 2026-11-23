namespace Numeric.BigIntTests.FSharp

open System
open Numeric

/// <summary>
/// Discriminated union representing symbolic BigInt expressions.
/// </summary>
type MathExpr =
    | Value of BigInt
    | Add of MathExpr * MathExpr
    | Sub of MathExpr * MathExpr
    | Mul of MathExpr * MathExpr
    | Div of MathExpr * MathExpr

module Evaluator =
    /// <summary>
    /// Pure functional evaluator using pattern matching.
    /// </summary>
    let rec evaluate (expr: MathExpr) : Result<BigInt, string> =
        match expr with
        | Value v -> Ok v
        | Add (left, right) ->
            match evaluate left, evaluate right with
            | Ok l, Ok r -> Ok (l + r)
            | Error err, _ | _, Error err -> Error err
        | Sub (left, right) ->
            match evaluate left, evaluate right with
            | Ok l, Ok r -> Ok (l - r)
            | Error err, _ | _, Error err -> Error err
        | Mul (left, right) ->
            match evaluate left, evaluate right with
            | Ok l, Ok r -> Ok (l * r)
            | Error err, _ | _, Error err -> Error err
        | Div (left, right) ->
            match evaluate left, evaluate right with
            | Ok l, Ok r ->
                if r.IsZero then
                    Error "Division by zero"
                else
                    Ok (l / r)
            | Error err, _ | _, Error err -> Error err

module Program =

    let private assertEqual (expected: 'T) (actual: 'T) (name: string) : unit =
        if expected <> actual then
            failwithf "Assertion failed for %s. Expected: %A, Got: %A" name expected actual

    let private testArithmetic () : unit =
        printf "Testing F# native operators (+, -, *, /, %%, unary -)... "
        let a = BigInt(120L)
        let b = BigInt(45L)

        assertEqual "165" ((a + b).ToString()) "120 + 45"
        assertEqual "75" ((a - b).ToString()) "120 - 45"
        assertEqual "5400" ((a * b).ToString()) "120 * 45"
        assertEqual "2" ((a / b).ToString()) "120 / 45"
        assertEqual "30" ((a % b).ToString()) "120 % 45"
        assertEqual "-120" ((-a).ToString()) "-120"
        printfn "PASSED"

    let private testComparisons () : unit =
        printf "Testing F# equality and comparisons (=, <>, <, <=, >, >=)... "
        let a = BigInt(500L)
        let b = BigInt(1000L)
        let a2 = BigInt(500L)

        assertEqual true (a = a2) "500 = 500"
        assertEqual true (a <> b) "500 <> 1000"
        assertEqual true (a < b) "500 < 1000"
        assertEqual true (a <= b) "500 <= 1000"
        assertEqual true (b > a) "1000 > 500"
        assertEqual true (b >= a) "1000 >= 500"
        printfn "PASSED"

    let private testPipeline () : unit =
        printf "Testing F# pipeline operator (|>) and chained transformations... "
        let result =
            BigInt.Parse("100")
            |> fun x -> x + BigInt(50L)
            |> fun x -> x * BigInt(2L)
            |> fun x -> x - BigInt(20L)
            |> fun x -> x / BigInt(4L)
            |> fun x -> x.ToString()

        assertEqual "70" result "Pipeline: ((100 + 50) * 2 - 20) / 4 == 70"
        printfn "PASSED"

    let private testDiscriminatedUnions () : unit =
        printf "Testing Discriminated Unions and Pattern Matching... "
        // Expression: (100 + 20) * (50 - 10) / 2
        let expr =
            Div (
                Mul (
                    Add (Value (BigInt(100L)), Value (BigInt(20L))),
                    Sub (Value (BigInt(50L)), Value (BigInt(10L)))
                ),
                Value (BigInt(2L))
            )

        match Evaluator.evaluate expr with
        | Ok res ->
            assertEqual "2400" (res.ToString()) "((100+20)*(50-10))/2 == 2400"
        | Error err ->
            failwithf "Evaluation failed unexpectedly: %s" err

        // Division by zero branch test
        let divZeroExpr = Div (Value (BigInt(10L)), Value (BigInt(0L)))
        match Evaluator.evaluate divZeroExpr with
        | Ok _ -> failwith "Expected division by zero error, but got Ok"
        | Error err -> assertEqual "Division by zero" err "Div by zero error message"

        printfn "PASSED"

    [<EntryPoint>]
    let main _ =
        printfn "============================================================"
        printfn ">>> Starting F# 10 (.NET 10) Numeric.BigInt Test Suite <<<"
        printfn "============================================================"

        try
            testArithmetic ()
            testComparisons ()
            testPipeline ()
            testDiscriminatedUnions ()

            printfn "\n[SUCCESS] All F# tests passed cleanly with 0 errors!"
            0
        with ex ->
            eprintfn "\n[FAIL] F# tests failed with exception: %s" (ex.ToString())
            1
