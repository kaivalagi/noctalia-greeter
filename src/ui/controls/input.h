#pragma once

#include "render/scene/input_area.h"
#include "render/scene/rect_node.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

class Label;

class Input : public Node {
public:
  Input();

  void setValue(std::string_view value);
  void setPlaceholder(std::string_view placeholder);
  void setFontSize(float size);
  void setControlHeight(float height);
  void setPasswordMode(bool enabled);
  void setInvalid(bool invalid);
  void setEnabled(bool enabled);
  void setBold(bool bold);

  void setOnChange(std::function<void(const std::string&)> callback);
  void setOnSubmit(std::function<void(const std::string&)> callback);
  void setOnFocusLoss(std::function<void()> callback);

  [[nodiscard]] const std::string& value() const noexcept { return m_value; }
  [[nodiscard]] InputArea* inputArea() noexcept { return m_inputArea; }

private:
  void doLayout(Renderer& renderer) override;
  LayoutSize doMeasure(Renderer& renderer, const LayoutConstraints& constraints) override;
  void applyVisualState();
  void handleKey(std::uint32_t sym, std::uint32_t utf32, std::uint32_t modifiers, bool preedit);

  RectNode* m_background = nullptr;
  Label* m_label = nullptr;
  InputArea* m_inputArea = nullptr;

  std::string m_value;
  std::string m_placeholder;
  float m_fontSize = 14.0f;
  float m_controlHeight = 36.0f;
  bool m_passwordMode = false;
  bool m_invalid = false;
  bool m_enabled = true;

  std::function<void(const std::string&)> m_onChange;
  std::function<void(const std::string&)> m_onSubmit;
  std::function<void()> m_onFocusLoss;
};
