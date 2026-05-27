#include "ui/controls/input.h"

#include "core/log.h"
#include "render/scene/input_area.h"
#include "ui/controls/label.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <algorithm>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace {
  constexpr Logger kLog("input");
}

Input::Input() : Node(NodeType::Container) {
  auto bg = std::make_unique<RectNode>();
  m_background = static_cast<RectNode*>(addChild(std::move(bg)));

  auto label = std::make_unique<Label>();
  label->setFontSize(m_fontSize);
  label->setMaxLines(1);
  label->setColor(colorForRole(ColorRole::OnSurface));
  m_label = static_cast<Label*>(addChild(std::move(label)));

  auto area = std::make_unique<InputArea>();
  area->setFocusable(true);
  area->setOnEnter([this](const InputArea::PointerData&) { applyVisualState(); });
  area->setOnLeave([this]() { applyVisualState(); });
  area->setOnPress([this](const InputArea::PointerData&) { applyVisualState(); });
  area->setOnKeyDown([this](const InputArea::KeyData& k) { handleKey(k.sym, k.utf32, k.modifiers, k.preedit); });
  m_inputArea = static_cast<InputArea*>(addChild(std::move(area)));

  applyVisualState();
}

void Input::setValue(std::string_view value) {
  m_value = std::string(value);
  markLayoutDirty();
}

void Input::setPlaceholder(std::string_view placeholder) { m_placeholder = std::string(placeholder); }
void Input::setFontSize(float size) {
  m_fontSize = std::max(1.0f, size);
  if (m_label) m_label->setFontSize(m_fontSize);
  markLayoutDirty();
}
void Input::setControlHeight(float height) { m_controlHeight = std::max(1.0f, height); }
void Input::setPasswordMode(bool enabled) { m_passwordMode = enabled; }
void Input::setInvalid(bool invalid) { m_invalid = invalid; applyVisualState(); }
void Input::setEnabled(bool enabled) {
  m_enabled = enabled;
  if (m_inputArea) m_inputArea->setEnabled(enabled);
  setOpacity(enabled ? 1.0f : 0.55f);
}
void Input::setBold(bool) {}

void Input::setOnChange(std::function<void(const std::string&)> callback) { m_onChange = std::move(callback); }
void Input::setOnSubmit(std::function<void(const std::string&)> callback) { m_onSubmit = std::move(callback); }
void Input::setOnFocusLoss(std::function<void()> callback) { m_onFocusLoss = std::move(callback); }

void Input::handleKey(std::uint32_t sym, std::uint32_t utf32, std::uint32_t modifiers, bool preedit) {
  if (preedit) return;

  bool ctrl = (modifiers & 0x2) != 0;

  if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
    if (m_onSubmit) m_onSubmit(m_value);
    return;
  }

  if (sym == XKB_KEY_BackSpace) {
    if (!m_value.empty()) {
      m_value.pop_back();
      markLayoutDirty();
      if (m_onChange) m_onChange(m_value);
    }
    return;
  }

  if (sym == XKB_KEY_Escape) {
    return;
  }

  // Ctrl+C / Ctrl+V handling
  if (ctrl && (sym == 'v' || sym == 'V')) {
    // TODO: clipboard paste
    return;
  }

  if (utf32 >= 0x20 && utf32 != 0x7F) {
    m_value += static_cast<char>(utf32);
    markLayoutDirty();
    if (m_onChange) m_onChange(m_value);
  }
}

void Input::doLayout(Renderer& renderer) {
  float w = width() > 0 ? width() : 200.0f;
  float h = m_controlHeight;
  setSize(w, h);

  if (m_background) {
    m_background->setPosition(0, 0);
    m_background->setSize(w, h);
  }

  if (m_label) {
    if (m_value.empty() && !m_placeholder.empty()) {
      m_label->setText(m_placeholder);
    } else if (m_passwordMode) {
      std::string dots;
      dots.reserve(m_value.size() * 3);
      for (std::size_t i = 0; i < m_value.size(); ++i) {
        dots += "\xE2\x80\xA2";
      }
      m_label->setText(dots);
    } else {
      m_label->setText(m_value);
    }
    m_label->setFontSize(m_fontSize);
    m_label->setMaxLines(1);
    m_label->setMaxWidth(std::max(0.0f, w - Style::spaceMd * 2.0f));
    m_label->measure(renderer);
    float textY = std::round((h - m_label->height()) * 0.5f);
    m_label->setPosition(Style::spaceMd, textY);
  }

  if (m_inputArea) {
    m_inputArea->setPosition(0, 0);
    m_inputArea->setSize(w, h);
  }

  applyVisualState();
}

LayoutSize Input::doMeasure(Renderer& renderer, const LayoutConstraints& constraints) {
  doLayout(renderer);
  return constraints.constrain({width(), height()});
}

void Input::applyVisualState() {
  if (!m_background) return;

  bool focused = m_inputArea && m_inputArea->focused();
  bool hovered = m_inputArea && m_inputArea->hovered();
  Color fill = focused ? colorForRole(ColorRole::Surface) : colorForRole(ColorRole::SurfaceVariant);
  Color border = m_invalid ? colorForRole(ColorRole::Error)
              : focused ? colorForRole(ColorRole::Primary)
                        : hovered ? colorForRole(ColorRole::Hover) : colorForRole(ColorRole::Outline);

  m_background->setStyle(RoundedRectStyle{
      .fill = fill,
      .border = border,
      .fillMode = FillMode::Solid,
      .radius = Style::scaledRadiusMd(),
      .softness = 1.0f,
      .borderWidth = Style::borderWidth,
  });

  if (m_label) {
    if (m_value.empty() && !m_placeholder.empty()) {
      m_label->setColor(colorForRole(ColorRole::OnSurfaceVariant, 0.68f));
    } else {
      m_label->setColor(colorForRole(ColorRole::OnSurface));
    }
  }
}
