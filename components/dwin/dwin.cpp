#include "dwin.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace dwin {

static const char *const TAG = "dwin";

// =============================================================================================
// Widget
// =============================================================================================
DWINDisplay *Widget::display() const { return this->page_ != nullptr ? this->page_->display() : nullptr; }

int Widget::resolve_x_(DWINDisplay *d, int w) const {
  if (this->x_ == COORD_CENTER)
    return (d->width() - w) / 2;
  if (this->x_ == COORD_END)
    return d->width() - w;
  return this->x_;
}

int Widget::resolve_y_(DWINDisplay *d, int h) const {
  if (this->y_ == COORD_CENTER)
    return (d->height() - h) / 2;
  if (this->y_ == COORD_END)
    return d->height() - h;
  return this->y_;
}

void Widget::redraw(DWINDisplay *d) {
  d->erase_area(this->bounds_);
  this->draw(d);
}

void Widget::refresh() {
  DWINDisplay *d = this->display();
  if (d == nullptr || !d->is_ready() || d->current_page() != this->page_ || this->hidden_)
    return;
  this->redraw(d);
}

void Widget::show() {
  if (!this->hidden_)
    return;
  this->hidden_ = false;
  DWINDisplay *d = this->display();
  if (d != nullptr && d->is_ready() && d->current_page() == this->page_) {
    this->draw(d);
    this->on_show(d);
  }
}

void Widget::hide() {
  if (this->hidden_)
    return;
  DWINDisplay *d = this->display();
  if (d != nullptr && d->is_ready() && d->current_page() == this->page_) {
    this->on_hide(d);
    d->erase_area(this->bounds_);
  }
  this->hidden_ = true;
}

// =============================================================================================
// Page
// =============================================================================================
void Page::fire(PageEvent ev) {
  for (auto &c : this->callbacks_)
    if (c.first == ev)
      c.second();
}

Widget *Page::focus_() const {
  for (auto *w : this->widgets_)
    if (!w->hidden() && w->focusable())
      return w;
  return nullptr;
}

void Page::render(DWINDisplay *d) {
  for (auto *w : this->widgets_)
    w->on_hide(d);  // resets per-render widget state
  if (this->background_image_ >= 0)
    d->show_picture(static_cast<uint8_t>(this->background_image_));
  else
    d->clear_screen(this->background_);
  for (auto *w : this->widgets_)
    if (!w->hidden())
      w->draw(d);
  for (auto *w : this->widgets_)
    if (!w->hidden())
      w->on_show(d);
}

void Page::update(DWINDisplay *d) {
  for (auto *w : this->widgets_)
    if (!w->hidden())
      w->update(d);
}

void Page::leave(DWINDisplay *d) {
  for (auto *w : this->widgets_)
    w->on_hide(d);
  this->fire(PageEvent::UNLOAD);
}

bool Page::handle_move(DWINDisplay *d, int delta) {
  Widget *w = this->focus_();
  return w != nullptr && w->on_move(d, delta);
}

bool Page::handle_click(DWINDisplay *d) {
  Widget *w = this->focus_();
  return w != nullptr && w->on_click(d);
}

// =============================================================================================
// DWINDisplay: lifecycle
// =============================================================================================
void DWINDisplay::setup() {
  this->boot_start_ = millis();
  this->last_activity_ = this->boot_start_;
  this->tx_ready_us_ = 0;
  this->tx_sent_us_ = micros();

#ifdef DWIN_USE_ENCODER
  if (this->encoder_ != nullptr) {
    this->encoder_->add_on_clockwise_callback(
        [this]() { this->input(this->encoder_invert_ ? Input::ANTICLOCKWISE : Input::CLOCKWISE); });
    this->encoder_->add_on_anticlockwise_callback(
        [this]() { this->input(this->encoder_invert_ ? Input::CLOCKWISE : Input::ANTICLOCKWISE); });
  }
#endif
#ifdef DWIN_USE_ENTER
  if (this->enter_ != nullptr)
    this->enter_->add_on_state_callback([this](bool state) { this->button_changed(state); });
#endif
}

void DWINDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "DWIN T5UIC1 display:");
  ESP_LOGCONFIG(TAG, "  Panel: %ux%u, direction=%u (logical %dx%d)", this->panel_w_, this->panel_h_, this->direction_,
                this->width(), this->height());
  ESP_LOGCONFIG(TAG, "  Pages: %u, handshake: %s", (unsigned) this->pages_.size(), YESNO(this->handshake_));
  ESP_LOGCONFIG(TAG, "  Brightness: %.0f%%, sleep after %ums to %.0f%%", this->brightness_pct_,
                (unsigned) this->sleep_delay_, this->sleep_pct_);
}

void DWINDisplay::init_() {
  this->state_ = State::READY;
  ESP_LOGI(TAG, "Display ready%s", this->handshake_ok_ ? "" : " (no handshake reply received)");
  this->set_orientation(this->direction_);
  this->apply_brightness_();
  Page *start = this->start_page_ != nullptr ? this->start_page_ : this->home_;
  if (start == nullptr && !this->pages_.empty())
    start = this->pages_.front();
  if (this->home_ == nullptr)
    this->home_ = start;
  this->last_activity_ = millis();
  this->show_page(start, false);
  this->fire_(DisplayEvent::READY);
}

void DWINDisplay::loop() {
  const uint32_t now = millis();
  this->read_rx_();

  if (this->state_ == State::BOOT) {
    if (!this->handshake_) {
      if (now - this->boot_start_ >= 1500)
        this->init_();
    } else if (this->handshake_ok_) {
      this->init_();
    } else {
      if (!this->hs_sent_ || now - this->last_hs_ >= 500) {
        this->hs_sent_ = true;
        this->last_hs_ = now;
        this->send_handshake();
      }
      if (now - this->boot_start_ >= this->boot_timeout_) {
        ESP_LOGW(TAG, "No handshake reply from display (check RX wiring); continuing anyway");
        this->init_();
      }
    }
  }

  this->flush_tx_();
  if (this->state_ != State::READY)
    return;

  if (this->auto_update_ && this->dirty_ && this->tx_queue_.empty()) {
    this->dirty_ = false;
    this->update_lcd();
  }

  // long press while still holding the button
  if (this->pressed_ && !this->long_fired_ && now - this->press_start_ >= this->long_press_ms_) {
    this->long_fired_ = true;
    this->input(Input::LONG_CLICK);
  }

  const uint32_t idle = now - this->last_activity_;
  if (!this->sleeping_ && this->sleep_delay_ > 0 && idle >= this->sleep_delay_ &&
      (this->sleep_any_page_ || this->current_ == this->home_))
    this->sleep();
  if (this->return_home_delay_ > 0 && this->current_ != this->home_ && this->home_ != nullptr &&
      idle >= this->return_home_delay_)
    this->go_home();

  if (!this->sleeping_ && this->current_ != nullptr && now - this->last_update_ >= this->update_interval_ &&
      this->tx_queue_.size() < 6) {
    this->last_update_ = now;
    this->current_->update(this);
  }
}

// =============================================================================================
// Navigation
// =============================================================================================
void DWINDisplay::show_page(Page *p, bool push) {
  if (p == nullptr)
    return;
  if (this->state_ != State::READY) {
    this->start_page_ = p;
    return;
  }
  if (this->current_ != nullptr) {
    this->current_->leave(this);
    if (p == this->home_) {
      this->history_.clear();
    } else if (push && this->current_ != p) {
      this->history_.push_back(this->current_);
      if (this->history_.size() > 16)
        this->history_.erase(this->history_.begin());
    }
  }
  this->current_ = p;
  p->render(this);
  p->fire(PageEvent::LOAD);
  this->fire_(DisplayEvent::PAGE_CHANGE);
}

void DWINDisplay::back() {
  if (this->history_.empty())
    return;
  Page *p = this->history_.back();
  this->history_.pop_back();
  this->show_page(p, false);
}

void DWINDisplay::go_home() {
  if (this->home_ == nullptr || this->current_ == this->home_)
    return;
  this->history_.clear();
  this->show_page(this->home_, false);
}

void DWINDisplay::redraw() {
  if (this->state_ == State::READY && this->current_ != nullptr)
    this->current_->render(this);
}

// =============================================================================================
// Power / backlight
// =============================================================================================
uint8_t DWINDisplay::pct_to_dim_(float pct) const {
  if (pct <= 0.0f)
    return 0;
  if (pct >= 100.0f)
    return 0xFF;
  // 0x01-0x1F flicker on this controller, so 1..100 % maps onto 0x20..0xFF
  return static_cast<uint8_t>(0x20 + (pct / 100.0f) * (0xFF - 0x20));
}

void DWINDisplay::apply_brightness_() {
  this->set_backlight_raw(this->pct_to_dim_(this->sleeping_ ? this->sleep_pct_ : this->brightness_pct_));
}

void DWINDisplay::set_brightness(float pct) {
  this->brightness_pct_ = pct;
  if (this->state_ == State::READY && !this->sleeping_)
    this->apply_brightness_();
}

void DWINDisplay::sleep() {
  if (this->sleeping_)
    return;
  this->sleeping_ = true;
  if (this->state_ == State::READY)
    this->apply_brightness_();
  this->fire_(DisplayEvent::SLEEP);
}

void DWINDisplay::wake() {
  this->last_activity_ = millis();
  if (!this->sleeping_)
    return;
  this->sleeping_ = false;
  if (this->state_ == State::READY)
    this->apply_brightness_();
  this->fire_(DisplayEvent::WAKE);
}

void DWINDisplay::beep(uint32_t ms) {
#ifdef DWIN_USE_BUZZER
  if (this->buzzer_ == nullptr || ms == 0)
    return;
  this->buzzer_->turn_on();
  this->set_timeout("dwin_beep", ms, [this]() { this->buzzer_->turn_off(); });
#endif
}

// =============================================================================================
// Input
// =============================================================================================
void DWINDisplay::input(Input ev) {
  this->last_activity_ = millis();
  if (this->state_ != State::READY || this->current_ == nullptr)
    return;
  if (this->sleeping_) {  // first interaction only wakes the screen
    this->wake();
    return;
  }
  Page *p = this->current_;
  switch (ev) {
    case Input::CLOCKWISE:
    case Input::ANTICLOCKWISE: {
      this->beep(this->move_beep_ms_);
      const int delta = ev == Input::CLOCKWISE ? 1 : -1;
      if (!p->handle_move(this, delta))
        p->fire(ev == Input::CLOCKWISE ? PageEvent::CLOCKWISE : PageEvent::ANTICLOCKWISE);
      break;
    }
    case Input::CLICK:
      this->beep(this->click_beep_ms_);
      if (!p->handle_click(this))
        p->fire(PageEvent::CLICK);
      break;
    case Input::LONG_CLICK:
      this->beep(this->click_beep_ms_);
      if (p->handles_long_click())
        p->fire(PageEvent::LONG_CLICK);
      else if (this->long_press_back_)
        this->back();
      break;
  }
}

void DWINDisplay::button_changed(bool pressed) {
  if (this->state_ != State::READY)
    return;
  if (pressed) {
    this->last_activity_ = millis();
    if (this->sleeping_) {
      this->wake();
      this->swallow_release_ = true;
      return;
    }
    this->pressed_ = true;
    this->long_fired_ = false;
    this->press_start_ = millis();
  } else {
    if (this->swallow_release_) {
      this->swallow_release_ = false;
      return;
    }
    if (!this->pressed_)
      return;
    this->pressed_ = false;
    if (!this->long_fired_)
      this->input(Input::CLICK);
  }
}

// =============================================================================================
// TX queue (paced by the processing times given in the guide) and RX parser
// =============================================================================================
void DWINDisplay::enqueue_(std::vector<uint8_t> &&frame, float cost_us) {
  if (this->tx_queue_.size() >= 400) {
    ESP_LOGW(TAG, "TX queue full, dropping frame 0x%02X", frame.size() > 1 ? frame[1] : 0);
    return;
  }
  if (frame.size() > 1 && frame[1] >= 0x01 && frame[1] <= 0x2F)
    this->dirty_ = true;  // a drawing command: needs a 0x3D commit once the queue drains
  this->tx_queue_.push_back(TxFrame{std::move(frame), static_cast<uint32_t>(cost_us * this->cost_scale_)});
  this->high_freq_.start();
}

void DWINDisplay::flush_tx_() {
  uint32_t baud = this->parent_->get_baud_rate();
  if (baud == 0)
    baud = 115200;
  int budget = 8;
  while (!this->tx_queue_.empty() && budget-- > 0) {
    if (static_cast<uint32_t>(micros() - this->tx_sent_us_) < this->tx_ready_us_)
      break;
    TxFrame &f = this->tx_queue_.front();
    this->write_array(f.bytes);
    const uint32_t tx_us = static_cast<uint32_t>(static_cast<uint64_t>(f.bytes.size()) * 10ULL * 1000000ULL / baud);
    this->tx_sent_us_ = micros();
    this->tx_ready_us_ = tx_us + f.cost_us;  // wait this long before the next frame
    this->tx_queue_.pop_front();
  }
  if (this->tx_queue_.empty())
    this->high_freq_.stop();
}

void DWINDisplay::read_rx_() {
  uint8_t b;
  while (this->available()) {
    if (!this->read_byte(&b))
      break;
    if (this->rx_.empty() && b != 0xAA)
      continue;
    this->rx_.push_back(b);
    this->rx_last_ = millis();
    const size_t n = this->rx_.size();
    if (n > 300) {
      this->rx_.clear();
      continue;
    }
    if (n >= 6 && this->rx_[n - 4] == 0xCC && this->rx_[n - 3] == 0x33 && this->rx_[n - 2] == 0xC3 &&
        this->rx_[n - 1] == 0x3C) {
      this->process_frame_();
      this->rx_.clear();
    }
  }
  if (!this->rx_.empty() && millis() - this->rx_last_ > 200)
    this->rx_.clear();
}

void DWINDisplay::process_frame_() {
  const uint8_t cmd = this->rx_[1];
  const uint8_t *p = this->rx_.data() + 2;
  const size_t n = this->rx_.size() - 6;
  ESP_LOGV(TAG, "RX cmd 0x%02X, %u data bytes", cmd, (unsigned) n);
  switch (cmd) {
    case 0x00:
      if (n >= 2 && p[0] == 0x4F && p[1] == 0x4B)
        this->handshake_ok_ = true;
      break;
    case 0x31:  // flash write ack (SRAM writes are not acknowledged)
    case 0x33:  // SRAM -> picture memory done
      this->fire_(DisplayEvent::MEMORY_WRITTEN);
      break;
    case 0x32:
      if (n >= 4) {
        const uint8_t type = p[0];
        const uint16_t addr = (static_cast<uint16_t>(p[1]) << 8) | p[2];
        const size_t len = std::min<size_t>(p[3], n - 4);
        std::vector<uint8_t> data(p + 4, p + 4 + len);
        for (auto &cb : this->memory_read_cbs_)
          cb(type, addr, data);
      }
      break;
    case 0x3A:
      if (n >= 1) {
        const size_t len = std::min<size_t>(p[0], n - 1);
        std::vector<uint8_t> data(p + 1, p + 1 + len);
        for (auto &cb : this->extuart_cbs_)
          cb(data);
      }
      break;
    case 0xFF:
      ESP_LOGW(TAG, "Display reported a CRC error on the last frame");
      break;
    default:
      break;
  }
}

void DWINDisplay::fire_(DisplayEvent ev) {
  for (auto &c : this->event_cbs_)
    if (c.first == ev)
      c.second();
}

// =============================================================================================
// Protocol commands
// =============================================================================================
void DWINDisplay::send_handshake() {
  FrameBuilder f(0x00);
  this->enqueue_(f.finish(), 1000);
}

void DWINDisplay::update_lcd() {
  FrameBuilder f(0x3D);
  this->enqueue_(f.finish(), 3000);
}

void DWINDisplay::set_backlight_raw(uint8_t dim) {
  FrameBuilder f(0x30);
  f.u8(dim);
  this->enqueue_(f.finish(), 200);
}

void DWINDisplay::set_orientation(uint8_t dir) {
  FrameBuilder f(0x34);
  f.u8(0x5A).u8(0xA5).u8(dir & 3);
  this->enqueue_(f.finish(), 100000);
}

void DWINDisplay::clear_screen(uint16_t color) {
  FrameBuilder f(0x01);
  f.u16(color);
  this->enqueue_(f.finish(), 1500);
}

void DWINDisplay::draw_points(uint16_t color, uint8_t nx, uint8_t ny, const std::vector<Point> &pts) {
  nx = std::max<uint8_t>(1, std::min<uint8_t>(nx, 15));
  ny = std::max<uint8_t>(1, std::min<uint8_t>(ny, 15));
  size_t i = 0;
  while (i < pts.size()) {
    const size_t cnt = std::min<size_t>(60, pts.size() - i);
    FrameBuilder f(0x02);
    f.u16(color).u8(nx).u8(ny);
    for (size_t k = 0; k < cnt; k++)
      f.pos(pts[i + k].x).pos(pts[i + k].y);
    this->enqueue_(f.finish(), 0.4f * nx * ny * cnt);
    i += cnt;
  }
}

void DWINDisplay::draw_polyline(uint16_t color, const std::vector<Point> &pts) {
  if (pts.size() < 2)
    return;
  const size_t n = std::min<size_t>(pts.size(), 61);
  FrameBuilder f(0x03);
  f.u16(color);
  float cost = 0;
  for (size_t i = 0; i < n; i++) {
    f.pos(pts[i].x).pos(pts[i].y);
    if (i > 0)
      cost += 0.5f * std::max(std::abs(pts[i].x - pts[i - 1].x), std::abs(pts[i].y - pts[i - 1].y));
  }
  this->enqueue_(f.finish(), cost);
}

void DWINDisplay::draw_rect(uint8_t mode, uint16_t color, int x1, int y1, int x2, int y2) {
  const int W = this->width(), H = this->height();
  if (x1 > x2)
    std::swap(x1, x2);
  if (y1 > y2)
    std::swap(y1, y2);
  if (x2 < 0 || y2 < 0 || x1 >= W || y1 >= H)
    return;
  x1 = std::max(x1, 0);
  y1 = std::max(y1, 0);
  x2 = std::min(x2, W - 1);
  y2 = std::min(y2, H - 1);
  FrameBuilder f(0x05);
  f.u8(mode).u16(color).pos(x1).pos(y1).pos(x2).pos(y2);
  this->enqueue_(f.finish(), 0.14f * (x2 - x1 + 1) * (y2 - y1 + 1));
}

void DWINDisplay::fill_rect(uint16_t color, const Rect &r) {
  if (!r.empty())
    this->draw_rect(1, color, r.x1, r.y1, r.x2, r.y2);
}

void DWINDisplay::draw_bitmap(uint16_t color1, uint16_t color0, int x, int y, int width, const uint8_t *data,
                              size_t len) {
  if (width <= 0 || data == nullptr)
    return;
  const size_t stride = (width + 7) / 8;
  const size_t rows = len / stride;
  const size_t rows_per_frame = std::max<size_t>(1, (MAX_FRAME_DATA - 10) / stride);
  size_t row = 0;
  while (row < rows) {
    const size_t cnt = std::min(rows_per_frame, rows - row);
    FrameBuilder f(0x08);
    f.pos(x).pos(y + row).u16(width).u16(color1).u16(color0);
    f.bytes(data + row * stride, cnt * stride);
    this->enqueue_(f.finish(), 0.22f * width * cnt);
    row += cnt;
  }
}

void DWINDisplay::move_area(uint8_t mode, uint16_t distance, uint16_t fill_color, int x1, int y1, int x2, int y2) {
  FrameBuilder f(0x09);
  f.u8(mode).u16(distance).u16(fill_color).pos(x1).pos(y1).pos(x2).pos(y2);
  this->enqueue_(f.finish(), 0.2f * std::abs(x2 - x1 + 1) * std::abs(y2 - y1 + 1));
}

void DWINDisplay::draw_text(uint8_t size, uint16_t color, uint16_t bcolor, bool bg, bool width_adjust, int x, int y,
                            const std::string &text) {
  if (text.empty())
    return;
  std::string t = text;
  if (t.size() > MAX_FRAME_DATA - 9) {
    t.resize(MAX_FRAME_DATA - 9);
    // do not cut a GB2312 double byte character in half
    size_t hi = 0;
    for (size_t i = t.size(); i > 0 && static_cast<uint8_t>(t[i - 1]) >= 0x80; i--)
      hi++;
    if (hi & 1)
      t.pop_back();
  }
  FrameBuilder f(0x11);
  f.u8((width_adjust ? 0x80 : 0) | (bg ? 0x40 : 0) | (size & 0x0F));
  f.u16(color).u16(bcolor).pos(x).pos(y);
  f.bytes(reinterpret_cast<const uint8_t *>(t.data()), t.size());
  const FontMetrics fm = font_metrics(size);
  this->enqueue_(f.finish(), 0.3f * fm.w * fm.h * t.size());
}

void DWINDisplay::draw_number(uint8_t size, uint16_t color, uint16_t bcolor, bool bg, bool is_signed, bool lead_show,
                              bool lead_zero, uint8_t digits_int, uint8_t digits_frac, int x, int y,
                              int64_t scaled) {
  digits_int = std::max<uint8_t>(1, std::min<uint8_t>(digits_int, 20));
  digits_frac = std::min<uint8_t>(digits_frac, 20);
  FrameBuilder f(0x14);
  f.u8((bg ? 0x80 : 0) | (is_signed ? 0x40 : 0) | (lead_show ? 0x20 : 0) | (lead_zero ? 0x10 : 0) | (size & 0x0F));
  f.u16(color).u16(bcolor).u8(digits_int).u8(digits_frac).pos(x).pos(y);
  if (scaled >= INT32_MIN && scaled <= INT32_MAX) {
    const uint32_t v = static_cast<uint32_t>(static_cast<int32_t>(scaled));
    f.u16(v >> 16).u16(v & 0xFFFF);
  } else {
    const uint64_t v = static_cast<uint64_t>(scaled);
    f.u16(v >> 48).u16((v >> 32) & 0xFFFF).u16((v >> 16) & 0xFFFF).u16(v & 0xFFFF);
  }
  const FontMetrics fm = font_metrics(size);
  this->enqueue_(f.finish(), 0.3f * fm.w * fm.h * (digits_int + digits_frac + 2));
}

void DWINDisplay::draw_qr(int x, int y, uint8_t pixel, const std::string &data) {
  pixel = std::max<uint8_t>(1, std::min<uint8_t>(pixel, 15));
  std::string t = data.substr(0, 154);
  FrameBuilder f(0x21);
  f.pos(x).pos(y).u8(pixel);
  f.bytes(reinterpret_cast<const uint8_t *>(t.data()), t.size());
  this->enqueue_(f.finish(), 7500.0f * pixel * pixel / 16.0f);
}

void DWINDisplay::show_picture(uint8_t jpeg_id) {
  FrameBuilder f(0x22);
  f.u8(0x00).u8(jpeg_id & 0x0F);
  this->enqueue_(f.finish(), 280000);
}

void DWINDisplay::show_icons(int x, int y, uint8_t mode, const std::vector<uint8_t> &ids) {
  if (ids.empty())
    return;
  const size_t n = std::min<size_t>(ids.size(), MAX_FRAME_DATA - 5);
  FrameBuilder f(0x23);
  f.pos(x).pos(y).u8(mode);
  f.bytes(ids.data(), n);
  this->enqueue_(f.finish(), 3200.0f * n);
}

void DWINDisplay::show_sram_icon(int x, int y, uint8_t mode, uint16_t address) {
  FrameBuilder f(0x24);
  f.pos(x).pos(y).u8(mode).u16(address);
  this->enqueue_(f.finish(), 3100);
}

void DWINDisplay::load_vda1(uint8_t jpeg_id) {
  if (this->vda1_pic_ == jpeg_id)
    return;
  this->vda1_pic_ = jpeg_id;
  FrameBuilder f(0x25);
  f.u8(0x01).u8(jpeg_id & 0x0F);
  this->enqueue_(f.finish(), 260000);
}

void DWINDisplay::copy_vda1(int sx1, int sy1, int sx2, int sy2, int x, int y) {
  FrameBuilder f(0x26);
  f.pos(sx1).pos(sy1).pos(sx2).pos(sy2).pos(x).pos(y);
  this->enqueue_(f.finish(), 0.2f * std::abs(sx2 - sx1 + 1) * std::abs(sy2 - sy1 + 1));
}

void DWINDisplay::copy_vda(uint8_t mode, int sx1, int sy1, int sx2, int sy2, int x, int y) {
  FrameBuilder f(0x27);
  f.u8(mode).pos(sx1).pos(sy1).pos(sx2).pos(sy2).pos(x).pos(y);
  this->enqueue_(f.finish(), 0.2f * std::abs(sx2 - sx1 + 1) * std::abs(sy2 - sy1 + 1));
}

void DWINDisplay::animation_setup(uint8_t group, uint8_t lib, uint8_t first, uint8_t last, uint8_t delay_10ms,
                                  bool restart, int x, int y) {
  FrameBuilder f(0x28);
  f.pos(x).pos(y).u8(0x80 | (restart ? 0x40 : 0) | (group & 0x0F)).u8(lib & 0x0F).u8(first).u8(last).u8(delay_10ms);
  this->enqueue_(f.finish(), 500);
}

void DWINDisplay::animation_enable(uint8_t group, bool on) {
  const uint16_t bit = static_cast<uint16_t>(1u << (group & 0x0F));
  if (on)
    this->cartoon_mask_ |= bit;
  else
    this->cartoon_mask_ &= ~bit;
  FrameBuilder f(0x29);
  f.u16(this->cartoon_mask_);
  this->enqueue_(f.finish(), 500);
}

void DWINDisplay::draw_barcode(int x, int y, const uint8_t *digits12) {
  FrameBuilder f(0x2A);
  f.pos(x & ~1).pos(y & ~1);
  f.bytes(digits12, 12);
  this->enqueue_(f.finish(), 500);
}

void DWINDisplay::erase_area(const Rect &r) {
  if (r.empty())
    return;
  if (this->current_ != nullptr && this->current_->background_image() >= 0)
    this->copy_vda(0x80, r.x1, r.y1, r.x2, r.y2, r.x1, r.y1);  // restore from the cached page picture (VDA 0)
  else
    this->fill_rect(this->current_ != nullptr ? this->current_->background() : 0, r);
}

void DWINDisplay::write_memory(uint8_t type, uint16_t address, const std::vector<uint8_t> &data) {
  const bool flash = type == 0xA5;
  const size_t limit = flash ? 0x4000 : 0x8000;
  size_t pos = 0;
  while (pos < data.size() && address + pos < limit) {
    const size_t cnt = std::min<size_t>(std::min<size_t>(245, data.size() - pos), limit - (address + pos));
    FrameBuilder f(0x31);
    f.u8(type).u16(address + pos);
    f.bytes(data.data() + pos, cnt);
    this->enqueue_(f.finish(), flash ? 1000000 : 0);
    pos += cnt;
  }
  if (pos < data.size())
    ESP_LOGW(TAG, "write_memory: data beyond end of %s memory was dropped", flash ? "Flash" : "SRAM");
}

void DWINDisplay::read_memory(uint8_t type, uint16_t address, uint8_t length) {
  const bool flash = type == 0xA5;
  size_t remaining = length;
  size_t pos = 0;
  while (remaining > 0) {
    const uint8_t cnt = static_cast<uint8_t>(std::min<size_t>(remaining, 0xF0));
    FrameBuilder f(0x32);
    f.u8(type).u16(address + pos).u8(cnt);
    this->enqueue_(f.finish(), flash ? 100 : 0);
    pos += cnt;
    remaining -= cnt;
  }
}

void DWINDisplay::sram_to_picture(uint8_t pic_id) {
  FrameBuilder f(0x33);
  f.u8(0x5A).u8(0xA5).u8(pic_id & 0x0F);
  this->enqueue_(f.finish(), 2000000);
  if (this->vda1_pic_ == (pic_id & 0x0F))
    this->vda1_pic_ = -1;  // cached copy is stale now
}

void DWINDisplay::store_picture(uint8_t pic_id, const uint8_t *data, size_t len) {
  if (len > 0x8000) {
    ESP_LOGW(TAG, "store_picture: %u bytes exceeds the 32 KB slot", (unsigned) len);
    len = 0x8000;
  }
  this->write_memory(0x5A, 0, std::vector<uint8_t>(data, data + len));
  this->sram_to_picture(pic_id);
}

void DWINDisplay::extuart_set_baud(uint32_t baud) {
  if (baud == 0)
    return;
  uint32_t v = 15667200UL / baud;
  v = std::max<uint32_t>(1, std::min<uint32_t>(v, 0x3FF));
  FrameBuilder f(0x38);
  f.u16(static_cast<uint16_t>(v));
  this->enqueue_(f.finish(), 500);
}

void DWINDisplay::extuart_send(const std::vector<uint8_t> &data) {
  size_t pos = 0;
  while (pos < data.size()) {
    const size_t cnt = std::min<size_t>(MAX_FRAME_DATA, data.size() - pos);
    FrameBuilder f(0x39);
    f.bytes(data.data() + pos, cnt);
    this->enqueue_(f.finish(), 500);
    pos += cnt;
  }
}

void DWINDisplay::send_raw(uint8_t cmd, const std::vector<uint8_t> &data, uint32_t cost_us) {
  FrameBuilder f(cmd);
  f.bytes(data.data(), std::min<size_t>(data.size(), MAX_FRAME_DATA));
  this->enqueue_(f.finish(), cost_us);
}

}  // namespace dwin
}  // namespace esphome
