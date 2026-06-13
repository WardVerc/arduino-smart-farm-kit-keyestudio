# Arduino + Neovim Setup — Keyestudio ESP32 Smart Farm Kit (KS0567)

A guide to coding and uploading Arduino sketches for the **Keyestudio ESP32 IoT Control Smart Farm Starter Kit (KS0567)** from Neovim using `arduino-cli`, without ever opening the Arduino IDE.

---

## Prerequisites

- macOS (adjust paths/ports for Linux)
- [Homebrew](https://brew.sh)
- Neovim with [LazyVim](https://lazyvim.org)
- Overseer extra enabled in LazyVim (`editor.overseer`)

---

## 1. Install arduino-cli

```bash
brew install arduino-cli
arduino-cli version  # verify
```

---

## 2. Configure the ESP32 board

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
```

Install **exactly version 2.0.6** — required by the Keyestudio KS0567 tutorial:

```bash
arduino-cli core install esp32:esp32@2.0.6
arduino-cli core list  # should show esp32:esp32 2.0.6
```

---

## 3. Install the Keyestudio libraries

Enable zip installs (needed for the bundled Keyestudio libraries):

```bash
arduino-cli config set library.enable_unsafe_install true
```

Install all libraries in one loop. The library zips are included in the kit download at `https://fs.keyestudio.com/KS0567`:

```bash
LIBS_DIR="$HOME/path/to/KS0567/libraries"

for zip in "$LIBS_DIR"/*.zip; do
  echo "Installing: $zip"
  arduino-cli lib install --zip-path "$zip"
done
```

Libraries included in the kit:

| Library             | Used for                                       |
| ------------------- | ---------------------------------------------- |
| `Dht11`             | Temperature & humidity sensor (project 7.x)    |
| `ESP32_AnalogWrite` | PWM output                                     |
| `ESP32_music_lib`   | Buzzer music (project 3.4)                     |
| `ESP32Servo`        | Servo motor / feeding cabin door (project 6.x) |
| `ESP32Tone`         | Buzzer tones (project 3.3)                     |
| `ESPAsyncWebServer` | Async web server (project 11.x)                |
| `LiquidCrystal_I2C` | LCD1602 display (project 7.2+)                 |

---

## 4. Find the ESP32 port

Plug in the ESP32 via USB, then run:

```bash
arduino-cli board list
```

Look for a `Serial Port (USB)` entry — on macOS it will look like `/dev/cu.usbserial-XXXX`. That's your port.

If you're unsure which entry is the ESP32, compare before and after plugging in:

```bash
# Before plugging in:
ls /dev/cu.*

# Plug in the ESP32, then run again:
ls /dev/cu.*
# The new entry is your ESP32
```

On Linux the port is typically `/dev/ttyUSB0` or `/dev/ttyACM0`. If you get a permission error on Linux:

```bash
sudo usermod -aG dialout $USER
# Log out and back in for this to take effect
```

---

## 5. Neovim setup

### Treat `.ino` files as C++

Add to `~/.config/nvim/lua/config/options.lua`:

```lua
vim.filetype.add({ extension = { ino = "cpp" } })
```

This gives you syntax highlighting and LSP features in `.ino` files.

### Overseer task templates

Create `~/.config/nvim/lua/overseer/template/arduino.lua`:

```lua
-- PORT: found by running `arduino-cli board list` with the ESP32 plugged in.
-- Look for the Serial Port (USB) entry, e.g. /dev/cu.usbserial-XXXX on macOS.
local PORT = "/dev/cu.usbserial-2140"

-- FQBN: esp32:esp32:esp32 is the generic ESP32 Dev Module used by the KS0567.
-- UploadSpeed=115200 is required — the default 921600 baud crashes on this board's USB-serial chip.
local FQBN = "esp32:esp32:esp32:UploadSpeed=115200"

return {
  generator = function(opts)
    local cwd = opts.dir or vim.fn.getcwd()
    -- Only show these tasks if cwd is inside the farm kit project
    if not cwd:find("farm kit", 1, true) then
      return {}
    end

    return {
      {
        name = "Arduino: Compile",
        builder = function()
          return {
            cmd = { "arduino-cli" },
            args = { "compile", "--fqbn", FQBN, vim.fn.expand("%:p:h") },
            name = "arduino-compile",
          }
        end,
      },
      {
        name = "Arduino: Upload",
        builder = function()
          return {
            cmd = { "arduino-cli" },
            args = { "upload", "-p", PORT, "--fqbn", FQBN, vim.fn.expand("%:p:h") },
            name = "arduino-upload",
          }
        end,
      },
      {
        name = "Arduino: Compile & Upload",
        builder = function()
          local dir = vim.fn.expand("%:p:h")
          return {
            cmd = { "bash" },
            args = { "-c", string.format(
              -- Single-quote the path to handle spaces in directory names
              "arduino-cli compile --fqbn '%s' '%s' && arduino-cli upload -p '%s' --fqbn '%s' '%s'",
              FQBN, dir, PORT, FQBN, dir
            )},
            name = "arduino-compile-upload",
          }
        end,
      },
      {
        name = "Arduino: Serial Monitor",
        builder = function()
          return {
            cmd = { "arduino-cli" },
            args = { "monitor", "-p", PORT, "--config", "baudrate=9600" },
            name = "arduino-serial",
          }
        end,
      },
    }
  end,
}
```

Usage: open any `.ino` file, then `:OverseerRun` and pick a task. Tasks only appear when the cwd contains `farm kit` — they won't pollute other projects.

> If you plug the ESP32 into a different USB port or use a different machine, update `PORT` at the top of this file.

---

## 6. Repo structure

```
ks0567-smart-farm/
├── .gitignore
├── README.md
├── SETUP.md              ← this file
└── sketches/
    ├── 1.1Blink/
    │   └── 1.1Blink.ino
    ├── 1.2PWM/
    │   └── 1.2PWM.ino
    ├── 2.1Photocell-sensor/
    │   └── 2.1Photocell-sensor.ino
    └── ...
```

`.gitignore`:

```
# Arduino build artifacts
build/
*.bin
*.elf
*.map

# macOS
.DS_Store
```

---

## Quick reference

| Command                                                                                      | What it does                     |
| -------------------------------------------------------------------------------------------- | -------------------------------- |
| `arduino-cli board list`                                                                     | Find the ESP32 port              |
| `arduino-cli core list`                                                                      | Check installed board cores      |
| `arduino-cli lib list`                                                                       | List installed libraries         |
| `arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 .`                          | Compile current sketch           |
| `arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32:UploadSpeed=115200 .` | Upload to ESP32                  |
| `arduino-cli monitor -p /dev/cu.usbserial-XXXX --config baudrate=9600`                       | Open serial monitor at 9600 baud |
