# BigInt Rewrite — Design

**Date:** 2026-07-11
**Status:** Approved
**Goal:** Replace `src/include/bigint.h` (a locally modified copy of faheel/BigInt, MIT) with an original, substantially faster implementation. Strict drop-in: no changes to `ECCrypto.cpp`, tests, or the build.

## Motivation

The current BigInt stores numbers as decimal strings and performs all
arithmetic digit-by-digit in base 10 with heavy string allocation. ECC
signing/verification (hundreds of 256-bit multiplications, divisions, and
modular inversions per operation) is minutes-slow in unoptimized builds and
dominates the test suite's runtime in Release. The user also wants an
implementation that is not derived from another project's code.

## Requirements

1. **Strict drop-in.** Same class name, same header path, header-only, same
   public API. `ECCrypto.cpp`, all tests, and CMake compile unchanged.
2. **Original implementation.** Written from textbook algorithm descriptions
   (schoolbook arithmetic, Knuth TAOCP Vol. 2 Algorithm D). No code taken
   from or written by reference to the old header or upstream faheel/BigInt.
   The MIT attribution block is removed; the old code stays in git history.
3. **Performance.** 64-bit limb representation with `unsigned __int128`
   intermediates (available on the project's pinned w64devkit GCC, x64).
   Target: full test suite drops well below the current ~128 s Release run;
   ECC self-test usable even at -O0.

## Representation

- Sign-magnitude: `bool negative` + `std::vector<uint64_t> limbs`,
  little-endian (limb 0 least significant).
- Canonical form: no high zero limbs; zero is the empty limb vector and is
  never negative.

## Semantics (critical compatibility contract)

The vendored header was locally modified to **floored division** (unlike
upstream, which truncates). The ECC layer depends on this: it feeds negative
values into `% p` and `inverse()` and expects results in `[0, p)`.

- `a / b` rounds toward negative infinity.
- `a % b` has the sign of the divisor `b` (or is zero), and
  `a == (a/b)*b + (a%b)` always holds.
  Consequence: for positive `b`, `a % b` is in `[0, b)` for any `a`.
- Division or modulo by zero throws `std::logic_error`.
- `pow(base, exp)`: preserves existing quirks — negative `exp` returns
  `base` if `|base| == 1` else `0` (throws `std::logic_error` for base 0);
  `pow(0, 0)` throws `std::logic_error`.
- `to_int()`: converts via decimal string + `std::stoi`-equivalent behavior;
  throws `std::out_of_range` when the value does not fit an `int`.
- `to_binary_string()`: magnitude bits, MSB first, no leading zeros, `"0"`
  for zero, `-` prefix for negative values.
- `BigInt(std::string)` accepts optional leading `-`/`+` and decimal digits;
  `BigInt(std::string, int base)` accepts bases 2–36 (digits `0-9a-z`,
  case-insensitive). Invalid input throws `std::invalid_argument`.

## Algorithms

| Operation | Algorithm |
|---|---|
| add/sub | limb-wise carry/borrow on magnitudes; sign logic decides add vs subtract |
| multiply | schoolbook, `__int128` accumulator (4–8 limb operands; Karatsuba is YAGNI) |
| divide/mod | single-limb divisor fast path (one `__int128` pass); Knuth Algorithm D for multi-limb divisors; floored adjustment applied after magnitude division |
| from-string | chunked multiply-accumulate (19 decimal digits or equivalent per chunk) |
| to_string | repeated division by 10^19, format chunks |
| to_binary_string | direct limb bit-walk, no divisions |
| `inverse(a, mod)` | extended Euclidean on `a mod m` (normalized non-negative first), result normalized into `[0, mod)` |
| `squared(x)` | `x * x` directly |
| `abs`, `pow` | trivial / exponentiation by squaring |

Division performance is the load-bearing choice: `inverse()` performs
hundreds of divisions per ECC point addition, so Algorithm D (vs bit-by-bit
shift-subtract) is worth roughly 50–100× exactly where the ECC layer spends
its time.

## Public API (unchanged)

Constructors: `BigInt()`, copy, `BigInt(const long long&)`,
`BigInt(const std::string&)`, `BigInt(const std::string&, int base)`.
Assignment from `BigInt` and `long long`. Unary `+` `-`. Binary `+ - * / %`
against `BigInt`, and `* / %` against `long long`. Compound
`+= -= *= /= %=` (BigInt) and `/=` (long long). All six comparisons against
`BigInt` and against `long long`. Conversions: `to_string()`,
`to_binary_string()`, `to_int()`. Free functions: `abs`, `pow(BigInt, int)`,
`squared`, `inverse(a, mod)`.

The old header's internal helpers (`is_valid_number`,
`strip_leading_zeroes`, `divide`, …) are not part of the used surface and are
not reproduced. No stream operators (the trimmed vendored copy has none).

## Testing

TDD against the existing suite plus new failing-first tests in
`tests/unit/bigint_test.cpp`:

- floored `/` and `%` across all four sign combinations, including
  exact-multiple cases
- carry/borrow boundaries: values around 2^64 and 2^128
- Knuth-D stress: divisors slightly smaller than dividends,
  quotient-digit correction cases
- base-2/base-16 construction and `to_binary_string` round-trips
- `to_int` in-range and out-of-range behavior
- `inverse` against known secp256k1 fixtures
  (e.g. `inverse(2, P) * 2 % P == 1`, negative inputs)

Acceptance: entire suite (67+ cases) green including ECC sign/verify,
wallet, and lifecycle integration tests; before/after timing of the full
suite recorded in the PR/commit message.

## Rejected alternatives

- **Binary shift-subtract division:** simpler, but ~50–100× slower where the
  ECC layer spends its time.
- **Fixed 512-bit width + Montgomery multiplication:** fastest option but no
  longer a general-purpose arbitrary-precision type, larger clean-room
  surface, and unnecessary for the drop-in goal.
- **32-bit limbs:** portable to MSVC, but the build is pinned to GCC and
  64-bit limbs are 2–4× faster.
