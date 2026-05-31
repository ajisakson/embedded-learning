# 02-beep — measuring distance with an HC-SR04 ultrasonic sensor

This project teaches you how the **HC-SR04 ultrasonic distance sensor** works by
building a little **parking sensor**: the ESP32 measures how far away an object
is, prints the distance over serial, and beeps a buzzer **faster the closer the
object gets** — just like backing a car toward a wall.

The distance always prints to the serial monitor, so **the buzzer is optional** —
you can do the whole project with just the sensor and watch the numbers.

Target chip: **ESP32** (WROOM/WROVER family).
Toolchain: **ESP-IDF v6.0.1**.

> **New here?** This guide assumes your toolchain is already set up. If you
> haven't installed Homebrew, ESP-IDF, and the USB-serial driver yet, do
> [`01-blink`](../01-blink/README.md) first — steps 1–8 there get your Mac ready,
> and everything carries over to this project.

---

## 1. How an HC-SR04 actually works

The HC-SR04 measures distance with **sound**, the same way a bat or a submarine's
sonar does. It has four pins: `VCC`, `Trig`, `Echo`, `GND`.

The measurement happens in four steps:

1. **You trigger it.** You set the `Trig` pin HIGH for **10 microseconds**, then
   LOW. That's the "go" signal.
2. **It chirps.** The sensor emits **8 pulses of 40 kHz ultrasound** — too
   high-pitched for humans to hear — out of one of its two metal cans.
3. **It listens for the echo.** The sound travels out, bounces off whatever is in
   front of it, and comes back to the other can.
4. **It reports the time.** The sensor raises the `Echo` pin HIGH for **exactly as
   long as the sound was in flight**. A longer HIGH pulse = the sound took longer
   = the object is farther away.

So your job in code is just: *send a trigger, then measure how long `Echo` stays
HIGH.*

### Turning time into distance

Sound travels through air at about **343 m/s**, which works out to roughly
**29.1 microseconds per centimeter**. But the sound makes a **round trip** — out
to the object *and back* — so it covers twice the distance you care about.

That gives the magic number used in basically every HC-SR04 example:

```
distance_cm = echo_pulse_microseconds / 58
```

(58 ≈ 29.1 × 2.) The code computes exactly this. There's a fuller derivation in
the [appendix](#appendix--where-58-comes-from).

> **Range & accuracy:** the HC-SR04 reliably reads roughly **2 cm to 400 cm**,
> accurate to about ±3 mm. Closer than ~2 cm it gets unreliable; past ~4 m the
> echo is usually too weak to detect (our code reports those as "out of range").

---

## 2. ⚠️ The one thing that can fry your board — read this

**The HC-SR04 runs on 5 V, and its `Echo` pin outputs a 5 V signal. The ESP32's
GPIO pins are 3.3 V and are NOT 5-volt tolerant.** Wiring `Echo` straight to a
GPIO can damage that pin over time.

The fix is a **voltage divider**: two resistors that drop the 5 V `Echo` signal
down to a safe ~3.3 V. You only need it on `Echo` — `Trig` is an *input* on the
sensor and is happy receiving the ESP32's 3.3 V signal.

A common pair is **1 kΩ + 2 kΩ** (anything in roughly that ratio works):

```
        ECHO ──────┬────[ 1 kΩ ]────┬──── GND
   (5 V from        │                │
    the sensor)     │            [ 2 kΩ ]
                    │                │
            to ESP32 GPIO18 ─────────┘
```

Wait — let me draw that the way you'll actually build it:

```
  HC-SR04 ECHO ──[ 1 kΩ ]──┬──[ 2 kΩ ]── GND
                           │
                           └──────────────► ESP32 GPIO18
```

The voltage at the middle tap is `5 V × 2kΩ / (1kΩ + 2kΩ) = 3.3 V`. 

> **No resistors yet?** You can *briefly* test with `Echo` wired directly to
> GPIO18 to confirm everything works — many people do and the board survives —
> but **don't leave it that way.** Add the divider before any real use.

---

## 3. What you need

**Hardware**

- Your ESP32 dev board + USB **data** cable (same as `01-blink`).
- The **HC-SR04** ultrasonic sensor.
- **2 resistors** for the `Echo` voltage divider (e.g. 1 kΩ and 2 kΩ).
- A **breadboard** and some jumper wires.
- _(Optional — the "beep")_ a small **active buzzer**. See the note below.
- _(Optional)_ a transistor if your buzzer is loud/large (see step 4).

**Software** — same as `01-blink`: ESP-IDF v6.0.1, sourced into your shell.

> **Active vs. passive buzzer:** this project uses an **active** buzzer — one that
> makes a tone on its own as soon as you apply voltage, so the code just switches
> a GPIO on and off. A **passive** buzzer needs you to generate the tone yourself
> (a square wave via the LEDC/PWM peripheral) and will only click with this code.
> If you're buying one, get an *active* buzzer for this project. (Driving a
> passive buzzer with PWM is a great stretch goal — see [step 8](#8-experiments).)

---

## 4. Wire it up

Pins used by the code (change them at the top of [main/main.c](main/main.c) if you
like):

| Sensor / part | ESP32 pin | Notes                                              |
| ------------- | --------- | -------------------------------------------------- |
| `VCC`         | **5V / VIN** | The sensor needs 5 V — **not** the 3V3 pin.     |
| `GND`         | **GND**   | Common ground with the board.                      |
| `Trig`        | **GPIO5** | Driven directly by the ESP32 (3.3 V is fine here). |
| `Echo`        | **GPIO18** | **Through the voltage divider** from step 2.      |
| Buzzer `+`    | **GPIO4** | Optional. Buzzer `−` goes to `GND`.                |

Full picture:

```
   ESP32                         HC-SR04
   ┌─────────┐                  ┌──────────┐
   │   5V/VIN ├──────────────────┤ VCC      │
   │     GND  ├──────────────────┤ GND      │
   │   GPIO5  ├──────────────────┤ Trig     │
   │  GPIO18  ├───[divider]───────┤ Echo     │   (1kΩ + 2kΩ, see step 2)
   └─────────┘                  └──────────┘

   ESP32 GPIO4 ──┤>├── buzzer ── GND          (optional active buzzer)
```

> **Buzzer current:** a tiny active buzzer (a few mA) can be driven straight from
> a GPIO. If yours is loud/big or the datasheet lists tens of mA, drive it through
> an NPN transistor (e.g. 2N2222: GPIO → 1 kΩ → base, buzzer between 5 V and
> collector, emitter to GND) so you don't overload the pin.

Power the board off **USB only** while testing — don't also feed it from another
supply.

---

## 5. The code, explained

The whole program is in [main/main.c](main/main.c). Two parts:

**`measure_distance_cm()`** does one measurement and returns centimeters:

```c
// 1. 10 µs trigger pulse
gpio_set_level(TRIG_PIN, 1);
esp_rom_delay_us(10);
gpio_set_level(TRIG_PIN, 0);

// 2. wait for ECHO to go HIGH, then 3. time how long it stays HIGH
//    using esp_timer_get_time() (a microsecond clock)
// 4. distance_cm = pulse_us / 58.0
```

It uses `esp_rom_delay_us()` for the precise 10 µs pulse and
`esp_timer_get_time()` (microseconds since boot) to time the echo. If no echo
arrives within `ECHO_TIMEOUT_US` (30 ms), it returns `-1.0` so the caller can show
"out of range" instead of hanging forever.

**`app_main()`** loops: measure → print → decide how to beep.

```c
if (cm > BEEP_RANGE_CM) {        // farther than 50 cm: stay quiet
    ...
} else {                          // closer = shorter gap between beeps
    int gap_ms = (int)((cm / BEEP_RANGE_CM) * 560.0f) + 40;
    // beep 40 ms ON, then gap_ms of silence
}
```

Up close the gap shrinks toward 40 ms (rapid beeping); near 50 cm it stretches to
~600 ms (lazy beeping). Beyond 50 cm it goes silent. That mapping is the entire
"parking sensor" feel — and it's just arithmetic you can tweak.

---

## 6. Build, flash, and watch

Source the environment (`get_idf`, from `01-blink` step 5) and make sure you're in
this `02-beep` directory. The first build also needs the target set once:

```bash
idf.py set-target esp32
idf.py -p /dev/tty.usbserial-0001 build flash monitor
```

(Use your own port from `01-blink` step 8 if it differs.) The monitor will stream:

```
distance: 132.4 cm
distance: 47.8 cm
distance: 12.1 cm
distance: -- (out of range / no echo)
```

Wave your hand in front of the sensor and watch the numbers track it. If you wired
the buzzer, it should beep faster as your hand gets closer. Press **`Ctrl-]`** to
exit the monitor.

---

## 7. Troubleshooting

| Symptom                                       | Likely cause / fix                                                                                                                                  |
| --------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| Always reads `-- (out of range)`              | `Echo` not reaching GPIO18 (check the divider's middle tap), `Trig`/`Echo` swapped, or sensor not getting **5 V** (you used 3V3).                    |
| Always reads a tiny constant like `0.x cm`    | Usually `Trig` and `Echo` swapped, or `Echo` shorted to ground through the divider wired backwards.                                                  |
| Readings jump around wildly                   | Normal for soft/angled/cloth targets (poor reflectors). Aim at a flat hard surface. Loose breadboard wires also cause this.                          |
| Numbers seem ~2× too big or too small         | Wrong divisor — it must be `/ 58` for **cm** (round trip). Don't divide by 29.                                                                       |
| Buzzer just clicks, never tones               | It's a **passive** buzzer; this code only does on/off. Use an active buzzer, or drive a passive one with PWM (see step 8).                           |
| Buzzer silent but distance prints fine        | Check `+`/`−` orientation, that `+` is on GPIO4, and that the object is within 50 cm (`BEEP_RANGE_CM`).                                              |
| `idf.py: command not found`                   | You didn't source the env in this terminal — run `get_idf` (`01-blink` step 5).                                                                     |
| Board won't flash / "No serial data received" | Hold **BOOT** while flashing starts, or press **EN/RST**; check the port (`01-blink` step 8).                                                        |
| Build fails after coming from `01-blink`      | Each project has its own `build/`. Run `idf.py set-target esp32` here, and `idf.py fullclean` if it's still unhappy.                                  |

---

## 8. Experiments

Once it works, try changing things — this is how it sticks:

- **Change the trip distance:** edit `BEEP_RANGE_CM` (e.g. 30 cm or 100 cm).
- **Change the feel:** tweak the `40` and `560` in the `gap_ms` formula for
  faster/slower beeping, or the `40` ms beep on-time.
- **Add an LED bar:** light more LEDs (or change a color) as the object gets
  closer — combine this with what you did in `01-blink`.
- **Smooth the readings:** average the last few measurements to steady the number.
- **Stretch — drive a passive buzzer:** use the **LEDC** peripheral to output a
  ~2 kHz square wave so a passive buzzer actually tones, and vary the *frequency*
  with distance for a true rising/falling pitch.

---

## Project layout

```
02-beep/
├── CMakeLists.txt        # Top-level project definition (project name: "Beep")
├── main/
│   ├── CMakeLists.txt    # Registers main.c as the app component
│   └── main.c            # Measure distance + drive the buzzer
├── sdkconfig             # Generated build config (target = esp32)
├── build/                # Generated build output (safe to delete; rebuilt)
└── README.md             # You are here
```

---

## Appendix — where `/ 58` comes from

Speed of sound in air at ~20 °C:

```
343 m/s = 34300 cm/s = 0.0343 cm/µs
```

Time for sound to travel **one** cm:

```
1 / 0.0343 ≈ 29.15 µs per cm   (one way)
```

But `Echo` is HIGH for the **round trip** (to the object and back), so the pulse
covers `2 ×` the distance:

```
29.15 × 2 ≈ 58.3 µs per cm   (round trip)
```

So, given the measured `Echo` pulse width in microseconds:

```
distance_cm = pulse_us / 58
```

That's the single line of physics at the heart of this whole project.

---

## Appendix — command reference

```bash
get_idf                                              # load ESP-IDF env
cd ~/ajisakson/embedded-learning/02-beep             # enter the project
idf.py set-target esp32                              # one-time, sets the chip
idf.py -p /dev/tty.usbserial-0001 build flash monitor  # build + upload + watch
# Ctrl-]  to exit the monitor
idf.py fullclean                                     # nuke build/ if things break
```
