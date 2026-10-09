#pragma once

// ESPHome component for the DWIN T5UIC1 "Kernel" serial display
// (Creality Ender-3 V2 LCD, 480x272 DMT48270C043_04WN).
// Protocol reference: "T5UIC1 Application Guide Ver2.3".

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#ifdef DWIN_USE_ENCODER
#include "esphome/components/rotary_encoder/rotary_encoder.h"
#endif
#ifdef DWIN_USE_ENTER
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef DWIN_USE_BUZZER
#include "esphome/components/output/binary_output.h"
#endif

namespace esphome {
namespace dwin {

// Special coordinate values understood by widgets.
static constexpr int16_t COORD_CENTER = -32768;  // centre the widget on the screen axis
static constexpr int16_t COORD_END = -32767;     // right / bottom align

static constexpr size_t MAX_FRAME_DATA = 248;  // protocol limit for cmd data bytes

struct Point {
  int16_t x;
  int16_t y;
};

struct Rect {
  int x1{0}, y1{0}, x2{-1}, y2{-1};
  bool empty() const { return x2 < x1 || y2 < y1; }
  static Rect from_size(int x, int y, int w, int h) {
    Rect r;
    r.x1 = x;
    r.y1 = y;
    r.x2 = x + w - 1;
    r.y2 = y + h - 1;
    return r;
  }
};

struct FontMetrics {
  uint8_t w;
  uint8_t h;
};

/// Font cell sizes. 0-9 are the built-in ASCII fonts (GB2312 glyphs are 2 cells wide),
/// 10 and 11 are the big numeric-only fonts used by the 0x14 "number" command.
inline FontMetrics font_metrics(uint8_t size) {
  static const uint8_t W[] = {6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 64, 44};
  static const uint8_t H[] = {12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 120, 80};
  if (size > 11)
    size = 11;
  return {W[size], H[size]};
}

/// Builds one serial frame:  AA <cmd> <data...> CC 33 C3 3C
class FrameBuilder {
 public:
  explicit FrameBuilder(uint8_t cmd) {
    buf_.reserve(32);
    buf_.push_back(0xAA);
    buf_.push_back(cmd);
  }
  FrameBuilder &u8(uint8_t v) {
    buf_.push_back(v);
    return *this;
  }
  FrameBuilder &u16(uint16_t v) {
    buf_.push_back(static_cast<uint8_t>(v >> 8));
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    return *this;
  }
  /// Coordinate / length, clamped to 0..65535
  FrameBuilder &pos(int v) {
    if (v < 0)
      v = 0;
    if (v > 0xFFFF)
      v = 0xFFFF;
    return u16(static_cast<uint16_t>(v));
  }
  FrameBuilder &bytes(const uint8_t *d, size_t n) {
    buf_.insert(buf_.end(), d, d + n);
    return *this;
  }
  size_t data_size() const { return buf_.size() - 2; }
  std::vector<uint8_t> finish() {
    buf_.push_back(0xCC);
    buf_.push_back(0x33);
    buf_.push_back(0xC3);
    buf_.push_back(0x3C);
    return std::move(buf_);
  }

 protected:
  std::vector<uint8_t> buf_;
};

enum class PageEvent : uint8_t { LOAD, UNLOAD, CLOCKWISE, ANTICLOCKWISE, CLICK, LONG_CLICK };
enum class DisplayEvent : uint8_t { READY, WAKE, SLEEP, PAGE_CHANGE, MEMORY_WRITTEN };
enum class Input : uint8_t { CLOCKWISE, ANTICLOCKWISE, CLICK, LONG_CLICK };

class DWINDisplay;
class Page;

// ---------------------------------------------------------------------------------------------
// Widget base
// ---------------------------------------------------------------------------------------------
class Widget {
 public:
  virtual ~Widget() = default;

  void set_x(int16_t v) { x_ = v; }
  void set_y(int16_t v) { y_ = v; }
  void set_width(int16_t v) { width_ = v; }
  void set_height(int16_t v) { height_ = v; }
  void set_hidden(bool v) { hidden_ = v; }
  bool hidden() const { return hidden_; }
  void set_page(Page *p) { page_ = p; }
  Page *page() const { return page_; }
  DWINDisplay *display() const;

  /// Runtime visibility control (erases the widget's last drawn area when hiding).
  void show();
  void hide();
  /// Force a redraw (if the widget is currently on screen).
  void refresh();

  /// Draw the widget from scratch. Must set bounds_.
  virtual void draw(DWINDisplay *d) = 0;
  /// Erase + draw again. Widgets may override to avoid flicker.
  virtual void redraw(DWINDisplay *d);
  /// Poll dynamic content (lambdas / sensors) and redraw if it changed.
  virtual void update(DWINDisplay *d) {}
  /// Called after the whole page has been drawn / right before it is left.
  virtual void on_show(DWINDisplay *d) {}
  virtual void on_hide(DWINDisplay *d) {}

  virtual bool focusable() const { return false; }
  virtual bool on_move(DWINDisplay *d, int delta) { return false; }
  virtual bool on_click(DWINDisplay *d) { return false; }

 protected:
  int resolve_x_(DWINDisplay *d, int w) const;
  int resolve_y_(DWINDisplay *d, int h) const;

  int16_t x_{0}, y_{0}, width_{0}, height_{0};
  bool hidden_{false};
  Rect bounds_;
  Page *page_{nullptr};
};

// ---------------------------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------------------------
class Page {
 public:
  void set_display(DWINDisplay *d) { display_ = d; }
  DWINDisplay *display() const { return display_; }
  void set_background(uint16_t c) { background_ = c; }
  uint16_t background() const { return background_; }
  void set_background_image(int8_t id) { background_image_ = id; }
  int8_t background_image() const { return background_image_; }
  void set_handles_long_click(bool v) { handles_long_click_ = v; }
  bool handles_long_click() const { return handles_long_click_; }

  void add_widget(Widget *w) {
    w->set_page(this);
    widgets_.push_back(w);
  }
  const std::vector<Widget *> &widgets() const { return widgets_; }

  void add_event_callback(PageEvent ev, std::function<void()> cb) { callbacks_.emplace_back(ev, std::move(cb)); }
  void fire(PageEvent ev);

  void render(DWINDisplay *d);
  void update(DWINDisplay *d);
  void leave(DWINDisplay *d);
  bool handle_move(DWINDisplay *d, int delta);
  bool handle_click(DWINDisplay *d);

 protected:
  Widget *focus_() const;

  DWINDisplay *display_{nullptr};
  uint16_t background_{0};
  int8_t background_image_{-1};
  bool handles_long_click_{false};
  std::vector<Widget *> widgets_;
  std::vector<std::pair<PageEvent, std::function<void()>>> callbacks_;
};

// ---------------------------------------------------------------------------------------------
// Display hub
// ---------------------------------------------------------------------------------------------
class DWINDisplay : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ---- configuration (called from generated code) ----
  void set_panel_size(uint16_t w, uint16_t h) {
    panel_w_ = w;
    panel_h_ = h;
  }
  void set_direction(uint8_t d) { direction_ = d & 3; }
  void set_home_page(Page *p) { home_ = p; }
  void set_default_brightness(float pct) { brightness_pct_ = pct; }
  void set_sleep_brightness(float pct) { sleep_pct_ = pct; }
  void set_sleep_delay(uint32_t ms) { sleep_delay_ = ms; }
  void set_sleep_on_any_page(bool v) { sleep_any_page_ = v; }
  void set_return_home_delay(uint32_t ms) { return_home_delay_ = ms; }
  void set_long_press_time(uint32_t ms) { long_press_ms_ = ms; }
  void set_long_press_back(bool v) { long_press_back_ = v; }
  void set_encoder_invert(bool v) { encoder_invert_ = v; }
  void set_click_beep(uint32_t ms) { click_beep_ms_ = ms; }
  void set_move_beep(uint32_t ms) { move_beep_ms_ = ms; }
  void set_clock_mhz(uint16_t mhz) { cost_scale_ = (mhz == 250) ? 1.76f : 1.1f; }
  void set_handshake(bool v) { handshake_ = v; }
  void set_auto_update(bool v) { auto_update_ = v; }
  void set_boot_timeout(uint32_t ms) { boot_timeout_ = ms; }
  void set_update_interval(uint32_t ms) { update_interval_ = ms; }
  void add_page(Page *p) {
    p->set_display(this);
    pages_.push_back(p);
  }
#ifdef DWIN_USE_ENCODER
  void set_encoder(rotary_encoder::RotaryEncoderSensor *s) { encoder_ = s; }
#endif
#ifdef DWIN_USE_ENTER
  void set_enter(binary_sensor::BinarySensor *s) { enter_ = s; }
#endif
#ifdef DWIN_USE_BUZZER
  void set_buzzer(output::BinaryOutput *o) { buzzer_ = o; }
#endif

  // ---- events ----
  void add_event_callback(DisplayEvent ev, std::function<void()> cb) {
    event_cbs_.emplace_back(ev, std::move(cb));
  }
  void add_memory_read_callback(std::function<void(uint8_t, uint16_t, const std::vector<uint8_t> &)> cb) {
    memory_read_cbs_.push_back(std::move(cb));
  }
  void add_extuart_callback(std::function<void(const std::vector<uint8_t> &)> cb) {
    extuart_cbs_.push_back(std::move(cb));
  }

  // ---- geometry (logical, after rotation) ----
  int width() const { return (direction_ & 1) ? panel_h_ : panel_w_; }
  int height() const { return (direction_ & 1) ? panel_w_ : panel_h_; }

  // ---- page navigation ----
  void show_page(Page *p, bool push = true);
  void back();
  void go_home();
  void redraw();
  Page *current_page() const { return current_; }
  Page *home_page() const { return home_; }
  bool is_ready() const { return state_ == State::READY; }

  // ---- power / backlight ----
  void set_brightness(float pct);  // 0..100 %
  void sleep();
  void wake();
  bool is_sleeping() const { return sleeping_; }
  void beep(uint32_t ms);

  // ---- input (called by encoder/button hooks, or manually from lambdas) ----
  void input(Input ev);
  void button_changed(bool pressed);

  // ================= raw protocol access (all commands from the guide) =================
  // 0x00 / 0x30 / 0x34
  void send_handshake();
  /// 0x3D "update LCD": commits buffered drawing to the panel (undocumented in the guide, used by Creality/Marlin)
  void update_lcd();
  void set_backlight_raw(uint8_t dim);
  void set_orientation(uint8_t dir);
  // 0x01 .. 0x09 drawing
  void clear_screen(uint16_t color);
  void draw_points(uint16_t color, uint8_t nx, uint8_t ny, const std::vector<Point> &pts);
  void draw_polyline(uint16_t color, const std::vector<Point> &pts);
  /// mode: 0 = frame, 1 = filled, 2 = XOR
  void draw_rect(uint8_t mode, uint16_t color, int x1, int y1, int x2, int y2);
  void fill_rect(uint16_t color, const Rect &r);
  void draw_bitmap(uint16_t color1, uint16_t color0, int x, int y, int width, const uint8_t *data, size_t len);
  /// mode bit7: 0 = rotate (ring) 1 = pan (fill vacated area); low bits: 0 left 1 right 2 up 3 down
  void move_area(uint8_t mode, uint16_t distance, uint16_t fill_color, int x1, int y1, int x2, int y2);
  // 0x11 / 0x14 text
  void draw_text(uint8_t size, uint16_t color, uint16_t bcolor, bool bg, bool width_adjust, int x, int y,
                 const std::string &text);
  void draw_number(uint8_t size, uint16_t color, uint16_t bcolor, bool bg, bool is_signed, bool lead_show,
                   bool lead_zero, uint8_t digits_int, uint8_t digits_frac, int x, int y, int64_t scaled);
  // 0x21 .. 0x2A pictures / icons
  void draw_qr(int x, int y, uint8_t pixel, const std::string &data);
  void show_picture(uint8_t jpeg_id);                                           // 0x22
  void show_icons(int x, int y, uint8_t mode, const std::vector<uint8_t> &ids);  // 0x23
  void show_sram_icon(int x, int y, uint8_t mode, uint16_t address);            // 0x24
  void load_vda1(uint8_t jpeg_id);                                              // 0x25 (cached)
  void copy_vda1(int sx1, int sy1, int sx2, int sy2, int x, int y);             // 0x26
  void copy_vda(uint8_t mode, int sx1, int sy1, int sx2, int sy2, int x, int y);  // 0x27
  void animation_setup(uint8_t group, uint8_t lib, uint8_t first, uint8_t last, uint8_t delay_10ms, bool restart,
                       int x, int y);                                           // 0x28
  void animation_enable(uint8_t group, bool on);                                // 0x29
  void draw_barcode(int x, int y, const uint8_t *digits12);                     // 0x2A
  /// Restore the background of an area (VDA0 copy if the page has an image, else solid fill).
  void erase_area(const Rect &r);
  // 0x31 / 0x32 / 0x33 memories
  void write_memory(uint8_t type, uint16_t address, const std::vector<uint8_t> &data);
  void read_memory(uint8_t type, uint16_t address, uint8_t length);
  void sram_to_picture(uint8_t pic_id);
  /// Convenience: upload a JPEG (<= 32 KB) via SRAM into picture/icon slot pic_id (blocks the queue ~2 s)
  void store_picture(uint8_t pic_id, const uint8_t *data, size_t len);
  // 0x38 / 0x39 extended UART
  void extuart_set_baud(uint32_t baud);
  void extuart_send(const std::vector<uint8_t> &data);
  /// Send an arbitrary command (cmd + data, no footer). cost_us = estimated processing time.
  void send_raw(uint8_t cmd, const std::vector<uint8_t> &data, uint32_t cost_us = 1000);

  size_t queue_size() const { return tx_queue_.size(); }

 protected:
  enum class State : uint8_t { BOOT, READY };
  struct TxFrame {
    std::vector<uint8_t> bytes;
    uint32_t cost_us;
  };

  void init_();
  void enqueue_(std::vector<uint8_t> &&frame, float cost_us);
  void flush_tx_();
  void read_rx_();
  void process_frame_();
  void fire_(DisplayEvent ev);
  uint8_t pct_to_dim_(float pct) const;
  void apply_brightness_();

  uint16_t panel_w_{480}, panel_h_{272};
  uint8_t direction_{0};
  Page *home_{nullptr};
  Page *current_{nullptr};
  Page *start_page_{nullptr};
  std::vector<Page *> pages_;
  std::vector<Page *> history_;

  State state_{State::BOOT};
  bool auto_update_{true};
  bool dirty_{false};
  bool handshake_{true};
  bool handshake_ok_{false};
  bool hs_sent_{false};
  uint32_t boot_start_{0}, last_hs_{0}, boot_timeout_{5000};

  // power
  float brightness_pct_{100.0f}, sleep_pct_{0.0f};
  uint32_t sleep_delay_{0}, return_home_delay_{0};
  bool sleep_any_page_{false};
  bool sleeping_{false};
  uint32_t last_activity_{0};
  uint32_t last_update_{0}, update_interval_{250};

  // input
  bool encoder_invert_{false};
  bool long_press_back_{true};
  uint32_t long_press_ms_{600};
  uint32_t click_beep_ms_{0}, move_beep_ms_{0};
  bool pressed_{false}, long_fired_{false}, swallow_release_{false};
  uint32_t press_start_{0};

  // tx pacing
  std::deque<TxFrame> tx_queue_;
  uint32_t tx_ready_us_{0};  // how long (us) to wait after tx_sent_us_ before the next frame
  uint32_t tx_sent_us_{0};
  float cost_scale_{1.1f};
  HighFrequencyLoopRequester high_freq_;

  // rx
  std::vector<uint8_t> rx_;
  uint32_t rx_last_{0};

  // state caches
  int vda1_pic_{-1};
  uint16_t cartoon_mask_{0};

  std::vector<std::pair<DisplayEvent, std::function<void()>>> event_cbs_;
  std::vector<std::function<void(uint8_t, uint16_t, const std::vector<uint8_t> &)>> memory_read_cbs_;
  std::vector<std::function<void(const std::vector<uint8_t> &)>> extuart_cbs_;

#ifdef DWIN_USE_ENCODER
  rotary_encoder::RotaryEncoderSensor *encoder_{nullptr};
#endif
#ifdef DWIN_USE_ENTER
  binary_sensor::BinarySensor *enter_{nullptr};
#endif
#ifdef DWIN_USE_BUZZER
  output::BinaryOutput *buzzer_{nullptr};
#endif
};

// ---------------------------------------------------------------------------------------------
// Triggers
// ---------------------------------------------------------------------------------------------
class PageTrigger : public Trigger<> {
 public:
  PageTrigger(Page *page, PageEvent ev) {
    page->add_event_callback(ev, [this]() { this->trigger(); });
  }
};

class DisplayTrigger : public Trigger<> {
 public:
  DisplayTrigger(DWINDisplay *d, DisplayEvent ev) {
    d->add_event_callback(ev, [this]() { this->trigger(); });
  }
};

class MemoryReadTrigger : public Trigger<uint8_t, uint16_t, std::vector<uint8_t>> {
 public:
  explicit MemoryReadTrigger(DWINDisplay *d) {
    d->add_memory_read_callback(
        [this](uint8_t type, uint16_t addr, const std::vector<uint8_t> &data) { this->trigger(type, addr, data); });
  }
};

class ExtUartTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit ExtUartTrigger(DWINDisplay *d) {
    d->add_extuart_callback([this](const std::vector<uint8_t> &data) { this->trigger(data); });
  }
};

}  // namespace dwin
}  // namespace esphome
