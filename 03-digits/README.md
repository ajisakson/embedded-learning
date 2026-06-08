# 03-digits — driving a bare 4-digit 7-segment display by multiplexing

This project drives a **raw 4-digit 7-segment display** — the kind with ~12 bare
pins and _no driver chip_ — straight from the ESP32's GPIOs. You'll show a number
across all four digits and, as a stretch, turn it into a counting clock.

The whole point of using a _bare_ display (instead of a TM1637 or MAX7219 module
that does the hard part for you) is to learn the one trick that makes these things
work: **multiplexing**. A 4-digit display has 32 LEDs but only ~12 pins, so you
_can't_ light all four digits independently at once. Instead you light **one digit
at a time, very fast**, cycling through all four hundreds of times a second. Your
eye blurs them together and sees four solid digits. That illusion — _persistence
of vision_ — is this entire project.

Target chip: **ESP32** (WROOM/WROVER family).
Toolchain: **ESP-IDF v6.0.1**.

> **New here?** This guide assumes your toolchain is already set up. If you
> haven't installed Homebrew, ESP-IDF, and the USB-serial driver yet, do
> [`01-blink`](../01-blink/README.md) first — steps 1–8 there get your Mac ready,
> and everything carries over.

---

## 1. How a 7-segment digit works

Each digit is just **8 LEDs** arranged in the familiar figure-8, named `a`–`g`
plus the decimal point `dp`:

```
     aaaa
    f    b
    f    b
     gggg
    e    c
    e    c
     dddd   • dp
```

You make any numeral by lighting the right combination of segments. Here's the
full map for `0`–`9` (the segments that are **ON**):

| Digit | Segments lit        |
| :---: | ------------------- |
|  `0`  | a b c d e f         |
|  `1`  | b c                 |
|  `2`  | a b g e d           |
|  `3`  | a b g c d           |
|  `4`  | f g b c             |
|  `5`  | a f g c d           |
|  `6`  | a f g e d c         |
|  `7`  | a b c               |
|  `8`  | a b c d e f g (all) |
|  `9`  | a b c d f g         |

So "drawing a digit" is nothing more than: set these 8 segment pins on/off to match
one row of that table. (Packed-byte versions of these are in the
[appendix](#appendix--segment-byte-table) if you'd rather store them as hex.)

---

## 2. Why 4 digits share only 12 pins — multiplexing

Four digits × eight segments = **32 LEDs**. Wiring each individually would need 32
pins. Instead the display **ties all four digits' segments together**: every
digit's `a` segment connects to one shared `a` pin, every `b` to one shared `b`
pin, and so on. That's **8 segment pins** total.

Then each digit gets **one "common" pin** that enables _just that digit_. Four
digits → **4 common pins**. 8 + 4 = **12 pins**.

The catch: because the segment lines are shared, whatever pattern you put on them
shows up on **every digit you've enabled at once**. If you enable all four commons
and drive the pattern for `2`, you get `2222` — you cannot show `1234`.

**The fix is time, not wiring.** To show `1234` you:

1. Put the `1` pattern on the segments, enable **only** digit 1, wait ~1–2 ms.
2. Put the `2` pattern on the segments, enable **only** digit 2, wait ~1–2 ms.
3. ...digit 3 → `3`, digit 4 → `4`.
4. Loop forever.

If one full pass through all four digits takes less than ~1/60 s, your eye can't
see the switching and all four look continuously lit. Slow it down and you'll
literally watch it scan — a great thing to try on purpose (see
[experiments](#11-experiments)).

> **Refresh-rate rule of thumb:** aim for a full 4-digit refresh **faster than
> ~60 Hz** (≈16 ms) to kill flicker; ~1 ms per digit (≈250 Hz refresh) looks
> rock-solid. ⚠️ **Timing gotcha:** ESP-IDF's FreeRTOS tick defaults to **100 Hz**,
> so `vTaskDelay(1)` is _10 ms_, not 1 ms — way too slow, you'll see flicker. Use
> **`esp_rom_delay_us()`** for the per-digit dwell (the same microsecond delay you
> used in `02-beep`), or drive the refresh from a hardware timer.

---

## 3. Common cathode vs. common anode — find out which you have

This is the single most important thing to get right, because it **flips all your
logic**. Bare displays come in two flavors:

- **Common cathode (CC)** — all the LEDs in a digit share their **negative** legs,
  tied to that digit's common pin. To light a digit you pull its common **LOW
  (GND)** and drive the segment pins **HIGH** to turn segments on.
- **Common anode (CA)** — the LEDs share their **positive** legs. You pull the
  common **HIGH (3.3 V)** and drive segment pins **LOW** to turn segments on
  (everything is inverted).

|                    | Common pin (to enable digit) | Segment pin (to light it) |
| ------------------ | ---------------------------- | ------------------------- |
| **Common cathode** | LOW                          | HIGH                      |
| **Common anode**   | HIGH                         | LOW                       |

**How to tell which you have:**

- **By part number** — e.g. `5641AS` is common cathode; `5461AS` is common anode.
  Search your exact marking.
- **By experiment** — with a coin cell (or 3.3 V through a 330 Ω resistor),
  touch one lead to a likely common pin and the other to a segment pin. If a
  single segment lights when the **common is on `+`**, it's common anode; if it
  lights when the **common is on `−` (GND)**, it's common cathode. (The resistor
  protects the LED — don't connect a bare LED straight across a battery.)

This guide's wiring and explanations assume **common cathode** (the more common
hobby part). If yours is common anode, the wiring is identical — only the
HIGH/LOW logic in your code inverts, and the commons go through their transistors
to **3.3 V** instead of GND (see step 4).

---

## 4. ⚠️ Resistors and current — don't cook the display or a GPIO

LEDs need **current-limiting resistors** or they (and your GPIO pins) get damaged.
Two decisions:

**1. One resistor per _segment_, not per digit.** Put a resistor (≈**330 Ω**;
220–470 Ω is fine) in series with each of the **8 segment lines**. You could
instead put 4 resistors on the common lines (fewer parts), but then a digit
showing `1` (2 segments) glows much brighter than `8` (7 segments), because the
shared resistor's current is split among however many segments are lit. Per-segment
resistors keep brightness even. So: **8 resistors, on a–g and dp.**

**2. Watch the current through the active common pin.** Here's the subtle part of
multiplexing: at any instant _only one digit is lit_, but **all of its lit segments'
current flows through that one common pin**. Showing `8` means 7 segments at once.
At ~330 Ω that's roughly 7 × ~4 mA ≈ **28 mA through a single GPIO** — right at the
ESP32's comfort limit (keep continuous per-pin current ≤ ~20 mA; ~40 mA is the
absolute max).

Two ways to stay safe:

- **Simple / dimmer:** use larger segment resistors (**470 Ω–1 kΩ**) so even an `8`
  stays well under 20 mA on the common. Fine for a first light-up; the display is
  just a bit dim.
- **Recommended / full brightness:** drive each of the **4 common lines through a
  small transistor** so the GPIO only switches the transistor, not the LED current.
  For **common cathode**, use an NPN (e.g. 2N2222): `GPIO → 1 kΩ → base`, the
  display's common pin → collector, emitter → **GND**. The GPIO going HIGH turns
  that digit on. (For **common anode**, use a PNP or a high-side switch to **3.3 V**
  instead.)

For a first build, the simple path is totally fine. Power the board from **USB
only** while testing.

---

## 5. What you need

**Hardware**

- Your ESP32 dev board + USB **data** cable (same as `01-blink`).
- A **bare 4-digit 7-segment display** (e.g. `5641AS` common-cathode, ~12 pins).
- **8 resistors** for the segment lines (≈330 Ω, or 470 Ω–1 kΩ for the simple path).
- A **breadboard** and a fistful of jumper wires (you'll run 12 signal lines).
- _(Recommended)_ **4 NPN transistors** (e.g. 2N2222) + **4 × 1 kΩ** base resistors
  to drive the common lines at full brightness (step 4).

**Software** — same as `01-blink`: ESP-IDF v6.0.1, sourced into your shell.

> **Heads up — this is a wire-heavy project.** Twelve signal lines plus power is a
> lot for a breadboard. Go slow, keep segment wires grouped and digit/common wires
> grouped, and label them. A loose jumper is the #1 cause of a "dead segment."

---

## 6. Find YOUR display's pinout first

⚠️ **Pinouts vary between parts — do not trust a diagram for a different part
number.** A 12-pin display has 6 pins along the top edge and 6 along the bottom.
For the common **`5641AS`** (common cathode), viewed from the **front** (segments
facing you, decimal points along the bottom):

```
   pins:  12   11   10    9    8    7        ← top edge
         ┌────────────────────────────┐
         │   8.   8.   8.   8.         │
         └────────────────────────────┘
   pins:   1    2    3    4    5    6         ← bottom edge
```

| Pin |       Function        | Pin |       Function       |
| :-: | :-------------------: | :-: | :------------------: |
|  1  |     segment **E**     | 12  | **DIG 1** (leftmost) |
|  2  |     segment **D**     | 11  |    segment **A**     |
|  3  |        **DP**         | 10  |    segment **F**     |
|  4  |     segment **C**     |  9  |      **DIG 2**       |
|  5  |     segment **G**     |  8  |      **DIG 3**       |
|  6  | **DIG 4** (rightmost) |  7  |    segment **B**     |

If your part differs, the reliable method is to find the **four common pins** (each
enables one whole digit) and the eight segment pins by experiment with the coin
cell + resistor from step 3, and write your own table. _Get this right before you
wire anything_ — everything downstream depends on it.

---

## 7. Wire it up

The code defines twelve pins — eight for the shared segments, four for the digit
commons. Suggested ESP32 GPIOs (all are safe output pins — they avoid the
input-only pins 34–39, the flash pins 6–11, the strapping pins 0/2/12/15, and the
UART pins 1/3):

**Segment lines** — each through its own **≈330 Ω resistor** to the matching
segment pin on the display:

| Segment | ESP32 GPIO |
| :-----: | :--------: |
|    a    |   **13**   |
|    b    |   **14**   |
|    c    |   **27**   |
|    d    |   **26**   |
|    e    |   **25**   |
|    f    |   **33**   |
|    g    |   **32**   |
|   dp    |   **4**    |

**Digit common lines** — directly (simple path) or **through an NPN transistor**
(recommended, step 4) to each digit's common pin:

|     Digit      | ESP32 GPIO | Display pin (5641AS) |
| :------------: | :--------: | :------------------: |
| D1 (leftmost)  |   **23**   |        pin 12        |
|       D2       |   **22**   |        pin 9         |
|       D3       |   **21**   |        pin 8         |
| D4 (rightmost) |   **19**   |        pin 6         |

The picture (common-cathode, simple path — commons straight to GPIO):

```
  ESP32                                  4-digit display (5641AS, common cathode)
  ┌──────────┐                          ┌───────────────────────────────┐
  │  GPIO13  ├──[330Ω]──── a ───────────┤ shared segment a   (pin 11)    │
  │  GPIO14  ├──[330Ω]──── b ───────────┤ shared segment b   (pin 7)     │
  │  GPIO27  ├──[330Ω]──── c ───────────┤ shared segment c   (pin 4)     │
  │  GPIO26  ├──[330Ω]──── d ───────────┤ shared segment d   (pin 2)     │
  │  GPIO25  ├──[330Ω]──── e ───────────┤ shared segment e   (pin 1)     │
  │  GPIO33  ├──[330Ω]──── f ───────────┤ shared segment f   (pin 10)    │
  │  GPIO32  ├──[330Ω]──── g ───────────┤ shared segment g   (pin 5)     │
  │  GPIO4   ├──[330Ω]──── dp ──────────┤ shared decimal pt  (pin 3)     │
  │          │                          │                               │
  │  GPIO23  ├──────────── D1 common ───┤ digit 1 common     (pin 12)   │
  │  GPIO22  ├──────────── D2 common ───┤ digit 2 common     (pin 9)    │
  │  GPIO21  ├──────────── D3 common ───┤ digit 3 common     (pin 8)    │
  │  GPIO19  ├──────────── D4 common ───┤ digit 4 common     (pin 6)    │
  └──────────┘                          └───────────────────────────────┘
```

Recommended transistor drive on each common (common-cathode), per digit:

```
  GPIO ──[1kΩ]──► base                       2N2222 (NPN)
                   (B)
   digit common ──► collector (C)
                    emitter (E) ──► GND
  GPIO HIGH = digit ON.  Repeat for all 4 commons.
```

> **WROVER note:** if you have a WROVER module (with PSRAM), avoid GPIO16/17 — this
> mapping already does. On a plain WROOM they're free if you need extras.

Plug the board into your Mac with the USB data cable once wired. Power from **USB
only** while testing.

---

## 8. The approach (you'll write this in `main/main.c`)

The code is intentionally left for you — here's the shape of it. You're filling in
[main/main.c](main/main.c), which currently just has an empty `app_main()`.

**a. Configure all 12 GPIOs as outputs.** Same `gpio_reset_pin` /
`gpio_set_direction(..., GPIO_MODE_OUTPUT)` pattern you used in `02-beep`, once per
pin. (A small array of pin numbers + a loop keeps this tidy.)

**b. Store the segment patterns.** Encode the table from step 1 — for each digit
`0`–`9`, which of the 8 segments are on. An array of 10 entries does it (either as
a list of segment flags, or as packed bytes from the
[appendix](#appendix--segment-byte-table)).

**c. Write one function that shows a single digit on one position.** Roughly:

```c
// pseudocode — not the answer, just the shape
void show_digit(int position, int value) {
    all_commons_off();              // blank first, to avoid "ghosting"
    set_segments(pattern[value]);   // drive a–dp for this numeral
    common_on(position);            // enable just this digit
    esp_rom_delay_us(1500);         // dwell ~1.5 ms (NOT vTaskDelay — see §2)
}
```

> **Why `all_commons_off()` first:** if you change the segments _while_ a digit is
> still enabled, that digit briefly shows the wrong pattern — you'll see faint
> "ghost" segments bleeding between digits. Always blank, then set segments, then
> enable. This is the classic multiplexing bug.

**d. The refresh loop.** Split the number you want to show into four digits
(thousands/hundreds/tens/ones via `/` and `%`), then in a tight loop call
`show_digit()` for position 0,1,2,3 over and over. That continuous scanning _is_
the display being "on."

**e. Make it do something.** Start by showing a fixed number like `1234` to prove
the wiring. Then keep a counter and increment it (e.g. once per second) — note you
can't just `vTaskDelay(1000)` between refreshes, or the display goes dark for a
second. Instead keep refreshing constantly and advance the counter based on
elapsed time (`esp_timer_get_time()`), so the display never stops scanning.

Remember the **CC vs CA polarity** from step 3: for common cathode, "segment on" =
GPIO HIGH and "digit enabled" = common LOW (or transistor base HIGH); invert both
for common anode.

---

## 9. Build, flash, and watch

Source the environment (`get_idf`, from `01-blink` step 5) and make sure you're in
this `03-digits` directory. Set the target once, then build/flash/monitor:

```bash
idf.py set-target esp32
idf.py -p /dev/tty.usbserial-0001 build flash monitor
```

(Use your own port from `01-blink` step 8 if it differs.) Once your code is in
place you should see your number standing steady across all four digits. Press
**`Ctrl-]`** to exit the monitor.

A good bring-up order: light **one segment** on **one digit** first (hard-code it),
then a whole digit, then all four via the multiplex loop. Don't wire all 12 lines
and expect `1234` on the first try.

---

## 10. Troubleshooting

| Symptom                                           | Likely cause / fix                                                                                                                                             |
| ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Whole display dark                                | No digit common is being enabled, or CC/CA polarity inverted (commons need LOW for CC). Confirm power/GND and that your refresh loop is actually running.      |
| All four digits show the **same** number          | You're enabling all commons at once (or never disabling the previous one) — that's the un-multiplexed failure. Enable exactly one digit per `show_digit` call. |
| Visible **flicker** or scanning                   | Refresh too slow. You're probably using `vTaskDelay(1)` (=10 ms at the 100 Hz tick). Switch the per-digit dwell to `esp_rom_delay_us()` (§2).                  |
| Faint **ghost** segments between digits           | Not blanking before switching digits. Turn all commons off _before_ changing the segment pins (§8c).                                                           |
| One segment never lights                          | Dead jumper, missing/wrong resistor on that segment line, or wrong GPIO→segment mapping. Test that GPIO by hard-coding it HIGH.                                |
| One digit never lights                            | That common line's wire/transistor, or wrong GPIO→common mapping / display pin (re-check §6).                                                                  |
| A digit shows a garbled numeral                   | Two segment wires swapped — compare against the segment map in §1.                                                                                             |
| Display is very **dim**                           | Expected on the simple path with big resistors; lower them toward 330 Ω, or add the common-line transistors (§4) for full brightness.                          |
| Some segments dimmer than others                  | You used per-_digit_ resistors; move to per-_segment_ resistors (§4).                                                                                          |
| Numbers are mirror-imaged / digits in wrong order | D1–D4 mapping reversed; swap the common-line assignments, or reverse how you split the number.                                                                 |
| `idf.py: command not found`                       | You didn't source the env in this terminal — run `get_idf` (`01-blink` step 5).                                                                                |
| Board won't flash / "No serial data received"     | Hold **BOOT** while flashing starts, or press **EN/RST**; check the port (`01-blink` step 8).                                                                  |
| Build fails after another project                 | Each project has its own `build/`. Run `idf.py set-target esp32` here, and `idf.py fullclean` if it's still unhappy.                                           |

---

## 11. Experiments

Once `1234` holds steady, push on it — this is where multiplexing clicks:

- **Watch the multiplexing happen:** crank the per-digit dwell way up (e.g.
  `esp_rom_delay_us(150000)` — 150 ms). The display will visibly scan one digit at
  a time. Now ramp it back down and watch the four digits "fuse" into one image.
- **Make it a clock:** count seconds and show `MM:SS`, using a decimal point as the
  separator (or the center colon if your display has one). Use
  `esp_timer_get_time()` to keep time while the refresh loop never stops.
- **Brightness control:** dim the display by leaving each digit enabled for only
  _part_ of its time slot (blank it early) — that's software PWM / duty-cycling.
- **Leading-zero blanking:** show `42` as `  42` instead of `0042` by skipping
  segments on leading-zero digits.
- **Move the refresh into its own FreeRTOS task** (or an `esp_timer` periodic
  callback) so `app_main` is free to compute the value while the display refreshes
  in the background — a clean separation worth learning.
- **Combine with `02-beep`:** display the measured distance in cm on the digits.

---

## Project layout

```
03-digits/
├── CMakeLists.txt        # Top-level project definition (project name: "Digits")
├── main/
│   ├── CMakeLists.txt    # Registers main.c as the app component
│   └── main.c            # Your multiplexing display code goes here
├── sdkconfig             # Generated build config (target = esp32)
├── build/                # Generated build output (safe to delete; rebuilt)
└── README.md             # You are here
```

---

## Appendix — segment byte table

If you'd rather store each numeral as a single byte instead of a list of segments,
here are the standard patterns with bit order `dp g f e d c b a` (bit 0 = `a`,
bit 7 = `dp`). These are for **common cathode**, where `1` = segment ON:

| Digit | Binary `dp g f e d c b a` |  Hex   |
| :---: | :-----------------------: | :----: |
|  `0`  |        `0011 1111`        | `0x3F` |
|  `1`  |        `0000 0110`        | `0x06` |
|  `2`  |        `0101 1011`        | `0x5B` |
|  `3`  |        `0100 1111`        | `0x4F` |
|  `4`  |        `0110 0110`        | `0x66` |
|  `5`  |        `0110 1101`        | `0x6D` |
|  `6`  |        `0111 1101`        | `0x7D` |
|  `7`  |        `0000 0111`        | `0x07` |
|  `8`  |        `0111 1111`        | `0x7F` |
|  `9`  |        `0110 1111`        | `0x6F` |

Add the decimal point by OR-ing in `0x80`. For **common anode**, invert each byte
(`~value & 0xFF`), since segments light on LOW.

> This packed form only works cleanly if you wire segments so bit _n_ maps to the
> GPIO you call segment _n_. If your GPIO order differs from `a..dp`, either reorder
> your pin array to match, or keep the per-segment list form from §1 — both are fine.

---

## Appendix — command reference

```bash
get_idf                                              # load ESP-IDF env
cd ~/ajisakson/embedded-learning/03-digits           # enter the project
idf.py set-target esp32                              # one-time, sets the chip
idf.py -p /dev/tty.usbserial-0001 build flash monitor  # build + upload + watch
# Ctrl-]  to exit the monitor
idf.py fullclean                                     # nuke build/ if things break
```
