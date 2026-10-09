#include "widgets.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace dwin {

static const char *const TAG = "dwin.widgets";

static Rect bbox_of(const std::vector<Point> &pts, int grow = 0) {
  Rect r;
  if (pts.empty())
    return r;
  r.x1 = r.x2 = pts[0].x;
  r.y1 = r.y2 = pts[0].y;
  for (auto &p : pts) {
    r.x1 = std::min<int>(r.x1, p.x);
    r.y1 = std::min<int>(r.y1, p.y);
    r.x2 = std::max<int>(r.x2, p.x);
    r.y2 = std::max<int>(r.y2, p.y);
  }
  r.x2 += grow;
  r.y2 += grow;
  return r;
}

// =============================================================================================
// Primitives
// =============================================================================================
void RectangleWidget::draw(DWINDisplay *d) {
  const int w = this->width_ > 0 ? this->width_ : d->width() - std::max<int>(this->x_, 0);
  const int h = this->height_ > 0 ? this->height_ : d->height() - std::max<int>(this->y_, 0);
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, h);
  this->bounds_ = Rect::from_size(x, y, w, h);
  const Rect &r = this->bounds_;
  if (this->xor_) {
    d->draw_rect(2, this->color_, r.x1, r.y1, r.x2, r.y2);
  } else if (this->fill_) {
    d->draw_rect(1, this->color_, r.x1, r.y1, r.x2, r.y2);
    if (this->has_border_)
      d->draw_rect(0, this->border_color_, r.x1, r.y1, r.x2, r.y2);
  } else {
    d->draw_rect(0, this->color_, r.x1, r.y1, r.x2, r.y2);
  }
}

void LineWidget::draw(DWINDisplay *d) {
  d->draw_polyline(this->color_, this->pts_);
  this->bounds_ = bbox_of(this->pts_);
}

void PointsWidget::draw(DWINDisplay *d) {
  d->draw_points(this->color_, this->size_, this->size_, this->pts_);
  this->bounds_ = bbox_of(this->pts_, this->size_ - 1);
}

void BitmapWidget::draw(DWINDisplay *d) {
  const int w = this->width_ > 0 ? this->width_ : 8;
  const int stride = (w + 7) / 8;
  const int rows = static_cast<int>(this->data_.size()) / stride;
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, rows);
  // Drawn with rectangles (background + one per run of set bits): command 0x08 did not render on
  // tested hardware even with V2.3 firmware.
  d->fill_rect(this->color0_, Rect::from_size(x, y, w, rows));
  for (int r = 0; r < rows; r++) {
    int run = -1;
    for (int c = 0; c <= w; c++) {
      const bool on = c < w && (this->data_[r * stride + c / 8] & (0x80 >> (c % 8)));
      if (on && run < 0)
        run = c;
      if (!on && run >= 0) {
        d->draw_rect(1, this->color1_, x + run, y + r, x + c - 1, y + r);
        run = -1;
      }
    }
  }
  this->bounds_ = Rect::from_size(x, y, w, rows);
}

// =============================================================================================
// String widgets
// =============================================================================================
const std::string &StringWidget::current_() {
  if (this->func_)
    this->str_ = this->func_();
  return this->str_;
}

void StringWidget::set_string(const std::string &s) {
  this->str_ = s;
  this->func_ = nullptr;
  if (s != this->shown_)
    this->refresh();
}

void StringWidget::update(DWINDisplay *d) {
  if (!this->func_)
    return;
  if (this->func_() != this->shown_)
    this->redraw(d);
}

void TextWidget::draw(DWINDisplay *d) {
  std::string s = this->current_();
  this->shown_ = s;
  const FontMetrics fm = font_metrics(this->size_);
  if (this->has_bg_ && this->left_aligned_()) {
    // pad with spaces so the background colour wipes any leftovers of a longer previous string
    if (s.size() < this->drawn_cells_)
      s.append(this->drawn_cells_ - s.size(), ' ');
    else
      this->drawn_cells_ = s.size();
  }
  const int w = static_cast<int>(s.size()) * fm.w;
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, fm.h);
  d->draw_text(this->size_, this->color_, this->bg_, this->has_bg_, this->wadj_, x, y, s);
  this->bounds_ = Rect::from_size(x, y, w, fm.h);
}

void TextWidget::redraw(DWINDisplay *d) {
  if (!(this->has_bg_ && this->left_aligned_())) {
    if (this->has_bg_)
      d->fill_rect(this->bg_, this->bounds_);
    else
      d->erase_area(this->bounds_);
  }
  this->draw(d);
}

void QRWidget::draw(DWINDisplay *d) {
  const std::string &s = this->current_();
  this->shown_ = s;
  const int size = 46 * this->pixel_;
  const int x = this->resolve_x_(d, size), y = this->resolve_y_(d, size);
  if (!s.empty())
    d->draw_qr(x, y, this->pixel_, s);
  this->bounds_ = Rect::from_size(x, y, size, size);
}

void BarcodeWidget::draw(DWINDisplay *d) {
  const std::string &s = this->current_();
  this->shown_ = s;
  uint8_t digits[12];
  int n = 0;
  for (char c : s) {
    if (c >= '0' && c <= '9' && n < 12)
      digits[n++] = c - '0';
  }
  const int x = this->resolve_x_(d, 222) & ~1, y = this->resolve_y_(d, 94) & ~1;
  if (n == 12)
    d->draw_barcode(x, y, digits);
  else
    ESP_LOGW(TAG, "EAN-13 barcode needs 12 digits, got %d", n);
  this->bounds_ = Rect::from_size(x, y, 222, 94);
}

// =============================================================================================
// Value widgets
// =============================================================================================
void ValueWidget::set_value(float v) {
  this->value_ = v;
  this->func_ = nullptr;
  DWINDisplay *d = this->display();
  if (d != nullptr && d->is_ready() && d->current_page() == this->page_ && !this->hidden_)
    this->update(d);
}

int64_t NumberWidget::scaled_() const {
  float v = this->current_();
  if (std::isnan(v))
    v = 0;
  double m = 1;
  for (uint8_t i = 0; i < this->digits_frac_; i++)
    m *= 10;
  return static_cast<int64_t>(std::llround(static_cast<double>(v) * m));
}

void NumberWidget::draw(DWINDisplay *d) {
  const int64_t sv = this->scaled_();
  this->shown_ = sv;
  this->drawn_ = true;
  const FontMetrics fm = font_metrics(this->size_);
  const int cells = this->digits_int_ + this->digits_frac_ + (this->digits_frac_ ? 1 : 0) + (this->signed_ ? 1 : 0);
  const int w = cells * fm.w;
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, fm.h);
  d->draw_number(this->size_, this->color_, this->bg_, this->has_bg_, this->signed_ || sv < 0, this->leading_ != 0,
                 this->leading_ == 2, this->digits_int_, this->digits_frac_, x, y, sv);
  this->bounds_ = Rect::from_size(x, y, w, fm.h);
}

void NumberWidget::redraw(DWINDisplay *d) {
  if (this->has_bg_)
    d->fill_rect(this->bg_, this->bounds_);
  else
    d->erase_area(this->bounds_);
  this->draw(d);
}

void NumberWidget::update(DWINDisplay *d) {
  if (this->drawn_ && this->scaled_() != this->shown_)
    this->redraw(d);
}

Rect ProgressBarWidget::inner_() const {
  Rect r = this->bounds_;
  if (this->has_border_) {
    r.x1++;
    r.y1++;
    r.x2--;
    r.y2--;
  }
  return r;
}

int ProgressBarWidget::fill_px_() const {
  const Rect in = this->inner_();
  const int len = this->vertical_ ? (in.y2 - in.y1 + 1) : (in.x2 - in.x1 + 1);
  const float range = this->max_ - this->min_;
  float frac = range > 0 ? (this->current_() - this->min_) / range : 0;
  if (std::isnan(frac) || frac < 0)
    frac = 0;
  if (frac > 1)
    frac = 1;
  return static_cast<int>(std::lround(frac * len));
}

void ProgressBarWidget::draw(DWINDisplay *d) {
  const int w = this->width_ > 0 ? this->width_ : (this->vertical_ ? 12 : 100);
  const int h = this->height_ > 0 ? this->height_ : (this->vertical_ ? 100 : 12);
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, h);
  this->bounds_ = Rect::from_size(x, y, w, h);
  d->fill_rect(this->bg_, this->bounds_);
  if (this->has_border_)
    d->draw_rect(0, this->border_, x, y, x + w - 1, y + h - 1);
  const int px = this->fill_px_();
  this->shown_px_ = px;
  if (px > 0) {
    const Rect in = this->inner_();
    if (this->vertical_)
      d->fill_rect(this->color_, Rect{in.x1, in.y2 - px + 1, in.x2, in.y2});
    else
      d->fill_rect(this->color_, Rect{in.x1, in.y1, in.x1 + px - 1, in.y2});
  }
}

void ProgressBarWidget::update(DWINDisplay *d) {
  if (this->shown_px_ < 0)
    return;
  const int px = this->fill_px_();
  if (px == this->shown_px_)
    return;
  const Rect in = this->inner_();
  const int lo = std::min(px, this->shown_px_), hi = std::max(px, this->shown_px_);
  const uint16_t color = px > this->shown_px_ ? this->color_ : this->bg_;
  if (this->vertical_)
    d->fill_rect(color, Rect{in.x1, in.y2 - hi + 1, in.x2, in.y2 - lo});
  else
    d->fill_rect(color, Rect{in.x1 + lo, in.y1, in.x1 + hi - 1, in.y2});
  this->shown_px_ = px;
}

// =============================================================================================
// Pictures
// =============================================================================================
void ImageWidget::draw(DWINDisplay *d) {
  const int w = std::max<int>(this->width_, 1), h = std::max<int>(this->height_, 1);
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, h);
  uint8_t mode = (this->transparent_ ? 0 : 0x80) | (this->restore_ ? 0x40 : 0) | (this->strong_ ? 0x20 : 0);
  if (this->picture_ >= 0) {
    d->load_vda1(static_cast<uint8_t>(this->picture_));
    mode |= 0x01;
  }
  d->copy_vda(mode, this->sx_, this->sy_, this->sx_ + w - 1, this->sy_ + h - 1, x, y);
  this->bounds_ = Rect::from_size(x, y, w, h);
}

void IconWidget::draw(DWINDisplay *d) {
  const std::vector<uint8_t> ids = this->func_ ? this->func_() : this->icons_;
  this->shown_ = ids;
  const int w = std::max<int>(1, static_cast<int>(ids.size())) * this->iw_;
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, this->ih_);
  d->show_icons(x, y, this->mode_(), ids);
  this->bounds_ = ids.empty() ? Rect() : Rect::from_size(x, y, w, this->ih_);
}

void IconWidget::update(DWINDisplay *d) {
  if (this->func_ && this->func_() != this->shown_)
    this->redraw(d);
}

void SramIconWidget::draw(DWINDisplay *d) {
  const int x = this->resolve_x_(d, this->iw_), y = this->resolve_y_(d, this->ih_);
  d->show_sram_icon(x, y, (this->transparent_ ? 0 : 0x80) | (this->strong_ ? 0x20 : 0), this->address_);
  this->bounds_ = Rect::from_size(x, y, this->iw_, this->ih_);
}

void AnimationWidget::on_show(DWINDisplay *d) {
  const int x = this->resolve_x_(d, this->width_ > 0 ? this->width_ : 28);
  const int y = this->resolve_y_(d, this->height_ > 0 ? this->height_ : 45);
  d->animation_setup(this->group_, this->lib_, this->first_, this->last_, this->delay_, this->restart_, x, y);
  d->animation_enable(this->group_, true);
  this->running_ = true;
}

void AnimationWidget::on_hide(DWINDisplay *d) {
  if (!this->running_)
    return;
  d->animation_enable(this->group_, false);
  this->running_ = false;
}

// =============================================================================================
// Menu
// =============================================================================================
bool MenuWidget::on_screen_(DWINDisplay *d) const {
  return d != nullptr && d->is_ready() && d->current_page() == this->page_ && !this->hidden_;
}

void MenuWidget::layout_(DWINDisplay *d) {
  const FontMetrics fm = font_metrics(this->size_);
  this->item_h_ = this->item_h_cfg_ > 0 ? this->item_h_cfg_ : fm.h + 2 * this->pad_;
  const int w = this->width_ > 0 ? this->width_ : d->width() - std::max<int>(this->x_, 0);
  const int h = this->height_ > 0 ? this->height_ : d->height() - std::max<int>(this->y_, 0);
  const int x = this->resolve_x_(d, w), y = this->resolve_y_(d, h);
  this->ax1_ = x;
  this->ay1_ = y;
  this->ax2_ = x + w - 1;
  this->ay2_ = y + h - 1;
  this->list_y_ = y + (this->title_.empty() ? 0 : this->item_h_);
  this->visible_ = std::max(1, (this->ay2_ - this->list_y_ + 1) / this->item_h_);
  this->bounds_ = Rect::from_size(x, y, w, h);
}

void MenuWidget::draw(DWINDisplay *d) {
  this->layout_(d);
  const int n = static_cast<int>(this->items_.size());
  if (this->sel_ >= n)
    this->sel_ = std::max(0, n - 1);
  if (this->sel_ < this->top_)
    this->top_ = this->sel_;
  if (this->sel_ >= this->top_ + this->visible_)
    this->top_ = this->sel_ - this->visible_ + 1;

  d->draw_rect(1, this->bg_, this->ax1_, this->ay1_, this->ax2_, this->ay2_);
  if (!this->title_.empty()) {
    const FontMetrics fm = font_metrics(this->size_);
    const uint16_t tc = this->has_title_color_ ? this->title_color_ : this->item_color_;
    const uint16_t tbg = this->has_title_bg_ ? this->title_bg_ : this->bg_;
    d->draw_rect(1, tbg, this->ax1_, this->ay1_, this->ax2_, this->ay1_ + this->item_h_ - 1);
    const int tw = static_cast<int>(this->title_.size()) * fm.w;
    const int tx = std::max(this->ax1_, this->ax1_ + (this->ax2_ - this->ax1_ + 1 - tw) / 2);
    d->draw_text(this->size_, tc, 0, false, false, tx, this->ay1_ + (this->item_h_ - fm.h) / 2, this->title_);
    d->draw_rect(1, tc, this->ax1_, this->ay1_ + this->item_h_ - 1, this->ax2_, this->ay1_ + this->item_h_ - 1);
  }
  this->draw_items_(d);
}

void MenuWidget::draw_items_(DWINDisplay *d) {
  const int n = static_cast<int>(this->items_.size());
  // wipe the list area first so a shorter last page leaves no stale rows
  d->draw_rect(1, this->bg_, this->ax1_, this->list_y_, this->ax2_, this->ay2_);
  for (int r = 0; r < this->visible_ && this->top_ + r < n; r++)
    this->draw_row_(d, this->top_ + r);
  if (this->has_scrollbar_())
    this->draw_scrollbar_(d);
}

void MenuWidget::draw_row_(DWINDisplay *d, int idx) {
  const int row = idx - this->top_;
  if (row < 0 || row >= this->visible_ || idx >= static_cast<int>(this->items_.size()))
    return;
  const FontMetrics fm = font_metrics(this->size_);
  const int ry1 = this->list_y_ + row * this->item_h_, ry2 = ry1 + this->item_h_ - 1;
  const int rx1 = this->ax1_, rx2 = this->ax2_ - (this->has_scrollbar_() ? 5 : 0);
  const bool sel = idx == this->sel_;
  const bool hl = sel && this->sel_type_ == SelectorType::HIGHLIGHT;

  d->draw_rect(1, hl ? this->sel_color_ : this->bg_, rx1, ry1, rx2, ry2);

  if (sel && this->sel_type_ == SelectorType::BOX)
    d->draw_rect(0, this->sel_color_, rx1, ry1, rx2, ry2);  // before the text so a text command is last

  std::string t = this->items_[idx]->text();
  this->items_[idx]->shown = t;
  int tx = rx1 + this->pad_;
  if (this->sel_type_ == SelectorType::ARROW)
    tx += fm.w * 2;
  else if (this->sel_type_ == SelectorType::BOX)
    tx += 2;
  const int ty = ry1 + (this->item_h_ - fm.h) / 2;
  const size_t cells = static_cast<size_t>(std::max(0, (rx2 - tx + 1) / fm.w));
  if (t.size() > cells)
    t.resize(cells);
  const uint16_t color = hl ? (this->has_hl_text_ ? this->hl_text_ : this->bg_) : this->item_color_;
  d->draw_text(this->size_, color, 0, false, false, tx, ty, t);

  if (sel && this->sel_type_ == SelectorType::ARROW)
    d->draw_text(this->size_, this->sel_color_, 0, false, false, rx1 + this->pad_, ty, ">");
}

void MenuWidget::draw_scrollbar_(DWINDisplay *d) {
  const int n = static_cast<int>(this->items_.size());
  const int tx1 = this->ax2_ - 3, tx2 = this->ax2_;
  const int ty1 = this->list_y_, ty2 = this->list_y_ + this->visible_ * this->item_h_ - 1;
  const int th = ty2 - ty1 + 1;
  d->draw_rect(1, 0x2104, tx1, ty1, tx2, ty2);
  const int thumb = std::max(8, th * this->visible_ / n);
  const int span = n - this->visible_;
  const int off = span > 0 ? (th - thumb) * this->top_ / span : 0;
  d->draw_rect(1, this->scrollbar_color_, tx1, ty1 + off, tx2, ty1 + off + thumb - 1);
}

void MenuWidget::update(DWINDisplay *d) {
  const int n = static_cast<int>(this->items_.size());
  for (int r = 0; r < this->visible_ && this->top_ + r < n; r++) {
    MenuItem *it = this->items_[this->top_ + r];
    if (it->text() != it->shown)
      this->draw_row_(d, this->top_ + r);
  }
}

void MenuWidget::select_(DWINDisplay *d, int idx) {
  const int n = static_cast<int>(this->items_.size());
  if (n == 0)
    return;
  idx = std::max(0, std::min(idx, n - 1));
  if (idx == this->sel_)
    return;
  this->layout_(d);
  const int old = this->sel_, old_top = this->top_;
  this->sel_ = idx;
  if (this->sel_ < this->top_)
    this->top_ = this->sel_;
  else if (this->sel_ >= this->top_ + this->visible_)
    this->top_ = this->sel_ - this->visible_ + 1;
  if (this->on_screen_(d)) {
    if (this->top_ != old_top) {
      this->draw_items_(d);
    } else {
      this->draw_row_(d, old);
      this->draw_row_(d, this->sel_);
    }
  }
  this->items_[this->sel_]->fire_select();
}

bool MenuWidget::on_move(DWINDisplay *d, int delta) {
  const int n = static_cast<int>(this->items_.size());
  if (n == 0)
    return true;
  int ns = this->sel_ + delta;
  if (ns < 0)
    ns = this->wrap_ ? n - 1 : 0;
  else if (ns >= n)
    ns = this->wrap_ ? 0 : n - 1;
  this->select_(d, ns);
  return true;
}

bool MenuWidget::on_click(DWINDisplay *d) {
  if (this->items_.empty())
    return true;
  MenuItem *it = this->items_[this->sel_];
  it->fire_enter();
  if (it->back())
    d->back();
  else if (it->target() != nullptr)
    d->show_page(it->target());
  return true;
}

void MenuWidget::move(int delta) {
  DWINDisplay *d = this->display();
  if (d != nullptr)
    this->on_move(d, delta);
}

void MenuWidget::select(int index) {
  DWINDisplay *d = this->display();
  if (d != nullptr)
    this->select_(d, index);
}

void MenuWidget::activate() {
  DWINDisplay *d = this->display();
  if (d != nullptr)
    this->on_click(d);
}

}  // namespace dwin
}  // namespace esphome
