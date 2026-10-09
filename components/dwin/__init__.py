"""ESPHome external component for the DWIN T5UIC1 serial display (Ender-3 V2 LCD)."""

import functools

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import output, sensor, text_sensor, uart, binary_sensor
from esphome.components.rotary_encoder.sensor import RotaryEncoderSensor
from esphome.components.uart import validate_raw_data
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import Lambda

CODEOWNERS = []
DEPENDENCIES = ["uart"]
MULTI_CONF = True

dwin_ns = cg.esphome_ns.namespace("dwin")
DWINDisplay = dwin_ns.class_("DWINDisplay", cg.Component, uart.UARTDevice)
Page = dwin_ns.class_("Page")
Widget = dwin_ns.class_("Widget")
StringWidget = dwin_ns.class_("StringWidget", Widget)
ValueWidget = dwin_ns.class_("ValueWidget", Widget)
RectangleWidget = dwin_ns.class_("RectangleWidget", Widget)
LineWidget = dwin_ns.class_("LineWidget", Widget)
PointsWidget = dwin_ns.class_("PointsWidget", Widget)
BitmapWidget = dwin_ns.class_("BitmapWidget", Widget)
TextWidget = dwin_ns.class_("TextWidget", StringWidget)
QRWidget = dwin_ns.class_("QRWidget", StringWidget)
BarcodeWidget = dwin_ns.class_("BarcodeWidget", StringWidget)
NumberWidget = dwin_ns.class_("NumberWidget", ValueWidget)
ProgressBarWidget = dwin_ns.class_("ProgressBarWidget", ValueWidget)
ImageWidget = dwin_ns.class_("ImageWidget", Widget)
IconWidget = dwin_ns.class_("IconWidget", Widget)
SramIconWidget = dwin_ns.class_("SramIconWidget", Widget)
AnimationWidget = dwin_ns.class_("AnimationWidget", Widget)
MenuWidget = dwin_ns.class_("MenuWidget", Widget)
MenuItem = dwin_ns.class_("MenuItem")
SelectorType = dwin_ns.enum("SelectorType", is_class=True)

PageEvent = dwin_ns.enum("PageEvent", is_class=True)
DisplayEvent = dwin_ns.enum("DisplayEvent", is_class=True)
DisplayOp = dwin_ns.enum("DisplayOp", is_class=True)
WidgetOp = dwin_ns.enum("WidgetOp", is_class=True)
MenuOp = dwin_ns.enum("MenuOp", is_class=True)

PageTrigger = dwin_ns.class_("PageTrigger", automation.Trigger.template())
DisplayTrigger = dwin_ns.class_("DisplayTrigger", automation.Trigger.template())
MemoryReadTrigger = dwin_ns.class_(
    "MemoryReadTrigger",
    automation.Trigger.template(cg.uint8, cg.uint16, cg.std_vector.template(cg.uint8)),
)
ExtUartTrigger = dwin_ns.class_(
    "ExtUartTrigger", automation.Trigger.template(cg.std_vector.template(cg.uint8))
)
MenuItemTrigger = dwin_ns.class_("MenuItemTrigger", automation.Trigger.template())

# ------------------------------------------------------------------------------------------------
# config keys
# ------------------------------------------------------------------------------------------------
CONF_ENCODER = "encoder"
CONF_ENTER = "enter"
CONF_BUZZER = "buzzer"
CONF_WIDTH = "width"
CONF_HEIGHT = "height"
CONF_DIRECTION = "direction"
CONF_HOME_PAGE = "home_page"
CONF_DEFAULT_BRIGHTNESS = "default_brightness"
CONF_SLEEP_DELAY = "sleep_delay"
CONF_SLEEP_BRIGHTNESS = "sleep_brightness"
CONF_SLEEP_ON_ANY_PAGE = "sleep_on_any_page"
CONF_RETURN_HOME_DELAY = "return_home_delay"
CONF_LONG_PRESS_TIME = "long_press_time"
CONF_LONG_PRESS_BACK = "long_press_back"
CONF_ENCODER_INVERT = "encoder_invert"
CONF_CLICK_BEEP = "click_beep"
CONF_MOVE_BEEP = "move_beep"
CONF_CPU_FREQUENCY = "cpu_frequency"
CONF_HANDSHAKE = "handshake"
CONF_AUTO_UPDATE = "auto_update"
CONF_BOOT_TIMEOUT = "boot_timeout"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_PAGES = "pages"
CONF_WIDGETS = "widgets"
CONF_BACKGROUND = "background"
CONF_BACKGROUND_IMAGE = "background_image"

CONF_X = "x"
CONF_Y = "y"
CONF_HIDDEN = "hidden"
CONF_COLOR = "color"
CONF_BG_COLOR = "bg_color"
CONF_BORDER_COLOR = "border_color"
CONF_FILL = "fill"
CONF_XOR = "xor"
CONF_POINTS = "points"
CONF_SIZE = "size"
CONF_TEXT = "text"
CONF_TEXT_SENSOR = "text_sensor"
CONF_SENSOR = "sensor"
CONF_VALUE = "value"
CONF_WIDTH_ADJUST = "width_adjust"
CONF_DIGITS = "digits"
CONF_DECIMALS = "decimals"
CONF_SIGNED = "signed"
CONF_LEADING = "leading_zeros"
CONF_PIXEL_SIZE = "pixel_size"
CONF_DATA = "data"
CONF_PICTURE = "picture"
CONF_SOURCE_X = "source_x"
CONF_SOURCE_Y = "source_y"
CONF_TRANSPARENT = "transparent"
CONF_RESTORE_BACKGROUND = "restore_background"
CONF_STRONG_FILTER = "strong_filter"
CONF_LIBRARY = "library"
CONF_ICON = "icon"
CONF_ICONS = "icons"
CONF_ICON_WIDTH = "icon_width"
CONF_ICON_HEIGHT = "icon_height"
CONF_ADDRESS = "address"
CONF_GROUP = "group"
CONF_START_ICON = "start_icon"
CONF_END_ICON = "end_icon"
CONF_DELAY = "delay"
CONF_RESTART = "restart"
CONF_MIN_VALUE = "min_value"
CONF_MAX_VALUE = "max_value"
CONF_VERTICAL = "vertical"
CONF_TITLE = "title"
CONF_TITLE_COLOR = "title_color"
CONF_TITLE_BG_COLOR = "title_bg_color"
CONF_ITEMS = "items"
CONF_ITEM_COLOR = "item_color"
CONF_ITEM_HEIGHT = "item_height"
CONF_PADDING = "padding"
CONF_SELECTOR_TYPE = "selector_type"
CONF_SELECTOR_COLOR = "selector_color"
CONF_HIGHLIGHT_TEXT_COLOR = "selected_text_color"
CONF_WRAP = "wrap"
CONF_SCROLLBAR = "scrollbar"
CONF_SCROLLBAR_COLOR = "scrollbar_color"
CONF_PAGE = "page"
CONF_BACK = "back"
CONF_ON_ENTER = "on_enter"
CONF_ON_SELECT = "on_select"

# ------------------------------------------------------------------------------------------------
# validators
# ------------------------------------------------------------------------------------------------
NAMED_COLORS = {
    "black": 0x000000,
    "white": 0xFFFFFF,
    "red": 0xFF0000,
    "green": 0x00FF00,
    "blue": 0x0000FF,
    "yellow": 0xFFFF00,
    "cyan": 0x00FFFF,
    "magenta": 0xFF00FF,
    "orange": 0xFF8000,
    "purple": 0x800080,
    "pink": 0xFF80C0,
    "brown": 0x804000,
    "navy": 0x000080,
    "teal": 0x008080,
    "maroon": 0x800000,
    "olive": 0x808000,
    "lime": 0x80FF00,
    "grey": 0x808080,
    "gray": 0x808080,
    "light_grey": 0xC0C0C0,
    "light_gray": 0xC0C0C0,
    "dark_grey": 0x404040,
    "dark_gray": 0x404040,
}


def _rgb565(rgb888):
    r, g, b = (rgb888 >> 16) & 0xFF, (rgb888 >> 8) & 0xFF, rgb888 & 0xFF
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def dwin_color(value):
    """Named colour, 0xRRGGBB integer or "#RRGGBB" string -> RGB565 integer."""
    if isinstance(value, bool):
        raise cv.Invalid("Colour must be a name, 0xRRGGBB or #RRGGBB")
    if isinstance(value, int):
        return _rgb565(cv.int_range(min=0, max=0xFFFFFF)(value))
    if isinstance(value, str):
        key = value.strip().lower()
        if key in NAMED_COLORS:
            return _rgb565(NAMED_COLORS[key])
        hexpart = key[1:] if key.startswith("#") else key
        if len(hexpart) == 6:
            try:
                return _rgb565(int(hexpart, 16))
            except ValueError:
                pass
        raise cv.Invalid(
            f"Unknown colour '{value}'. Use a name ({', '.join(sorted(NAMED_COLORS))}), 0xRRGGBB or #RRGGBB"
        )
    raise cv.Invalid("Colour must be a name, 0xRRGGBB or #RRGGBB")


COORD_CENTER = -32768
COORD_END = -32767
_COORD_WORDS = {
    "CENTER": COORD_CENTER,
    "MIDDLE": COORD_CENTER,
    "RIGHT": COORD_END,
    "BOTTOM": COORD_END,
    "END": COORD_END,
    "LEFT": 0,
    "TOP": 0,
}


def coord(value):
    """Integer pixel position, or CENTER / RIGHT / BOTTOM keywords."""
    if isinstance(value, str):
        key = value.strip().upper()
        if key in _COORD_WORDS:
            return _COORD_WORDS[key]
        raise cv.Invalid(f"Unknown position '{value}', use an integer or one of {sorted(_COORD_WORDS)}")
    return cv.int_range(min=-4096, max=8192)(value)


def gb2312_text(value):
    value = cv.string(value)
    try:
        value.encode("gb2312")
    except UnicodeEncodeError as err:
        raise cv.Invalid(f"Text contains characters not available in the GB2312 font: {err}") from err
    return value


def direction_validator(value):
    if value in (0, 90, 180, 270, "0", "90", "180", "270"):
        return int(value) // 90 if int(value) >= 90 else 0
    return cv.int_range(min=0, max=3)(value)


def _cstr(value):
    """Static text -> std::string literal containing GB2312 bytes (octal escapes keep it unambiguous)."""
    data = value.encode("gb2312")
    out = []
    for b in data:
        ch = chr(b)
        if 32 <= b < 127 and ch not in '"\\?':
            out.append(ch)
        else:
            out.append("\\%03o" % b)
    return cg.RawExpression(f'std::string("{"".join(out)}", {len(data)})')


# ------------------------------------------------------------------------------------------------
# widget schemas
# ------------------------------------------------------------------------------------------------
POS = {
    cv.Optional(CONF_X, default=0): coord,
    cv.Optional(CONF_Y, default=0): coord,
    cv.Optional(CONF_HIDDEN, default=False): cv.boolean,
}
SIZE_OPT = {
    cv.Optional(CONF_WIDTH): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_HEIGHT): cv.int_range(min=1, max=2000),
}
SIZE_REQ = {
    cv.Required(CONF_WIDTH): cv.int_range(min=1, max=2000),
    cv.Required(CONF_HEIGHT): cv.int_range(min=1, max=2000),
}
COLOR_WHITE = {cv.Optional(CONF_COLOR, default="white"): dwin_color}


def _widget(cls, *parts):
    schema = {cv.GenerateID(): cv.declare_id(cls)}
    for part in parts:
        schema.update(part)
    return cv.Schema(schema)


def point_validator(value):
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return {CONF_X: coord(value[0]), CONF_Y: coord(value[1])}
    return cv.Schema({cv.Required(CONF_X): cv.int_range(-4096, 8192), cv.Required(CONF_Y): cv.int_range(-4096, 8192)})(
        value
    )


def _one_of_text_or_sensor(value):
    return cv.has_exactly_one_key(CONF_TEXT, CONF_TEXT_SENSOR)(value)


def _number_source(value):
    return cv.has_exactly_one_key(CONF_VALUE, CONF_SENSOR)(value)


def icon_source(value):
    return cv.has_exactly_one_key(CONF_ICON, CONF_ICONS)(value)


def validate_ean(value):
    value = cv.string_strict(value) if not isinstance(value, int) else str(value)
    if not value.isdigit() or len(value) not in (12, 13):
        raise cv.Invalid("EAN-13 data must be 12 digits (the 13th check digit is computed by the display)")
    return value


def validate_delay(value):
    value = cv.positive_time_period_milliseconds(value)
    ms = value.total_milliseconds
    if ms > 2550 or ms % 10:
        raise cv.Invalid("Animation delay must be a multiple of 10 ms, at most 2550 ms")
    return value


RECTANGLE_SCHEMA = _widget(
    RectangleWidget,
    POS,
    SIZE_OPT,
    COLOR_WHITE,
    {
        cv.Optional(CONF_FILL, default=False): cv.boolean,
        cv.Optional(CONF_XOR, default=False): cv.boolean,
        cv.Optional(CONF_BORDER_COLOR): dwin_color,
    },
)

LINE_SCHEMA = _widget(
    LineWidget,
    {cv.Optional(CONF_HIDDEN, default=False): cv.boolean},
    COLOR_WHITE,
    {cv.Required(CONF_POINTS): cv.All(cv.ensure_list(point_validator), cv.Length(min=2, max=61))},
)

POINTS_SCHEMA = _widget(
    PointsWidget,
    {cv.Optional(CONF_HIDDEN, default=False): cv.boolean},
    COLOR_WHITE,
    {
        cv.Optional(CONF_SIZE, default=1): cv.int_range(min=1, max=15),
        cv.Required(CONF_POINTS): cv.ensure_list(point_validator),
    },
)

TEXT_SCHEMA = cv.All(
    _widget(
        TextWidget,
        POS,
        COLOR_WHITE,
        {
            cv.Optional(CONF_SIZE, default=3): cv.int_range(min=0, max=9),
            cv.Optional(CONF_BG_COLOR): dwin_color,
            cv.Optional(CONF_WIDTH_ADJUST, default=False): cv.boolean,
            cv.Optional(CONF_TEXT): cv.templatable(gb2312_text),
            cv.Optional(CONF_TEXT_SENSOR): cv.use_id(text_sensor.TextSensor),
        },
    ),
    _one_of_text_or_sensor,
)

NUMBER_SCHEMA = cv.All(
    _widget(
        NumberWidget,
        POS,
        COLOR_WHITE,
        {
            cv.Optional(CONF_SIZE, default=3): cv.int_range(min=0, max=11),
            cv.Optional(CONF_BG_COLOR): dwin_color,
            cv.Optional(CONF_VALUE): cv.templatable(cv.float_),
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_DIGITS, default=4): cv.int_range(min=1, max=20),
            cv.Optional(CONF_DECIMALS, default=0): cv.int_range(min=0, max=20),
            cv.Optional(CONF_SIGNED, default=True): cv.boolean,
            cv.Optional(CONF_LEADING, default="none"): cv.one_of("none", "space", "zero", lower=True),
        },
    ),
    _number_source,
)

PROGRESS_SCHEMA = cv.All(
    _widget(
        ProgressBarWidget,
        POS,
        {
            cv.Optional(CONF_WIDTH): cv.int_range(min=3, max=2000),
            cv.Optional(CONF_HEIGHT): cv.int_range(min=3, max=2000),
        },
        {
            cv.Optional(CONF_COLOR, default="green"): dwin_color,
            cv.Optional(CONF_BG_COLOR, default="dark_grey"): dwin_color,
            cv.Optional(CONF_BORDER_COLOR): dwin_color,
            cv.Optional(CONF_VERTICAL, default=False): cv.boolean,
            cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=100.0): cv.float_,
            cv.Optional(CONF_VALUE): cv.templatable(cv.float_),
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        },
    ),
    _number_source,
)

QR_SCHEMA = _widget(
    QRWidget,
    POS,
    {
        cv.Optional(CONF_PIXEL_SIZE, default=4): cv.int_range(min=1, max=15),
        cv.Required(CONF_DATA): cv.templatable(gb2312_text),
    },
)

BARCODE_SCHEMA = _widget(BarcodeWidget, POS, {cv.Required(CONF_DATA): cv.templatable(validate_ean)})

BITMAP_SCHEMA = _widget(
    BitmapWidget,
    POS,
    {cv.Required(CONF_WIDTH): cv.int_range(min=1, max=480)},
    {
        cv.Optional(CONF_COLOR, default="white"): dwin_color,
        cv.Optional(CONF_BG_COLOR, default="black"): dwin_color,
        cv.Required(CONF_DATA): cv.ensure_list(cv.hex_uint8_t),
    },
)

IMAGE_SCHEMA = _widget(
    ImageWidget,
    POS,
    SIZE_REQ,
    {
        cv.Optional(CONF_PICTURE): cv.int_range(min=0, max=15),
        cv.Optional(CONF_SOURCE_X, default=0): cv.int_range(min=0, max=1000),
        cv.Optional(CONF_SOURCE_Y, default=0): cv.int_range(min=0, max=1000),
        cv.Optional(CONF_TRANSPARENT, default=False): cv.boolean,
        cv.Optional(CONF_RESTORE_BACKGROUND, default=False): cv.boolean,
        cv.Optional(CONF_STRONG_FILTER, default=False): cv.boolean,
    },
)

ICON_SCHEMA = cv.All(
    _widget(
        IconWidget,
        POS,
        {
            cv.Required(CONF_LIBRARY): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ICON): cv.templatable(cv.uint8_t),
            cv.Optional(CONF_ICONS): cv.ensure_list(cv.uint8_t),
            cv.Optional(CONF_ICON_WIDTH, default=28): cv.int_range(min=1, max=480),
            cv.Optional(CONF_ICON_HEIGHT, default=45): cv.int_range(min=1, max=272),
            cv.Optional(CONF_TRANSPARENT, default=False): cv.boolean,
            cv.Optional(CONF_RESTORE_BACKGROUND, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_FILTER, default=False): cv.boolean,
        },
    ),
    icon_source,
)

SRAM_ICON_SCHEMA = _widget(
    SramIconWidget,
    POS,
    {
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=0x7FFF),
        cv.Optional(CONF_ICON_WIDTH, default=28): cv.int_range(min=1, max=480),
        cv.Optional(CONF_ICON_HEIGHT, default=45): cv.int_range(min=1, max=272),
        cv.Optional(CONF_TRANSPARENT, default=False): cv.boolean,
        cv.Optional(CONF_STRONG_FILTER, default=False): cv.boolean,
    },
)

ANIMATION_SCHEMA = _widget(
    AnimationWidget,
    POS,
    {
        cv.Optional(CONF_GROUP, default=0): cv.int_range(min=0, max=15),
        cv.Required(CONF_LIBRARY): cv.int_range(min=0, max=15),
        cv.Required(CONF_START_ICON): cv.uint8_t,
        cv.Required(CONF_END_ICON): cv.uint8_t,
        cv.Optional(CONF_DELAY, default="100ms"): validate_delay,
        cv.Optional(CONF_RESTART, default=True): cv.boolean,
    },
)


def _unwrap_item(value):
    if isinstance(value, dict) and set(value) == {"item"}:
        return value["item"]
    return value


MENU_ITEM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MenuItem),
        cv.Required(CONF_TEXT): cv.templatable(gb2312_text),
        cv.Optional(CONF_PAGE): cv.use_id(Page),
        cv.Optional(CONF_BACK, default=False): cv.boolean,
        cv.Optional(CONF_ON_ENTER): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MenuItemTrigger)}
        ),
        cv.Optional(CONF_ON_SELECT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MenuItemTrigger)}
        ),
    }
)

SELECTOR_TYPES = {"box": "BOX", "arrow": "ARROW", "highlight": "HIGHLIGHT"}

MENU_SCHEMA = _widget(
    MenuWidget,
    POS,
    SIZE_OPT,
    {
        cv.Optional(CONF_SIZE, default=3): cv.int_range(min=0, max=9),
        cv.Optional(CONF_PADDING, default=4): cv.int_range(min=0, max=40),
        cv.Optional(CONF_ITEM_HEIGHT): cv.int_range(min=8, max=272),
        cv.Optional(CONF_BG_COLOR, default="black"): dwin_color,
        cv.Optional(CONF_ITEM_COLOR, default="white"): dwin_color,
        cv.Optional(CONF_SELECTOR_TYPE, default="box"): cv.one_of(*SELECTOR_TYPES, lower=True),
        cv.Optional(CONF_SELECTOR_COLOR, default="red"): dwin_color,
        cv.Optional(CONF_HIGHLIGHT_TEXT_COLOR): dwin_color,
        cv.Optional(CONF_TITLE): gb2312_text,
        cv.Optional(CONF_TITLE_COLOR): dwin_color,
        cv.Optional(CONF_TITLE_BG_COLOR): dwin_color,
        cv.Optional(CONF_WRAP, default=False): cv.boolean,
        cv.Optional(CONF_SCROLLBAR, default=True): cv.boolean,
        cv.Optional(CONF_SCROLLBAR_COLOR, default="grey"): dwin_color,
        cv.Required(CONF_ITEMS): cv.All(
            cv.ensure_list(cv.All(_unwrap_item, MENU_ITEM_SCHEMA)), cv.Length(min=1)
        ),
    },
)


# ------------------------------------------------------------------------------------------------
# widget code generation
# ------------------------------------------------------------------------------------------------
async def _string_prop(var, value, static_setter, func_setter):
    if isinstance(value, Lambda):
        func = await cg.process_lambda(value, [], return_type=cg.std_string)
        cg.add(getattr(var, func_setter)(func))
    else:
        cg.add(getattr(var, static_setter)(_cstr(value)))


async def _float_prop(var, value):
    if isinstance(value, Lambda):
        func = await cg.process_lambda(value, [], return_type=cg.float_)
        cg.add(var.set_value_func(func))
    else:
        cg.add(var.set_value_func(cg.RawExpression(f"[]() -> float {{ return {float(value)}f; }}")))


async def rectangle_to_code(var, conf):
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_fill(conf[CONF_FILL]))
    cg.add(var.set_xor(conf[CONF_XOR]))
    if CONF_BORDER_COLOR in conf:
        cg.add(var.set_border_color(conf[CONF_BORDER_COLOR]))


async def line_to_code(var, conf):
    cg.add(var.set_color(conf[CONF_COLOR]))
    for p in conf[CONF_POINTS]:
        cg.add(var.add_point(p[CONF_X], p[CONF_Y]))


async def points_to_code(var, conf):
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_point_size(conf[CONF_SIZE]))
    for p in conf[CONF_POINTS]:
        cg.add(var.add_point(p[CONF_X], p[CONF_Y]))


async def bitmap_to_code(var, conf):
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_bg_color(conf[CONF_BG_COLOR]))
    cg.add(var.set_data(conf[CONF_DATA]))


async def text_to_code(var, conf):
    cg.add(var.set_size(conf[CONF_SIZE]))
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_width_adjust(conf[CONF_WIDTH_ADJUST]))
    if CONF_BG_COLOR in conf:
        cg.add(var.set_bg_color(conf[CONF_BG_COLOR]))
    if CONF_TEXT in conf:
        await _string_prop(var, conf[CONF_TEXT], "set_static_string", "set_string_func")
    else:
        ts = await cg.get_variable(conf[CONF_TEXT_SENSOR])
        cg.add(var.set_text_sensor(ts))


async def number_to_code(var, conf):
    cg.add(var.set_size(conf[CONF_SIZE]))
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_digits(conf[CONF_DIGITS], conf[CONF_DECIMALS]))
    cg.add(var.set_signed(conf[CONF_SIGNED]))
    cg.add(var.set_leading({"none": 0, "space": 1, "zero": 2}[conf[CONF_LEADING]]))
    if CONF_BG_COLOR in conf:
        cg.add(var.set_bg_color(conf[CONF_BG_COLOR]))
    if CONF_VALUE in conf:
        await _float_prop(var, conf[CONF_VALUE])
    else:
        cg.add(var.set_sensor(await cg.get_variable(conf[CONF_SENSOR])))


async def progress_to_code(var, conf):
    cg.add(var.set_color(conf[CONF_COLOR]))
    cg.add(var.set_bg_color(conf[CONF_BG_COLOR]))
    cg.add(var.set_vertical(conf[CONF_VERTICAL]))
    cg.add(var.set_range(conf[CONF_MIN_VALUE], conf[CONF_MAX_VALUE]))
    if CONF_BORDER_COLOR in conf:
        cg.add(var.set_border_color(conf[CONF_BORDER_COLOR]))
    if CONF_VALUE in conf:
        await _float_prop(var, conf[CONF_VALUE])
    else:
        cg.add(var.set_sensor(await cg.get_variable(conf[CONF_SENSOR])))


async def qr_to_code(var, conf):
    cg.add(var.set_pixel_size(conf[CONF_PIXEL_SIZE]))
    await _string_prop(var, conf[CONF_DATA], "set_static_string", "set_string_func")


async def barcode_to_code(var, conf):
    data = conf[CONF_DATA]
    if not isinstance(data, Lambda):
        data = data[:12]
        cg.add(var.set_static_string(data))
    else:
        func = await cg.process_lambda(data, [], return_type=cg.std_string)
        cg.add(var.set_string_func(func))


async def image_to_code(var, conf):
    if CONF_PICTURE in conf:
        cg.add(var.set_picture(conf[CONF_PICTURE]))
    cg.add(var.set_source(conf[CONF_SOURCE_X], conf[CONF_SOURCE_Y]))
    cg.add(var.set_transparent(conf[CONF_TRANSPARENT]))
    cg.add(var.set_restore_background(conf[CONF_RESTORE_BACKGROUND]))
    cg.add(var.set_strong_filter(conf[CONF_STRONG_FILTER]))


async def icon_to_code(var, conf):
    cg.add(var.set_library(conf[CONF_LIBRARY]))
    cg.add(var.set_icon_size(conf[CONF_ICON_WIDTH], conf[CONF_ICON_HEIGHT]))
    cg.add(var.set_transparent(conf[CONF_TRANSPARENT]))
    cg.add(var.set_restore_background(conf[CONF_RESTORE_BACKGROUND]))
    cg.add(var.set_strong_filter(conf[CONF_STRONG_FILTER]))
    if CONF_ICONS in conf:
        cg.add(var.set_icons(conf[CONF_ICONS]))
    else:
        icon = conf[CONF_ICON]
        if isinstance(icon, Lambda):
            func = await cg.process_lambda(icon, [], return_type=cg.uint8)
            cg.add(var.set_icons_func(cg.RawExpression(f"[f = {func}]() -> std::vector<uint8_t> {{ return {{f()}}; }}")))
        else:
            cg.add(var.set_icons([icon]))


async def sram_icon_to_code(var, conf):
    cg.add(var.set_address(conf[CONF_ADDRESS]))
    cg.add(var.set_icon_size(conf[CONF_ICON_WIDTH], conf[CONF_ICON_HEIGHT]))
    cg.add(var.set_transparent(conf[CONF_TRANSPARENT]))
    cg.add(var.set_strong_filter(conf[CONF_STRONG_FILTER]))


async def animation_to_code(var, conf):
    cg.add(var.set_group(conf[CONF_GROUP]))
    cg.add(var.set_library(conf[CONF_LIBRARY]))
    cg.add(var.set_icons(conf[CONF_START_ICON], conf[CONF_END_ICON]))
    cg.add(var.set_delay(conf[CONF_DELAY].total_milliseconds // 10))
    cg.add(var.set_restart(conf[CONF_RESTART]))


async def menu_to_code(var, conf):
    cg.add(var.set_size(conf[CONF_SIZE]))
    cg.add(var.set_padding(conf[CONF_PADDING]))
    if CONF_ITEM_HEIGHT in conf:
        cg.add(var.set_item_height(conf[CONF_ITEM_HEIGHT]))
    cg.add(var.set_bg_color(conf[CONF_BG_COLOR]))
    cg.add(var.set_item_color(conf[CONF_ITEM_COLOR]))
    cg.add(var.set_selector(getattr(SelectorType, SELECTOR_TYPES[conf[CONF_SELECTOR_TYPE]]), conf[CONF_SELECTOR_COLOR]))
    if CONF_HIGHLIGHT_TEXT_COLOR in conf:
        cg.add(var.set_highlight_text_color(conf[CONF_HIGHLIGHT_TEXT_COLOR]))
    if CONF_TITLE in conf:
        cg.add(var.set_title(_cstr(conf[CONF_TITLE])))
    if CONF_TITLE_COLOR in conf:
        cg.add(var.set_title_color(conf[CONF_TITLE_COLOR]))
    if CONF_TITLE_BG_COLOR in conf:
        cg.add(var.set_title_bg_color(conf[CONF_TITLE_BG_COLOR]))
    cg.add(var.set_wrap(conf[CONF_WRAP]))
    cg.add(var.set_scrollbar(conf[CONF_SCROLLBAR], conf[CONF_SCROLLBAR_COLOR]))
    for iconf in conf[CONF_ITEMS]:
        item = cg.new_Pvariable(iconf[CONF_ID])
        cg.add(var.add_item(item))
        text = iconf[CONF_TEXT]
        if isinstance(text, Lambda):
            func = await cg.process_lambda(text, [], return_type=cg.std_string)
            cg.add(item.set_text_func(func))
        else:
            cg.add(item.set_text(_cstr(text)))
        if CONF_PAGE in iconf:
            cg.add(item.set_target(await cg.get_variable(iconf[CONF_PAGE])))
        cg.add(item.set_back(iconf[CONF_BACK]))
        for key, is_select in ((CONF_ON_ENTER, False), (CONF_ON_SELECT, True)):
            for tconf in iconf.get(key, []):
                trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID], item, is_select)
                await automation.build_automation(trigger, [], tconf)


WIDGETS = {
    "rectangle": (RectangleWidget, RECTANGLE_SCHEMA, rectangle_to_code),
    "line": (LineWidget, LINE_SCHEMA, line_to_code),
    "points": (PointsWidget, POINTS_SCHEMA, points_to_code),
    "bitmap": (BitmapWidget, BITMAP_SCHEMA, bitmap_to_code),
    "text": (TextWidget, TEXT_SCHEMA, text_to_code),
    "number": (NumberWidget, NUMBER_SCHEMA, number_to_code),
    "progress_bar": (ProgressBarWidget, PROGRESS_SCHEMA, progress_to_code),
    "qr_code": (QRWidget, QR_SCHEMA, qr_to_code),
    "barcode": (BarcodeWidget, BARCODE_SCHEMA, barcode_to_code),
    "image": (ImageWidget, IMAGE_SCHEMA, image_to_code),
    "icon": (IconWidget, ICON_SCHEMA, icon_to_code),
    "sram_icon": (SramIconWidget, SRAM_ICON_SCHEMA, sram_icon_to_code),
    "animation": (AnimationWidget, ANIMATION_SCHEMA, animation_to_code),
    "menu": (MenuWidget, MENU_SCHEMA, menu_to_code),
}


def widget_entry(value):
    """One `widgets:` list entry: {<widget type>: <config or list of configs>}"""
    if not isinstance(value, dict) or len(value) != 1:
        raise cv.Invalid(
            "Each widget entry must have exactly one key naming the widget type, one of: "
            + ", ".join(WIDGETS)
        )
    ((kind, body),) = value.items()
    if kind not in WIDGETS:
        raise cv.Invalid(f"Unknown widget type '{kind}'. Valid types: {', '.join(WIDGETS)}")
    return {kind: cv.ensure_list(WIDGETS[kind][1])(body)}


# ------------------------------------------------------------------------------------------------
# pages and display
# ------------------------------------------------------------------------------------------------
PAGE_EVENTS = {
    "on_load": PageEvent.LOAD,
    "on_unload": PageEvent.UNLOAD,
    "on_clockwise": PageEvent.CLOCKWISE,
    "on_anticlockwise": PageEvent.ANTICLOCKWISE,
    "on_click": PageEvent.CLICK,
    "on_long_click": PageEvent.LONG_CLICK,
}

DISPLAY_EVENTS = {
    "on_ready": DisplayEvent.READY,
    "on_wake": DisplayEvent.WAKE,
    "on_sleep": DisplayEvent.SLEEP,
    "on_page_change": DisplayEvent.PAGE_CHANGE,
    "on_memory_written": DisplayEvent.MEMORY_WRITTEN,
}


def _auto(trigger_cls):
    return automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_cls)})


def _not_reserved(value):
    if str(value) in ("main", "setup", "loop", "app"):
        raise cv.Invalid(f"'{value}' clashes with a C++ symbol; use another page id, e.g. '{value}_page'")
    return value


PAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.All(cv.declare_id(Page), _not_reserved),
        cv.Optional(CONF_BACKGROUND, default="black"): dwin_color,
        cv.Optional(CONF_BACKGROUND_IMAGE): cv.int_range(min=0, max=15),
        cv.Optional(CONF_WIDGETS, default=[]): cv.ensure_list(widget_entry),
        **{cv.Optional(key): _auto(PageTrigger) for key in PAGE_EVENTS},
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DWINDisplay),
            cv.Optional(CONF_ENCODER): cv.use_id(RotaryEncoderSensor),
            cv.Optional(CONF_ENTER): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_BUZZER): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_WIDTH, default=480): cv.int_range(min=1, max=480),
            cv.Optional(CONF_HEIGHT, default=272): cv.int_range(min=1, max=480),
            cv.Optional(CONF_DIRECTION, default=0): direction_validator,
            cv.Optional(CONF_HOME_PAGE): cv.use_id(Page),
            cv.Optional(CONF_DEFAULT_BRIGHTNESS, default="100%"): cv.percentage,
            cv.Optional(CONF_SLEEP_BRIGHTNESS, default="0%"): cv.percentage,
            cv.Optional(CONF_SLEEP_DELAY): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SLEEP_ON_ANY_PAGE, default=False): cv.boolean,
            cv.Optional(CONF_RETURN_HOME_DELAY): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LONG_PRESS_TIME, default="600ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LONG_PRESS_BACK, default=True): cv.boolean,
            cv.Optional(CONF_ENCODER_INVERT, default=False): cv.boolean,
            cv.Optional(CONF_CLICK_BEEP): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MOVE_BEEP): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CPU_FREQUENCY, default=400): cv.one_of(250, 400, int=True),
            cv.Optional(CONF_HANDSHAKE, default=True): cv.boolean,
            cv.Optional(CONF_AUTO_UPDATE, default=True): cv.boolean,
            cv.Optional(CONF_BOOT_TIMEOUT, default="5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_UPDATE_INTERVAL, default="250ms"): cv.positive_time_period_milliseconds,
            cv.Required(CONF_PAGES): cv.All(cv.ensure_list(PAGE_SCHEMA), cv.Length(min=1)),
            **{cv.Optional(key): _auto(DisplayTrigger) for key in DISPLAY_EVENTS},
            cv.Optional("on_memory_read"): _auto(MemoryReadTrigger),
            cv.Optional("on_extuart_data"): _auto(ExtUartTrigger),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("dwin", require_tx=True)


async def _build_widget(page, kind, wconf):
    var = cg.new_Pvariable(wconf[CONF_ID])
    cg.add(page.add_widget(var))
    if CONF_X in wconf:
        cg.add(var.set_x(wconf[CONF_X]))
    if CONF_Y in wconf:
        cg.add(var.set_y(wconf[CONF_Y]))
    if CONF_WIDTH in wconf:
        cg.add(var.set_width(wconf[CONF_WIDTH]))
    if CONF_HEIGHT in wconf:
        cg.add(var.set_height(wconf[CONF_HEIGHT]))
    if wconf.get(CONF_HIDDEN):
        cg.add(var.set_hidden(True))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_panel_size(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_direction(config[CONF_DIRECTION]))
    cg.add(var.set_default_brightness(config[CONF_DEFAULT_BRIGHTNESS] * 100))
    cg.add(var.set_sleep_brightness(config[CONF_SLEEP_BRIGHTNESS] * 100))
    if CONF_SLEEP_DELAY in config:
        cg.add(var.set_sleep_delay(config[CONF_SLEEP_DELAY].total_milliseconds))
    cg.add(var.set_sleep_on_any_page(config[CONF_SLEEP_ON_ANY_PAGE]))
    if CONF_RETURN_HOME_DELAY in config:
        cg.add(var.set_return_home_delay(config[CONF_RETURN_HOME_DELAY].total_milliseconds))
    cg.add(var.set_long_press_time(config[CONF_LONG_PRESS_TIME].total_milliseconds))
    cg.add(var.set_long_press_back(config[CONF_LONG_PRESS_BACK]))
    cg.add(var.set_encoder_invert(config[CONF_ENCODER_INVERT]))
    if CONF_CLICK_BEEP in config:
        cg.add(var.set_click_beep(config[CONF_CLICK_BEEP].total_milliseconds))
    if CONF_MOVE_BEEP in config:
        cg.add(var.set_move_beep(config[CONF_MOVE_BEEP].total_milliseconds))
    cg.add(var.set_clock_mhz(config[CONF_CPU_FREQUENCY]))
    cg.add(var.set_handshake(config[CONF_HANDSHAKE]))
    cg.add(var.set_auto_update(config[CONF_AUTO_UPDATE]))
    cg.add(var.set_boot_timeout(config[CONF_BOOT_TIMEOUT].total_milliseconds))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL].total_milliseconds))

    if CONF_ENCODER in config:
        cg.add_define("DWIN_USE_ENCODER")
        cg.add(var.set_encoder(await cg.get_variable(config[CONF_ENCODER])))
    if CONF_ENTER in config:
        cg.add_define("DWIN_USE_ENTER")
        cg.add(var.set_enter(await cg.get_variable(config[CONF_ENTER])))
    if CONF_BUZZER in config:
        cg.add_define("DWIN_USE_BUZZER")
        cg.add(var.set_buzzer(await cg.get_variable(config[CONF_BUZZER])))

    # Phase 1: create every page object first so widgets/menus/actions can reference each other.
    pages = []
    for pconf in config[CONF_PAGES]:
        page = cg.new_Pvariable(pconf[CONF_ID])
        cg.add(var.add_page(page))
        pages.append((page, pconf))

    if CONF_HOME_PAGE in config:
        cg.add(var.set_home_page(await cg.get_variable(config[CONF_HOME_PAGE])))

    # Phase 2: create every widget object (automations may reference widgets of any page).
    pending = []
    for page, pconf in pages:
        cg.add(page.set_background(pconf[CONF_BACKGROUND]))
        if CONF_BACKGROUND_IMAGE in pconf:
            cg.add(page.set_background_image(pconf[CONF_BACKGROUND_IMAGE]))
        for entry in pconf[CONF_WIDGETS]:
            ((kind, wconfs),) = entry.items()
            for wconf in wconfs:
                var_w = await _build_widget(page, kind, wconf)
                pending.append((var_w, kind, wconf))

    # Phase 3: widget contents (menu items, lambdas, ...) and automations.
    for var_w, kind, wconf in pending:
        await WIDGETS[kind][2](var_w, wconf)
    for page, pconf in pages:
        for key, event in PAGE_EVENTS.items():
            for tconf in pconf.get(key, []):
                if key == "on_long_click":
                    cg.add(page.set_handles_long_click(True))
                trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID], page, event)
                await automation.build_automation(trigger, [], tconf)

    for key, event in DISPLAY_EVENTS.items():
        for tconf in config.get(key, []):
            trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID], var, event)
            await automation.build_automation(trigger, [], tconf)
    for tconf in config.get("on_memory_read", []):
        trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.uint8, "type"), (cg.uint16, "address"), (cg.std_vector.template(cg.uint8), "data")],
            tconf,
        )
    for tconf in config.get("on_extuart_data", []):
        trigger = cg.new_Pvariable(tconf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_vector.template(cg.uint8), "x")], tconf)


# ------------------------------------------------------------------------------------------------
# actions
# ------------------------------------------------------------------------------------------------
register_action = functools.partial(automation.register_action, synchronous=True)

def _display_id(extra=None, simple=True):
    schema = {cv.GenerateID(): cv.use_id(DWINDisplay)}
    if extra:
        schema.update(extra)
    return automation.maybe_simple_id(schema) if simple else cv.Schema(schema)


SimpleDisplayAction = dwin_ns.class_("SimpleDisplayAction", automation.Action)


def _register_simple_display(name, op):
    @register_action(name, SimpleDisplayAction, _display_id())
    async def _to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg, getattr(DisplayOp, op))
        await cg.register_parented(var, config[CONF_ID])
        return var


for _name, _op in (
    ("dwin.sleep", "SLEEP"),
    ("dwin.wake", "WAKE"),
    ("dwin.page.back", "BACK"),
    ("dwin.page.home", "HOME"),
    ("dwin.page.redraw", "REDRAW"),
    ("dwin.update", "UPDATE"),
):
    _register_simple_display(_name, _op)


ShowPageAction = dwin_ns.class_("ShowPageAction", automation.Action)


@register_action(
    "dwin.page.show",
    ShowPageAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Page),
            cv.Optional("push", default=True): cv.templatable(cv.boolean),
        }
    ),
)
async def page_show_to_code(config, action_id, template_arg, args):
    page = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, page)
    cg.add(var.set_push(await cg.templatable(config["push"], args, bool)))
    return var


BrightnessAction = dwin_ns.class_("BrightnessAction", automation.Action)


@register_action(
    "dwin.brightness",
    BrightnessAction,
    _display_id({cv.Required("brightness"): cv.templatable(cv.float_range(min=0, max=100))}, simple=False),
)
async def brightness_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_brightness(await cg.templatable(config["brightness"], args, float)))
    return var


BeepAction = dwin_ns.class_("BeepAction", automation.Action)


@register_action(
    "dwin.beep",
    BeepAction,
    _display_id({cv.Optional("duration", default="100ms"): cv.templatable(cv.positive_time_period_milliseconds)}, simple=False),
)
async def beep_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_duration(await cg.templatable(config["duration"], args, cg.uint32)))
    return var


OrientationAction = dwin_ns.class_("OrientationAction", automation.Action)


@register_action(
    "dwin.orientation",
    OrientationAction,
    _display_id({cv.Required(CONF_DIRECTION): direction_validator}, simple=False),
)
async def orientation_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_direction(config[CONF_DIRECTION]))
    return var


async def _bytes_prop(var, data, args):
    if isinstance(data, Lambda):
        func = await cg.process_lambda(data, args, return_type=cg.std_vector.template(cg.uint8))
        cg.add(var.data.set_func(func))
    else:
        cg.add(var.data.set_static(list(data)))


BYTES = cv.templatable(validate_raw_data)

RawSendAction = dwin_ns.class_("RawSendAction", automation.Action)


@register_action(
    "dwin.raw_send",
    RawSendAction,
    _display_id(
        {
            cv.Required("command"): cv.hex_uint8_t,
            cv.Optional(CONF_DATA, default=[]): BYTES,
            cv.Optional("processing_time", default="1ms"): cv.positive_time_period_microseconds,
        },
        simple=False,
    ),
)
async def raw_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_cmd(config["command"]))
    cg.add(var.set_cost(config["processing_time"].total_microseconds))
    await _bytes_prop(var, config[CONF_DATA], args)
    return var


MEMORY_TYPES = {"sram": 0x5A, "flash": 0xA5}

MemoryWriteAction = dwin_ns.class_("MemoryWriteAction", automation.Action)


@register_action(
    "dwin.memory.write",
    MemoryWriteAction,
    _display_id(
        {
            cv.Optional("type", default="sram"): cv.enum(MEMORY_TYPES, lower=True),
            cv.Optional(CONF_ADDRESS, default=0): cv.templatable(cv.int_range(min=0, max=0x7FFF)),
            cv.Required(CONF_DATA): BYTES,
        },
        simple=False,
    ),
)
async def memory_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_type(config["type"].enum_value))
    cg.add(var.set_address(await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)))
    await _bytes_prop(var, config[CONF_DATA], args)
    return var


MemoryReadAction = dwin_ns.class_("MemoryReadAction", automation.Action)


@register_action(
    "dwin.memory.read",
    MemoryReadAction,
    _display_id(
        {
            cv.Optional("type", default="sram"): cv.enum(MEMORY_TYPES, lower=True),
            cv.Optional(CONF_ADDRESS, default=0): cv.templatable(cv.int_range(min=0, max=0x7FFF)),
            cv.Required("length"): cv.templatable(cv.int_range(min=1, max=240)),
        },
        simple=False,
    ),
)
async def memory_read_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_type(config["type"].enum_value))
    cg.add(var.set_address(await cg.templatable(config[CONF_ADDRESS], args, cg.uint16)))
    cg.add(var.set_length(await cg.templatable(config["length"], args, cg.uint8)))
    return var


SramToPictureAction = dwin_ns.class_("SramToPictureAction", automation.Action)


@register_action(
    "dwin.sram_to_picture",
    SramToPictureAction,
    _display_id({cv.Required(CONF_PICTURE): cv.templatable(cv.int_range(min=0, max=15))}, simple=False),
)
async def sram_to_picture_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_picture(await cg.templatable(config[CONF_PICTURE], args, cg.uint8)))
    return var


ExtUartSendAction = dwin_ns.class_("ExtUartSendAction", automation.Action)


@register_action(
    "dwin.extuart.send",
    ExtUartSendAction,
    _display_id({cv.Required(CONF_DATA): BYTES}, simple=False),
)
async def extuart_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    await _bytes_prop(var, config[CONF_DATA], args)
    return var


ExtUartBaudAction = dwin_ns.class_("ExtUartBaudAction", automation.Action)


@register_action(
    "dwin.extuart.baud_rate",
    ExtUartBaudAction,
    _display_id({cv.Required("baud_rate"): cv.templatable(cv.int_range(min=15300, max=15667200))}, simple=False),
)
async def extuart_baud_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_baud_rate(await cg.templatable(config["baud_rate"], args, cg.uint32)))
    return var


AnimationGroupAction = dwin_ns.class_("AnimationGroupAction", automation.Action)


def _register_animation_group(name, on):
    @register_action(
        name,
        AnimationGroupAction,
        _display_id({cv.Required(CONF_GROUP): cv.templatable(cv.int_range(min=0, max=15))}, simple=False),
    )
    async def _to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg, on)
        await cg.register_parented(var, config[CONF_ID])
        cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.uint8)))
        return var


_register_animation_group("dwin.animation.start", True)
_register_animation_group("dwin.animation.stop", False)


MoveAreaAction = dwin_ns.class_("MoveAreaAction", automation.Action)
MOVE_DIRECTIONS = {"left": 0, "right": 1, "up": 2, "down": 3}


@register_action(
    "dwin.move_area",
    MoveAreaAction,
    _display_id(
        {
            cv.Required("direction"): cv.enum(MOVE_DIRECTIONS, lower=True),
            cv.Required("distance"): cv.templatable(cv.int_range(min=0, max=240)),
            cv.Optional("ring", default=False): cv.boolean,
            cv.Optional("fill_color", default="black"): dwin_color,
            cv.Required("x1"): cv.templatable(cv.int_range(min=0, max=479)),
            cv.Required("y1"): cv.templatable(cv.int_range(min=0, max=479)),
            cv.Required("x2"): cv.templatable(cv.int_range(min=0, max=479)),
            cv.Required("y2"): cv.templatable(cv.int_range(min=0, max=479)),
        },
        simple=False,
    ),
)
async def move_area_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    mode = config["direction"].enum_value | (0 if config["ring"] else 0x80)
    cg.add(var.set_mode(mode))
    cg.add(var.set_color(config["fill_color"]))
    cg.add(var.set_distance(await cg.templatable(config["distance"], args, cg.uint16)))
    for key in ("x1", "y1", "x2", "y2"):
        cg.add(getattr(var, f"set_{key}")(await cg.templatable(config[key], args, int)))
    return var


WidgetOpAction = dwin_ns.class_("WidgetOpAction", automation.Action)


def _register_widget_op(name, op):
    @register_action(
        name,
        WidgetOpAction,
        automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(Widget)}),
    )
    async def _to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg, getattr(WidgetOp, op))
        await cg.register_parented(var, config[CONF_ID])
        return var


_register_widget_op("dwin.widget.show", "SHOW")
_register_widget_op("dwin.widget.hide", "HIDE")
_register_widget_op("dwin.widget.refresh", "REFRESH")


StringSetAction = dwin_ns.class_("StringSetAction", automation.Action)


@register_action(
    "dwin.text.set",
    StringSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(StringWidget),
            cv.Required(CONF_TEXT): cv.templatable(gb2312_text),
        }
    ),
)
async def text_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    text = config[CONF_TEXT]
    if isinstance(text, Lambda):
        cg.add(var.set_value(await cg.templatable(text, args, cg.std_string)))
    else:
        cg.add(var.set_value(_cstr(text)))
    return var


ValueSetAction = dwin_ns.class_("ValueSetAction", automation.Action)


@register_action(
    "dwin.value.set",
    ValueSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ValueWidget),
            cv.Required(CONF_VALUE): cv.templatable(cv.float_),
        }
    ),
)
async def value_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, float)))
    return var


MenuOpAction = dwin_ns.class_("MenuOpAction", automation.Action)


def _register_menu_op(name, op, extra=None, simple=True):
    schema = {cv.Required(CONF_ID): cv.use_id(MenuWidget)}
    if extra:
        schema.update(extra)

    @register_action(
        name, MenuOpAction, automation.maybe_simple_id(schema) if simple else cv.Schema(schema)
    )
    async def _to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg, getattr(MenuOp, op))
        await cg.register_parented(var, config[CONF_ID])
        if "index" in config:
            cg.add(var.set_index(await cg.templatable(config["index"], args, int)))
        return var


_register_menu_op("dwin.menu.next", "NEXT")
_register_menu_op("dwin.menu.previous", "PREV")
_register_menu_op("dwin.menu.enter", "ENTER")
_register_menu_op(
    "dwin.menu.select",
    "SELECT",
    {cv.Required("index"): cv.templatable(cv.int_range(min=0, max=255))},
    simple=False,
)
