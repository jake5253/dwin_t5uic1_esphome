#pragma once

#include "dwin.h"
#include "widgets.h"

namespace esphome {
namespace dwin {

enum class DisplayOp : uint8_t { SLEEP, WAKE, BACK, HOME, REDRAW, UPDATE };
enum class WidgetOp : uint8_t { SHOW, HIDE, REFRESH };
enum class MenuOp : uint8_t { NEXT, PREV, ENTER, SELECT };

/// Holds either a constant byte vector or a (stateless) lambda that produces one.
template<typename... Ts> class BytesValue {
 public:
  void set_static(const std::vector<uint8_t> &d) {
    static_ = d;
    func_ = nullptr;
  }
  template<typename F> void set_func(F f) { func_ = std::move(f); }
  std::vector<uint8_t> get(const Ts &...x) const { return func_ ? func_(x...) : static_; }

 protected:
  std::vector<uint8_t> static_;
  std::function<std::vector<uint8_t>(const Ts &...)> func_;
};

template<typename... Ts> class SimpleDisplayAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  explicit SimpleDisplayAction(DisplayOp op) : op_(op) {}
  void play(const Ts &...x) override {
    switch (this->op_) {
      case DisplayOp::SLEEP:
        this->parent_->sleep();
        break;
      case DisplayOp::WAKE:
        this->parent_->wake();
        break;
      case DisplayOp::BACK:
        this->parent_->back();
        break;
      case DisplayOp::HOME:
        this->parent_->go_home();
        break;
      case DisplayOp::REDRAW:
        this->parent_->redraw();
        break;
      case DisplayOp::UPDATE:
        this->parent_->update_lcd();
        break;
    }
  }

 protected:
  DisplayOp op_;
};

template<typename... Ts> class ShowPageAction : public Action<Ts...> {
 public:
  explicit ShowPageAction(Page *page) : page_(page) {}
  TEMPLATABLE_VALUE(bool, push)
  void play(const Ts &...x) override {
    DWINDisplay *d = this->page_->display();
    if (d != nullptr)
      d->show_page(this->page_, this->push_.value(x...));
  }

 protected:
  Page *page_;
};

template<typename... Ts> class BrightnessAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  TEMPLATABLE_VALUE(float, brightness)  // percent 0..100
  void play(const Ts &...x) override { this->parent_->set_brightness(this->brightness_.value(x...)); }
};

template<typename... Ts> class BeepAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  TEMPLATABLE_VALUE(uint32_t, duration)
  void play(const Ts &...x) override { this->parent_->beep(this->duration_.value(x...)); }
};

template<typename... Ts> class OrientationAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  TEMPLATABLE_VALUE(uint8_t, direction)
  void play(const Ts &...x) override { this->parent_->set_orientation(this->direction_.value(x...)); }
};

template<typename... Ts> class RawSendAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  void set_cmd(uint8_t c) { cmd_ = c; }
  void set_cost(uint32_t us) { cost_ = us; }
  BytesValue<Ts...> data;
  void play(const Ts &...x) override { this->parent_->send_raw(this->cmd_, this->data.get(x...), this->cost_); }

 protected:
  uint8_t cmd_{0};
  uint32_t cost_{1000};
};

template<typename... Ts> class MemoryWriteAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  void set_type(uint8_t t) { type_ = t; }
  TEMPLATABLE_VALUE(uint16_t, address)
  BytesValue<Ts...> data;
  void play(const Ts &...x) override {
    this->parent_->write_memory(this->type_, this->address_.value(x...), this->data.get(x...));
  }

 protected:
  uint8_t type_{0x5A};
};

template<typename... Ts> class MemoryReadAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  void set_type(uint8_t t) { type_ = t; }
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint8_t, length)
  void play(const Ts &...x) override {
    this->parent_->read_memory(this->type_, this->address_.value(x...), this->length_.value(x...));
  }

 protected:
  uint8_t type_{0x5A};
};

template<typename... Ts> class SramToPictureAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  TEMPLATABLE_VALUE(uint8_t, picture)
  void play(const Ts &...x) override { this->parent_->sram_to_picture(this->picture_.value(x...)); }
};

template<typename... Ts> class ExtUartSendAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  BytesValue<Ts...> data;
  void play(const Ts &...x) override { this->parent_->extuart_send(this->data.get(x...)); }
};

template<typename... Ts> class ExtUartBaudAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  TEMPLATABLE_VALUE(uint32_t, baud_rate)
  void play(const Ts &...x) override { this->parent_->extuart_set_baud(this->baud_rate_.value(x...)); }
};

template<typename... Ts> class AnimationGroupAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  explicit AnimationGroupAction(bool on) : on_(on) {}
  TEMPLATABLE_VALUE(uint8_t, group)
  void play(const Ts &...x) override { this->parent_->animation_enable(this->group_.value(x...), this->on_); }

 protected:
  bool on_;
};

template<typename... Ts> class MoveAreaAction : public Action<Ts...>, public Parented<DWINDisplay> {
 public:
  void set_mode(uint8_t m) { mode_ = m; }
  void set_color(uint16_t c) { color_ = c; }
  TEMPLATABLE_VALUE(uint16_t, distance)
  TEMPLATABLE_VALUE(int, x1)
  TEMPLATABLE_VALUE(int, y1)
  TEMPLATABLE_VALUE(int, x2)
  TEMPLATABLE_VALUE(int, y2)
  void play(const Ts &...x) override {
    this->parent_->move_area(this->mode_, this->distance_.value(x...), this->color_, this->x1_.value(x...),
                             this->y1_.value(x...), this->x2_.value(x...), this->y2_.value(x...));
  }

 protected:
  uint8_t mode_{0};
  uint16_t color_{0};
};

template<typename... Ts> class WidgetOpAction : public Action<Ts...>, public Parented<Widget> {
 public:
  explicit WidgetOpAction(WidgetOp op) : op_(op) {}
  void play(const Ts &...x) override {
    switch (this->op_) {
      case WidgetOp::SHOW:
        this->parent_->show();
        break;
      case WidgetOp::HIDE:
        this->parent_->hide();
        break;
      case WidgetOp::REFRESH:
        this->parent_->refresh();
        break;
    }
  }

 protected:
  WidgetOp op_;
};

template<typename... Ts> class StringSetAction : public Action<Ts...>, public Parented<StringWidget> {
 public:
  TEMPLATABLE_VALUE(std::string, value)
  void play(const Ts &...x) override { this->parent_->set_string(this->value_.value(x...)); }
};

template<typename... Ts> class ValueSetAction : public Action<Ts...>, public Parented<ValueWidget> {
 public:
  TEMPLATABLE_VALUE(float, value)
  void play(const Ts &...x) override { this->parent_->set_value(this->value_.value(x...)); }
};

template<typename... Ts> class MenuOpAction : public Action<Ts...>, public Parented<MenuWidget> {
 public:
  explicit MenuOpAction(MenuOp op) : op_(op) {}
  TEMPLATABLE_VALUE(int, index)
  void play(const Ts &...x) override {
    switch (this->op_) {
      case MenuOp::NEXT:
        this->parent_->move(1);
        break;
      case MenuOp::PREV:
        this->parent_->move(-1);
        break;
      case MenuOp::ENTER:
        this->parent_->activate();
        break;
      case MenuOp::SELECT:
        this->parent_->select(this->index_.value(x...));
        break;
    }
  }

 protected:
  MenuOp op_;
};

}  // namespace dwin
}  // namespace esphome
