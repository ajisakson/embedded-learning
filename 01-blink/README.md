# 01-blink — ESP32 "Hello, World" from absolute scratch

This guide takes you from a clean macOS machine to a **blinking LED on a physical
ESP32 board**. It assumes you have _nothing_ installed yet — no toolchain, no
ESP-IDF, no repo. Follow it top to bottom and you'll end with two LEDs blinking
back and forth.

The project itself blinks two LEDs in alternation:

- the **onboard LED** wired to `GPIO2`, and
- an **external LED** wired to `GPIO5`.

Target chip: **ESP32** (the original WROOM/WROVER family, not a C3/S3/etc.).
Toolchain: **ESP-IDF v6.0.1**.

---

## 0. What you need

**Hardware**

- An ESP32 dev board (e.g. ESP32-DevKitC, WROVER-Kit, NodeMCU-32S — anything with
  an ESP32 WROOM/WROVER module).
- A **USB data cable** (not a charge-only cable — this trips up a lot of people).
- _(Optional, for the external LED)_ one LED + one ~330Ω resistor + a couple of
  jumper wires + a breadboard.

**Software** — installed below:

- Homebrew
- A few command-line tools (`cmake`, `ninja`, `dfu-util`, `python`)
- ESP-IDF v6.0.1 and its bundled toolchain

Everything in this guide is for **macOS** (Apple Silicon or Intel). Commands are
run in **Terminal** (or the VS Code integrated terminal).

---

## 1. Install Homebrew

Homebrew is the macOS package manager. If you already have it, skip to step 2.
Check with:

```bash
brew --version
```

If that prints a version, you're done. Otherwise install it:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

> **Apple Silicon note:** Homebrew installs to `/opt/homebrew`. The installer
> prints two `echo ... >> ~/.zprofile` lines at the end — **run them**, then
> restart your terminal, so the `brew` command is on your `PATH`. Verify again
> with `brew --version`.

---

## 2. Install the build prerequisites with Homebrew

ESP-IDF needs a handful of host tools. Install them all in one go:

```bash
brew install cmake ninja dfu-util python git
```

What each one is for:

| Package    | Why it's needed                                             |
| ---------- | ----------------------------------------------------------- |
| `cmake`    | Generates the build system ESP-IDF uses.                    |
| `ninja`    | The fast build backend that actually compiles the firmware. |
| `dfu-util` | Lets some boards be flashed over USB DFU.                   |
| `python`   | ESP-IDF's tooling (`idf.py`, flashing, etc.) is Python.     |
| `git`      | Used to clone both ESP-IDF and this repo.                   |

Verify they installed:

```bash
cmake --version && ninja --version && python3 --version && git --version
```

---

## 3. Install the USB-to-serial driver (only if your board isn't detected)

Your Mac talks to the ESP32 over a USB-to-UART bridge chip. Most ESP32 boards use
one of two chips, and macOS may or may not already have the driver:

- **Silicon Labs CP210x** (very common — this is what `/dev/tty.usbserial-0001`
  usually is). Driver:
  <https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers>
- **WCH CH340/CH9102** (common on cheaper boards). Driver:
  <https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html>

You can install the CP210x driver via Homebrew too:

```bash
brew install --cask silicon-labs-vcp-driver
```

You don't have to do this _yet_ — come back here only if step 8 ("Find the
serial port") shows nothing. After installing a driver you may need to approve it
in **System Settings → Privacy & Security** and reboot.

---

## 4. Install ESP-IDF v6.0.1

ESP-IDF is Espressif's official SDK. We install the exact version this project was
built against (**v6.0.1**) into `~/esp` and let it download its own toolchain.

```bash
# 1. Make a home for it
mkdir -p ~/esp
cd ~/esp

# 2. Clone v6.0.1 with all submodules (this is a big download — be patient)
git clone -b v6.0.1 --recursive https://github.com/espressif/esp-idf.git

# 3. Install the toolchain for the ESP32 target
cd ~/esp/esp-idf
./install.sh esp32
```

`install.sh` downloads the compiler, debugger, and Python environment into
`~/.espressif`. This takes a few minutes and a couple GB of disk.

---

## 5. Load the ESP-IDF environment ("export")

ESP-IDF doesn't put `idf.py` permanently on your `PATH`. Instead you **source its
export script** in each new terminal where you want to build:

```bash
. ~/esp/esp-idf/export.sh
```

(That's a dot, a space, then the path — it runs the script in your _current_
shell.) You should see output ending in something like _"Done! You can now compile
ESP-IDF projects."_

> **Make this easier:** add a shortcut to your shell profile so you can just type
> `get_idf`:
>
> ```bash
> echo "alias get_idf='. ~/esp/esp-idf/export.sh'" >> ~/.zshrc
> ```
>
> Restart your terminal, then run `get_idf` whenever you start working.

**You must run this in every new terminal session before building.** If `idf.py`
ever reports "command not found", you forgot this step.

---

## 6. Clone this repo

Pick a directory for your code (this guide uses `~/ajisakson`, matching the
author's setup — anywhere is fine):

```bash
mkdir -p ~/ajisakson
cd ~/ajisakson
git clone https://github.com/ajisakson/embedded-learning.git
cd embedded-learning/01-blink
```

You're now inside the blink project. You should see `main/`, `CMakeLists.txt`, and
this `README.md`.

---

## 7. Wire up the hardware

**Onboard LED (no wiring needed):** Most ESP32 dev boards have a built-in LED on
`GPIO2`. If yours does, it'll blink with zero extra hardware.

**External LED (optional but fun):** Wire an LED to `GPIO5`:

```
GPIO5 ──[ 330Ω resistor ]──►|── GND
                            LED
                       (long leg = +, to the resistor side)
```

- LED **long leg (anode, +)** → through the **330Ω resistor** → **GPIO5**
- LED **short leg (cathode, −)** → **GND**

The resistor protects the LED and the GPIO pin — don't skip it.

> Because the code drives the two pins in opposite states, the onboard LED and the
> external LED blink **alternately** (one on while the other is off).

Now plug the board into your Mac with the USB data cable.

---

## 8. Find the serial port

With the board plugged in, list the serial devices:

```bash
ls /dev/tty.usb* /dev/tty.SLAB* /dev/tty.wchusb* 2>/dev/null
```

You're looking for something like:

- `/dev/tty.usbserial-0001` ← what this project's config expects (CP210x)
- `/dev/tty.SLAB_USBtoUART` (older CP210x driver)
- `/dev/tty.wchusbserial1420` (CH340)

**Nothing listed?** Then either the cable is charge-only, the board isn't getting
power, or the USB-serial driver is missing — go back to [step 3](#3-install-the-usb-to-serial-driver-only-if-your-board-isnt-detected).

Note your port name — you'll use it as `-p <PORT>` below. The examples assume
`/dev/tty.usbserial-0001`.

---

## 9. Set the target, build, flash, and watch it blink

Make sure you've sourced the environment in this terminal (`get_idf`, from step 5)
and that you're in the `01-blink` directory.

**Set the chip target** (only needed once per project; this repo is already set to
`esp32`, but it's harmless to run):

```bash
idf.py set-target esp32
```

**Build the firmware:**

```bash
idf.py build
```

The first build is slow (it compiles the whole SDK). Subsequent builds are fast.
A successful build ends with a _"Project build complete"_ message and instructions
showing the flash command.

**Flash it to the board and open the serial monitor in one step:**

```bash
idf.py -p /dev/tty.usbserial-0001 flash monitor
```

This uploads the firmware and then streams the board's output. **Your LED(s)
should now be blinking** (~600 ms on/off).

To exit the serial monitor, press **`Ctrl-]`** (control + right square bracket).

> **One command for everything:** `idf.py -p /dev/tty.usbserial-0001 build flash monitor`
> builds, flashes, and monitors in a single line. Use this as your everyday loop.

---

## 10. Make a change and re-flash (the dev loop)

To prove the loop works end to end, change the blink speed. Open
[main/main.c](main/main.c) and edit:

```c
#define BLINK_DELAY_MS 600
```

Set it to e.g. `150` for a faster blink, save, then re-run:

```bash
idf.py -p /dev/tty.usbserial-0001 flash monitor
```

The LEDs should now blink noticeably faster. That's the whole edit → build →
flash → observe cycle.

---

## Troubleshooting

| Symptom                                                   | Fix                                                                                                                                                                                            |
| --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `idf.py: command not found`                               | You didn't source the environment. Run `get_idf` (or `. ~/esp/esp-idf/export.sh`) in this terminal.                                                                                            |
| No `/dev/tty.usb*` device appears                         | Bad/charge-only cable, or missing USB-serial driver. Try a different cable first, then install the driver from [step 3](#3-install-the-usb-to-serial-driver-only-if-your-board-isnt-detected). |
| `Failed to connect to ESP32: ... No serial data received` | Hold the board's **BOOT** button while flashing starts, release after it begins; or press **EN/RST**. Some boards need this to enter download mode.                                            |
| `Permission denied` opening the port                      | Make sure nothing else (another monitor, Arduino IDE) has the port open. Unplug/replug the board.                                                                                              |
| Wrong port flashed                                        | Double-check the exact name from [step 8](#8-find-the-serial-port). It can change between USB ports/reboots.                                                                                   |
| Onboard LED doesn't blink                                 | Your board may not have an LED on `GPIO2`. Wire the external LED on `GPIO5` (step 7) to confirm the firmware is running.                                                                       |
| Build fails after switching chips/versions                | Run `idf.py fullclean` and build again.                                                                                                                                                        |
| Garbage characters in monitor                             | The monitor baud is 115200 by default; if you changed it, match it with `-b <baud>`. Exit with `Ctrl-]`.                                                                                       |

---

## Project layout

```
01-blink/
├── CMakeLists.txt        # Top-level project definition (project name: "Blink")
├── main/
│   ├── CMakeLists.txt    # Registers main.c as the app component
│   └── main.c            # The blink program (GPIO2 + GPIO5)
├── sdkconfig             # Generated build config (target = esp32)
├── build/                # Generated build output (safe to delete; rebuilt)
└── README.md             # You are here
```

---

## Appendix — Quick command reference

```bash
get_idf                                              # load ESP-IDF env (step 5)
cd ~/ajisakson/embedded-learning/01-blink            # enter the project
idf.py set-target esp32                              # one-time, sets the chip
idf.py build                                         # compile
idf.py -p /dev/tty.usbserial-0001 flash monitor      # upload + watch output
# Ctrl-]  to exit the monitor
idf.py fullclean                                     # nuke build/ if things break
```
