#pragma once

#include "dwin.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace dwin {

// ---------------------------------------------------------------------------------------------
// Primitive shapes
// ---------------------------------------------------------------------------------------------
class RectangleWidget : public Widget {
 public:
  void set_color(uint16_t c) { color_ = c; }
  void set_fill(bool v) { fill_ = v; }
  void set_xor(bool v) { xor_ = v; }
  void set_border_color(uint16_t c) {
    border_color_ = c;
    has_border_ = true;
  }
  void draw(DWINDisplay *d) override;

 protected:
  uint16_t color_{0xFFFF}, border_color_{0};
  bool fill_{false}, xor_{false}, has_border_{false};
};

class LineWidget : public Widget {
 public:
  void set_color(uint16_t c) { color_ = c; }
  void add_point(int16_t x, int16_t y) { pts_.push_back({x, y}); }
  void draw(DWINDisplay *d) override;

 protected:
  uint16_t color_{0xFFFF};
  std::vector<Point> pts_;
};

class PointsWidget : public Widget {
 public:
  void set_color(uint16_t c) { color_ = c; }
  void set_point_size(uint8_t s) { size_ = s; }
  void add_point(int16_t x, int16_t y) { pts_.push_back({x, y}); }
  void draw(DWINDisplay *d) override;

 protected:
  uint16_t color_{0xFFFF};
  uint8_t size_{1};
  std::vector<Point> pts_;
};

class BitmapWidget : public Widget {
 public:
  void set_color(uint16_t c) { color1_ = c; }
  void set_bg_color(uint16_t c) { color0_ = c; }
  void set_data(const std::vector<uint8_t> &d) { data_ = d; }
  void draw(DWINDisplay *d) override;

 protected:
  uint16_t color1_{0xFFFF}, color0_{0};
  std::vector<uint8_t> data_;
};

// ---------------------------------------------------------------------------------------------
// Dynamic text-like widgets
// ---------------------------------------------------------------------------------------------
class StringWidget : public Widget {
 public:
  void set_string_func(std::function<std::string()> f) { func_ = std::move(f); }
  void set_string(const std::string &s);
  void set_static_string(const std::string &s) { str_ = s; }
  void update(DWINDisplay *d) override;
#ifdef USE_TEXT_SENSOR
  void set_text_sensor(text_sensor::TextSensor *s) {
    func_ = [s]() { return s->state; };
  }
#endif

 protected:
  const std::string &current_();
  std::string str_, shown_;
  std::function<std::string()> func_;
};

class TextWidget : public StringWidget {
 public:
  void set_size(uint8_t s) { size_ = s; }
  void set_color(uint16_t c) { color_ = c; }
  void set_bg_color(uint16_t c) {
    bg_ = c;
    has_bg_ = true;
  }
  void set_width_adjust(bool v) { wadj_ = v; }
  void draw(DWINDisplay *d) override;
  void redraw(DWINDisplay *d) override;
  void on_hide(DWINDisplay *d) override { drawn_cells_ = 0; }

 protected:
  bool left_aligned_() const { return x_ != COORD_CENTER && x_ != COORD_END; }
  uint8_t size_{3};
  uint16_t color_{0xFFFF}, bg_{0};
  bool has_bg_{false}, wadj_{false};
  size_t drawn_cells_{0};
};

class QRWidget : public StringWidget {
 public:
  void set_pixel_size(uint8_t p) { pixel_ = p; }
  void draw(DWINDisplay *d) override;

 protected:
  uint8_t pixel_{4};
};

class BarcodeWidget : public StringWidget {
 public:
  void draw(DWINDisplay *d) override;
};

class ValueWidget : public Widget {
 public:
  void set_value_func(std::function<float()> f) { func_ = std::move(f); }
  void set_value(float v);
#ifdef USE_SENSOR
  void set_sensor(sensor::Sensor *s) {
    func_ = [s]() { return s->state; };
  }
#endif

 protected:
  float current_() const { return func_ ? func_() : value_; }
  float value_{0};
  std::function<float()> func_;
};

class NumberWidget : public ValueWidget {
 public:
  void set_size(uint8_t s) { size_ = s; }
  void set_color(uint16_t c) { color_ = c; }
  void set_bg_color(uint16_t c) {
    bg_ = c;
    has_bg_ = true;
  }
  void set_digits(uint8_t i, uint8_t f) {
    digits_int_ = i;
    digits_frac_ = f;
  }
  void set_signed(bool v) { signed_ = v; }
  /// 0 = hide leading zeros, 1 = show as spaces, 2 = show as zeros
  void set_leading(uint8_t v) { leading_ = v; }
  void draw(DWINDisplay *d) override;
  void redraw(DWINDisplay *d) override;
  void update(DWINDisplay *d) override;

 protected:
  int64_t scaled_() const;
  uint8_t size_{3}, digits_int_{4}, digits_frac_{0}, leading_{0};
  uint16_t color_{0xFFFF}, bg_{0};
  bool has_bg_{false}, signed_{true}, drawn_{false};
  int64_t shown_{0};
};

class ProgressBarWidget : public ValueWidget {
 public:
  void set_range(float lo, float hi) {
    min_ = lo;
    max_ = hi;
  }
  void set_color(uint16_t c) { color_ = c; }
  void set_bg_color(uint16_t c) { bg_ = c; }
  void set_border_color(uint16_t c) {
    border_ = c;
    has_border_ = true;
  }
  void set_vertical(bool v) { vertical_ = v; }
  void draw(DWINDisplay *d) override;
  void update(DWINDisplay *d) override;

 protected:
  Rect inner_() const;
  int fill_px_() const;
  float min_{0}, max_{100};
  uint16_t color_{0x07E0}, bg_{0}, border_{0xFFFF};
  bool has_border_{false}, vertical_{false};
  int shown_px_{-1};
};

// ---------------------------------------------------------------------------------------------
// Pictures / icons
// ---------------------------------------------------------------------------------------------
/// Copies a region of a stored JPEG (or of the current page picture) to the screen (0x25 + 0x27)
class ImageWidget : public Widget {
 public:
  void set_picture(int8_t id) { picture_ = id; }
  void set_source(int16_t x, int16_t y) {
    sx_ = x;
    sy_ = y;
  }
  void set_transparent(bool v) { transparent_ = v; }
  void set_restore_background(bool v) { restore_ = v; }
  void set_strong_filter(bool v) { strong_ = v; }
  void draw(DWINDisplay *d) override;

 protected:
  int8_t picture_{-1};
  int16_t sx_{0}, sy_{0};
  bool transparent_{false}, restore_{false}, strong_{false};
};

/// Icons from a JPEG icon library stored in picture memory (0x23)
class IconWidget : public Widget {
 public:
  void set_library(uint8_t l) { lib_ = l; }
  void set_icons(const std::vector<uint8_t> &v) { icons_ = v; }
  void set_icons_func(std::function<std::vector<uint8_t>()> f) { func_ = std::move(f); }
  void set_icon_size(uint8_t w, uint8_t h) {
    iw_ = w;
    ih_ = h;
  }
  void set_transparent(bool v) { transparent_ = v; }
  void set_restore_background(bool v) { restore_ = v; }
  void set_strong_filter(bool v) { strong_ = v; }
  void draw(DWINDisplay *d) override;
  void update(DWINDisplay *d) override;

 protected:
  uint8_t mode_() const { return (transparent_ ? 0 : 0x80) | (restore_ ? 0x40 : 0) | (strong_ ? 0x20 : 0) | (lib_ & 0x0F); }
  uint8_t lib_{0}, iw_{28}, ih_{45};
  bool transparent_{false}, restore_{false}, strong_{false};
  std::vector<uint8_t> icons_, shown_;
  std::function<std::vector<uint8_t>()> func_;
};

/// JPEG icon whose data lives in SRAM (0x24)
class SramIconWidget : public Widget {
 public:
  void set_address(uint16_t a) { address_ = a; }
  void set_icon_size(uint8_t w, uint8_t h) {
    iw_ = w;
    ih_ = h;
  }
  void set_transparent(bool v) { transparent_ = v; }
  void set_strong_filter(bool v) { strong_ = v; }
  void draw(DWINDisplay *d) override;

 protected:
  uint16_t address_{0};
  uint8_t iw_{28}, ih_{45};
  bool transparent_{false}, strong_{false};
};

/// Self-running icon animation (0x28/0x29). Runs while the page is shown.
class AnimationWidget : public Widget {
 public:
  void set_group(uint8_t g) { group_ = g; }
  void set_library(uint8_t l) { lib_ = l; }
  void set_icons(uint8_t first, uint8_t last) {
    first_ = first;
    last_ = last;
  }
  void set_delay(uint8_t d10) { delay_ = d10; }
  void set_restart(bool v) { restart_ = v; }
  void draw(DWINDisplay *d) override {}
  void on_show(DWINDisplay *d) override;
  void on_hide(DWINDisplay *d) override;

 protected:
  uint8_t group_{0}, lib_{0}, first_{0}, last_{0}, delay_{10};
  bool restart_{true}, running_{false};
};

// ---------------------------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------------------------
enum class SelectorType : uint8_t { BOX, ARROW, HIGHLIGHT };

class MenuItem {
 public:
  void set_text(const std::string &t) { text_ = t; }
  void set_text_func(std::function<std::string()> f) { text_f_ = std::move(f); }
  void set_target(Page *p) { target_ = p; }
  void set_back(bool b) { back_ = b; }
  void add_enter_callback(std::function<void()> cb) { enter_cbs_.push_back(std::move(cb)); }
  void add_select_callback(std::function<void()> cb) { select_cbs_.push_back(std::move(cb)); }
  std::string text() const { return text_f_ ? text_f_() : text_; }
  void fire_enter() {
    for (auto &c : enter_cbs_)
      c();
  }
  void fire_select() {
    for (auto &c : select_cbs_)
      c();
  }
  Page *target() const { return target_; }
  bool back() const { return back_; }
  std::string shown;

 protected:
  std::string text_;
  std::function<std::string()> text_f_;
  Page *target_{nullptr};
  bool back_{false};
  std::vector<std::function<void()>> enter_cbs_, select_cbs_;
};

class MenuItemTrigger : public Trigger<> {
 public:
  MenuItemTrigger(MenuItem *item, bool on_select) {
    if (on_select)
      item->add_select_callback([this]() { this->trigger(); });
    else
      item->add_enter_callback([this]() { this->trigger(); });
  }
};

class MenuWidget : public Widget {
 public:
  void add_item(MenuItem *i) { items_.push_back(i); }
  void set_size(uint8_t s) { size_ = s; }
  void set_padding(uint8_t p) { pad_ = p; }
  void set_item_height(int16_t h) { item_h_cfg_ = h; }
  void set_bg_color(uint16_t c) { bg_ = c; }
  void set_item_color(uint16_t c) { item_color_ = c; }
  void set_selector(SelectorType t, uint16_t color) {
    sel_type_ = t;
    sel_color_ = color;
  }
  void set_highlight_text_color(uint16_t c) {
    hl_text_ = c;
    has_hl_text_ = true;
  }
  void set_title(const std::string &t) { title_ = t; }
  void set_title_color(uint16_t c) {
    title_color_ = c;
    has_title_color_ = true;
  }
  void set_title_bg_color(uint16_t c) {
    title_bg_ = c;
    has_title_bg_ = true;
  }
  void set_wrap(bool v) { wrap_ = v; }
  void set_scrollbar(bool v, uint16_t color) {
    scrollbar_ = v;
    scrollbar_color_ = color;
  }

  void draw(DWINDisplay *d) override;
  void update(DWINDisplay *d) override;
  bool focusable() const override { return true; }
  bool on_move(DWINDisplay *d, int delta) override;
  bool on_click(DWINDisplay *d) override;

  // for actions
  void move(int delta);
  void select(int index);
  void activate();
  int selected() const { return sel_; }

 protected:
  void layout_(DWINDisplay *d);
  void draw_items_(DWINDisplay *d);
  void draw_row_(DWINDisplay *d, int idx);
  void draw_scrollbar_(DWINDisplay *d);
  void select_(DWINDisplay *d, int idx);
  bool on_screen_(DWINDisplay *d) const;
  bool has_scrollbar_() const { return scrollbar_ && static_cast<int>(items_.size()) > visible_; }

  std::vector<MenuItem *> items_;
  uint8_t size_{3}, pad_{4};
  int16_t item_h_cfg_{0};
  uint16_t bg_{0}, item_color_{0xFFFF}, sel_color_{0xF800}, hl_text_{0}, title_color_{0xFFFF}, title_bg_{0};
  uint16_t scrollbar_color_{0x7BEF};
  bool has_hl_text_{false}, has_title_color_{false}, has_title_bg_{false}, wrap_{false}, scrollbar_{true};
  SelectorType sel_type_{SelectorType::BOX};
  std::string title_;
  int sel_{0}, top_{0};
  int ax1_{0}, ay1_{0}, ax2_{0}, ay2_{0}, list_y_{0}, item_h_{24}, visible_{1};
};

}  // namespace dwin
}  // namespace esphome
