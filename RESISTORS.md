# Combining Resistors to Hit a Target Value

You usually only own a handful of resistor values (a typical kit jumps
10, 22, 47, 100, 220, 330, 470, 1k, 2.2k, 4.7k, 10k ...). When you need a
value you don't have, you combine two you *do* have. There are exactly
two ways to wire them.

## The two rules

### Series — values ADD

```
  ──[ R1 ]──[ R2 ]──
```

```
R_total = R1 + R2
```

Series always gives you a **bigger** resistance than either part.
Example: 220 + 100 = **320 Ω** (close to 330).

### Parallel — the reciprocals add

```
      ┌─[ R1 ]─┐
  ────┤        ├────
      └─[ R2 ]─┘
```

```
1/R_total = 1/R1 + 1/R2

           R1 * R2
R_total = ─────────       (handy form for exactly two resistors)
           R1 + R2
```

Parallel always gives you a resistance **smaller** than the smallest
part. Two equal resistors in parallel = half the value (e.g. 1k || 1k =
500 Ω).

> Notation: `R1 || R2` means "R1 in parallel with R2".

## "Current splitting" vs. "making a value"

The term *current splitting* (a current divider) describes what
physically happens in a parallel pair: the total current splits between
the two branches, **inversely** proportional to their resistance — the
smaller resistor hogs more current.

```
I_through_R1 = I_total * R2 / (R1 + R2)
```

But when you're trying to *synthesize a resistance* (e.g. "I need
330 Ω"), the thing you actually care about is the **equivalent
resistance** from the two rules above. So this doc is really about
picking R1 and R2 so their combination equals your target.

## Your example: 1k and 100 Ω → can you get 330?

Let's just run both rules on those two parts:

| Wiring   | Math                          | Result      |
|----------|-------------------------------|-------------|
| Series   | 1000 + 100                    | **1100 Ω**  |
| Parallel | (1000 * 100) / (1000 + 100)   | **90.9 Ω**  |

So **no** — a 1k and a 100 Ω can only ever give you 1100 Ω or 90.9 Ω,
never 330. The reachable values from any *single pair* are just those two
numbers. To land on 330 you need different parts.

## How to actually get ~330 Ω

330 Ω is itself a standard value, so first choice: just buy/grab a 330.
If you don't have one, here are real combinations:

| Combination          | Wiring   | Math                        | Result    |
|----------------------|----------|-----------------------------|-----------|
| 220 + 100            | series   | 220 + 100                   | 320 Ω     |
| 220 + 110            | series   | 220 + 110                   | 330 Ω ✅  |
| 100 + 220            | series   | (same as above)             | 320 Ω     |
| 470 \|\| 1k          | parallel | (470*1000)/1470             | 319.7 Ω   |
| 470 \|\| 1.1k        | parallel | (470*1100)/1570             | 329.3 Ω ✅|
| 1k \|\| 500          | parallel | (1000*500)/1500             | 333 Ω     |
| 390 \|\| 2.2k        | parallel | (390*2200)/2590             | 331.3 Ω ✅|
| three 1k in parallel | parallel | 1000 / 3                     | 333 Ω     |

For an LED current-limiting resistor (the usual reason you want ~330),
being off by 5–10 Ω changes the current by a milliamp or two — totally
fine. Don't chase an exact 330.

## Practical workflow

1. **Need it bigger than what you have?** Put resistors in **series** and
   add.
2. **Need it smaller?** Put them in **parallel**.
3. **Need an in-between oddball value?** Parallel two values that
   bracket it, or series two that sum to it. Compute with the formulas
   above (or an online "resistor combination calculator").
4. **Watch the tolerance.** Most kit resistors are ±5%. A 1k can really
   be 950–1050 Ω, so don't sweat single-ohm precision — the parts
   themselves aren't that precise.
5. **Watch power.** When resistors share current/voltage, each only
   dissipates part of the total power, but check that no single one
   exceeds its rating (usually 1/4 W in a kit). P = I²R = V²/R.

## Quick reference

```
Series:    R = R1 + R2              (always larger)
Parallel:  R = (R1*R2)/(R1+R2)      (always smaller than smallest)
Equal pair in parallel:  R = R1/2
N equal in parallel:     R = R1/N
```
