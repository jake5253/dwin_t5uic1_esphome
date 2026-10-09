# DWIN T5UIC1 (Ender-3 V2 LCD) – ESPHome external component

A retained-mode UI component for the **Ender-3 V2 display** — the DWIN T5UIC1 serial
controller with a rotary knob and **no touch screen**. This is a full-featured
re-implementation of the entire T5UIC1 serial protocol: every documented feature is covered.

The configuration is modelled to be *similar* to LVGL (though not identical) — if you have
designed LVGL UIs before, the structure of pages, widgets and their properties should feel
familiar.

## Install

Add the following to your YAML:

```YAML
external_components:
  source:
      type: git
      url: https://github.com/jake5253/dwin_t5uic1_esphome
      ref: main
```

You need a `uart:` with `tx`/`rx` wired to the display. Optionally:

* `encoder:` — a `rotary_encoder` sensor (the knob)
* `enter:` — a `binary_sensor` for the knob press
* `buzzer:` — any `output` (a `gpio` output for a simple buzzer, or `ledc` for a passive
  one that needs a tone)

### Wiring: the 10-pin header

The back of the screen has a 2x5 IDC header (10 pins) with a notch/key. **On the screen the
connector is numbered right-to-left**: with the **notch UP**, the top-right-most pin is pin
`1` and the bottom-right-most pin is pin `2`:

```
                     ▲ notch (key up)
     ┌───────────────┴───────────┐
     │   9    7    5    3    1   │  top row (odd pins)
     │  10    8    6    4    2   │  bottom row (even pins)
     └───────────────────────────┘
        viewed from the BACK of the screen
```

| Pin | Signal | Ribbon colour | Connects to |
|---|---|---|---|
| 1 | NC | brown | — |
| 2 | NC | red | — |
| 3 | RX | orange | your UART's **TX** |
| 4 | TX | yellow | your UART's **RX** |
| 5 | Encoder Button | green | `enter:` binary_sensor |
| 6 | Buzzer | blue | `buzzer:` output |
| 7 | Encoder B | violet | rotary encoder pin B |
| 8 | Encoder A | grey | rotary encoder pin A |
| 9 | GND | white | ground |
| 10 | 5V | black | 5V supply |

The colours are for the rainbow ribbon cable that comes with the screen. `RX`/`TX` are named
from the **screen's** point of view, so the screen's `RX` (pin 3) goes to your ESP's TX and
the screen's `TX` (pin 4) goes to your ESP's RX.

### Firmware / baud rate

You almost certainly don't need to worry about any of this: the display is **almost always
set to 115200 baud**, which is what this component expects. To be sure — and to make sure
you're running the latest T5 firmware (`T5UIC1_V23_20210303`) — copy the whole `DWIN_SET`
folder from this repository to the **root of a FAT32-formatted SD card** and insert the card
into the screen. It configures:

```
CPU Frequency: 400Mhz
Power On: Black Screen
CRC: Disabled (leave disabled -- not implemented in this project)
Direction: 0 (landscape)
Screen: DMT48270C043_04WN
Baud: 115200
```

See `example.yaml` for a tour of features. The sections below document every available
option.

# Features

## Display options

| Option | Req. | Default | Description |
|---|---|---|---|
| `id` | no | — | Only needed if you have more than one display, or want to reference it from actions. |
| `encoder` | no | — | `rotary_encoder` sensor used as the knob. |
| `enter` | no | — | `binary_sensor` for the knob button. |
| `buzzer` | no | — | `output` (binary or float, e.g. `gpio` or `ledc`) for beeps; used with `click_beep`/`move_beep` or `dwin.beep`. |
| `click_beep` | no | *none (off)* | Beep length on a knob click. |
| `move_beep` | no | *none (off)* | Beep length on each knob detent. |
| `encoder_invert` | no | `false` | Swap clockwise/anticlockwise if the knob turns the wrong way. |
| `long_press_time` | no | `600ms` | How long the button must be held for a long click. |
| `long_press_back` | no | `true` | Long press navigates back (when there's page history). Suppressed on any page that defines `on_long_click`. The first input while the screen is asleep only wakes it. |
| `width` | no | `480` | Physical panel width in pixels. |
| `height` | no | `272` | Physical panel height in pixels. |
| `direction` | no | `0` | Rotation: `0`/`90`/`180`/`270` (or `0`-`3`). `90`/`270` swap the logical width/height (480x272 → 272x480); all widget coordinates are logical. |
| `home_page` | no | first page | The page `dwin.page.home` returns to (and the page shown at boot). |
| `default_brightness` | no | `100%` | Backlight level while awake. |
| `sleep_delay` | no | *never* | Idle time before the display dims. |
| `sleep_brightness` | no | `0%` | Backlight level while asleep (`0%` = off). |
| `sleep_on_any_page` | no | `false` | Dim after idle on *any* page, not just the home page. |
| `return_home_delay` | no | *never* | Idle time before returning to the home page from a sub-page. |
| `update_interval` | no | `250ms` | How often dynamic widget content (lambdas, sensors) is polled and redrawn on change. |
| `auto_update` | no | `true` | The screen should automatically update whenever a value changes; the component commits with `0x3D` once drawing finishes. Force it *now* with `- dwin.update:` or `- dwin.raw_send: {command: 0x3D}`. |
| `handshake` | no | `true` | Wait for the display's boot handshake reply before initialising (warns and continues after `boot_timeout`). |
| `boot_timeout` | no | `5s` | How long to wait for the handshake reply. |
| `cpu_frequency` | no | `400` | `250` or `400` MHz — must match the display config; scales how long commands are paced for. |
| `pages` | **yes** | — | The list of pages (at least one). |

## Display events

All optional automations on the display itself:

| Event | Arguments | Description |
|---|---|---|
| `on_ready` | — | Display initialised and the first page shown. |
| `on_wake` / `on_sleep` | — | Screen backlight restored / dimmed. |
| `on_page_change` | — | Any page navigation. |
| `on_memory_read` | `type`, `address`, `data` | Reply to `dwin.memory.read`. |
| `on_memory_written` | — | Flash write or SRAM→picture finished. |
| `on_extuart_data` | `x` (byte vector) | Data received on the display's extended UART. |

## Pages

| Option | Req. | Default | Description |
|---|---|---|---|
| `id` | **yes** | — | Unique name. `main`, `setup`, `loop` and `app` are rejected (C++ symbol clash) — use e.g. `main_page`. |
| `background` | no | `black` | Solid colour the page is cleared to (and the colour used to erase widgets on it). |
| `background_image` | no | *none* | Full-screen JPEG slot `0`-`15` from the SD card as the page background (widgets erase by restoring from it). |
| `widgets` | no | `[]` | The widget list (see below). |

Page events (all optional):

| Event | Description |
|---|---|
| `on_load` / `on_unload` | Page shown / left. |
| `on_clockwise` / `on_anticlockwise` | Knob turned — only fired when no focusable widget (e.g. a `menu`) consumed the turn. |
| `on_click` | Knob pressed — likewise only when no focusable widget consumed it. |
| `on_long_click` | Knob held; **defining it disables the default "long press = back" for this page**. |

## Widgets

Every widget entry is `{<type>: {config}}` (or a list of configs). Common options:

| Option | Req. | Default | Description |
|---|---|---|---|
| `x`, `y` | no | `0` | Position; accepts an integer or `CENTER`/`MIDDLE` (centre on that axis), `RIGHT`/`BOTTOM`/`END` (align to the far edge), `LEFT`/`TOP`. |
| `hidden` | no | `false` | Start hidden (toggle at runtime with `dwin.widget.show`/`dwin.widget.hide`). |

Colours on any option take a name (`white`, `red`, `orange`, `grey`, …), an `0xRRGGBB`
integer or a `"#RRGGBB"` string.

### Shapes

* **`rectangle`** — *optional: `width`/`height` (no size → extends to the screen edge)*.
  Options: `color` *(optional, default `white`)*, `fill` *(optional, default `false`)*,
  `xor` *(optional, default `false`)*, `border_color` *(optional)*.
* **`line`** — *required: `points` (2-61, `[x, y]` pairs or `{x, y}` dicts)*.
  Options: `color` *(optional, default `white`)*.
* **`points`** — *required: `points`*. Options: `size` *(optional, default `1` — 1-15 px
  dot)*, `color` *(optional, default `white`)*.

### `bitmap`

*Required: `width` (1-480) and `data` (one row per line, 8 pixels per byte, MSB = leftmost).
Optional: `color` *(default `white` — the "on" pixels)*, `bg_color` *(default `black`)*.*

> **Note:** the bitmap is always drawn in software (background fill + one rectangle per run
> of set bits), because the native bitmap command (0x08) does not render on the display
> firmware. Use `bitmap_converter.html` (see below) to produce the `data` from an image.

### `text`

*Required: exactly one of `text` or `text_sensor`. Optional: `size` *(default `3`, fonts
0-9)*, `color` *(default `white`)*, `bg_color` *(optional)*, `width_adjust` *(default
`false`)*.*

* Static `text` is converted to GB2312 at compile time, so Chinese works out of the box;
  lambda text must be ASCII or already-GB2312 bytes.
* Lambdas and `text_sensor` bindings are polled every `update_interval` and redrawn on
  change.
* Text/number drawn over a filled shape: give it a `bg_color` equal to the shape's colour,
  otherwise the page background is used to erase old content.

### `number`

*Required: exactly one of `value` or `sensor`. Optional: `size` *(default `3`)*, `color`
*(default `white`)*, `bg_color`, `digits` *(default `4`)*, `decimals` *(default `0`)*,
`signed` *(default `true`)*, `leading_zeros` *(default `none`; `none` | `space` | `zero`)*.*

### `progress_bar`

*Required: exactly one of `value` or `sensor`. Optional: `width`/`height`, `color` *(default
`green`)*, `bg_color` *(default `dark_grey`)*, `border_color`, `vertical` *(default
`false`)*, `min_value` *(default `0`)*, `max_value` *(default `100`)*.*

### `qr_code` and `barcode` — NOT WORKING

> **These two widgets do not work and will not be fixed.** The problem is in the T5
> firmware/kernel of the display itself, not in this component — the QR code renders
> distorted/unreadable and the EAN-13 barcode doesn't render at all. Treat both as
> unsupported; don't bother using them.

(Schemas, for reference: `qr_code` takes `data` *(required)* and `pixel_size` *(default
`4`)*; `barcode` takes 12 digits as `data` *(required)* — the display computes the 13th.)

### SD-card media (`image`, `icon`, `sram_icon`, `animation`)

These need JPEG/`.ICO` files in the `DWIN_SET` folder on the display's SD card — that's why
they aren't demonstrated in `example.yaml`.

* **`image`** — copy a region of a stored JPEG. *Required: `width`, `height`.* Optional:
  `picture` *(slot 0-15; omit to use the current page's `background_image`)*, `source_x` /
  `source_y` *(default `0`)*, `transparent` *(default `false`)*, `restore_background`
  *(default `false`)*, `strong_filter` *(default `false`)*.
* **`icon`** — icons from an `.ICO` library. *Required: `library` (0-15) and exactly one of
  `icon` / `icons`.* Optional: `icon_width` *(default `28`)*, `icon_height` *(default
  `45`)*, `transparent` / `restore_background` / `strong_filter` *(default `false`)*.
* **`sram_icon`** — a JPEG icon whose bytes you wrote into SRAM with `dwin.memory.write`.
  *Required: `address`.* Optional: `icon_width` *(28)*, `icon_height` *(45)*,
  `transparent`, `strong_filter`.
* **`animation`** — plays autonomously while its page is shown. *Required: `library`
  (0-15), `start_icon`, `end_icon`.* Optional: `group` *(default `0`)*, `delay` *(default
  `100ms` — multiple of 10 ms, max 2550 ms)*, `restart` *(default `true`)*.

### `menu`

A scrollable, encoder-driven list. *Required: `items` (at least one).* Optional:
`size` *(default `3`)*, `padding` *(default `4`)*, `item_height` *(optional)*, `title`,
`title_color`, `title_bg_color`, `bg_color` *(default `black`)*, `item_color` *(default
`white`)*, `selector_type` *(default `box`; `box` | `arrow` | `highlight`)*,
`selector_color` *(default `red`)*, `selected_text_color` *(optional)*, `wrap` *(default
`false`)*, `scrollbar` *(default `true`)*, `scrollbar_color` *(default `grey`)*, plus
`x`/`y`/`width`/`height`.

Each item takes: `text` *(required — static or lambda, re-polled while visible)*, `page`
*(optional — clicking navigates there and pushes history)*, `back` *(optional, default
`false` — clicking goes back instead)*, `on_enter` / `on_select` *(optional automations;
`on_select` fires while turning past the item, `on_enter` when it's clicked)*.

The menu is focusable: turning the knob moves the selection, clicking activates the item.
You can also drive it with `dwin.menu.next`, `dwin.menu.previous`, `dwin.menu.enter` and
`dwin.menu.select`.

## Actions

| Action | Options | Description |
|---|---|---|
| `dwin.sleep` / `dwin.wake` | — | Dim / restore the backlight. |
| `dwin.page.show` | `id` *(required)*, `push` *(optional, default `true` — set `false` to replace without adding to history)* | Show a page. |
| `dwin.page.back` / `dwin.page.home` / `dwin.page.redraw` | — | Navigate back / to the home page / re-render the current page. |
| `dwin.update` | — | Commit drawing to the screen now (`0x3D`). |
| `dwin.brightness` | `brightness` *(required, 0-100)* | Set the backlight level. |
| `dwin.beep` | `duration` *(optional, default `100ms`)* | Buzz the buzzer output. |
| `dwin.orientation` | `direction` *(required)* | Change the rotation at runtime. |
| `dwin.raw_send` | `command` *(required, hex)*, `data` *(optional, default `[]`)*, `processing_time` *(optional, default `1ms`)* | Send any raw protocol command. |
| `dwin.memory.write` | `data` *(required)*, `type` *(optional, default `sram`)*, `address` *(optional, default `0`)* | Write display SRAM (0x0000-0x7FFF) or flash (100k write cycles — sparingly). |
| `dwin.memory.read` | `length` *(required, 1-240)*, `type` *(optional, default `sram`)*, `address` *(optional, default `0`)* | Read display memory; result arrives in `on_memory_read`. |
| `dwin.sram_to_picture` | `picture` *(required, 0-15)* | Store SRAM contents as a picture slot (~2 s). |
| `dwin.extuart.send` | `data` *(required)* | Send bytes on the display's extended UART. |
| `dwin.extuart.baud_rate` | `baud_rate` *(required)* | Set the extended UART baud rate. |
| `dwin.animation.start` / `dwin.animation.stop` | `group` *(required, 0-15)* | Start/stop an `animation` widget's group. |
| `dwin.move_area` | `direction` *(required: `left`/`right`/`up`/`down`)*, `distance` *(required, 0-240)*, `x1`/`y1`/`x2`/`y2` *(required)*, `ring` *(optional, default `false` — rotate instead of pan)*, `fill_color` *(optional, default `black`)* | Scroll a screen region. |
| `dwin.widget.show` / `dwin.widget.hide` / `dwin.widget.refresh` | `id` *(required — any widget)* | Show, hide (erases its area), or force-redraw a widget. |
| `dwin.text.set` | `id` *(required — any string widget incl. `qr_code`/`barcode`)*, `text` *(required)* | Set the widget's text immediately. |
| `dwin.value.set` | `id` *(required — `number`/`progress_bar`)*, `value` *(required)* | Set the widget's value immediately. |
| `dwin.menu.next` / `dwin.menu.previous` / `dwin.menu.enter` | `id` *(required)* | Move the selection / activate the item. |
| `dwin.menu.select` | `id` *(required)*, `index` *(required)* | Jump to a specific menu item. |

# Bitmap converter

`bitmap_converter.html` (in this repo) turns a JPG/PNG/GIF/WebP image into the `data` for a
`bitmap` widget. It's a single HTML file — just open it in your browser (everything runs
locally, nothing is uploaded). It gives you a preview, threshold / dithering (Floyd-Steinberg
or Bayer) / invert controls, the two widget colours, and a copy button for the YAML (full
widget or bare `data:` list).

The size limit is the logical screen size: **480x272**, or **272x480** when `direction` is
`90`/`270`.
