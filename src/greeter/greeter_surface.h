#pragma once

#include "greetd/greetd_client.h"
#include "render/animation/animation_manager.h"
#include "render/core/texture_handle.h"
#include "render/scene/input_dispatcher.h"
#include "render/scene/node.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Box;
class Button;
class Glyph;
class GreeterWindow;
class Input;
class InputArea;
class Label;
class RenderContext;
class ImageNode;
class RectNode;

class GreeterSurface {
public:
  GreeterSurface();
  ~GreeterSurface();

  void initialize(GreeterWindow& window, RenderContext* context);

  void setGreetdClient(GreetdClient* client);
  void setUsername(const std::string& username);
  void setOnExitRequested(std::function<void()> callback);

  void onPointerEvent(float x, float y, std::uint32_t button, bool pressed);
  void onPointerMotion(float x, float y);
  void onKeyEvent(std::uint32_t sym, std::uint32_t utf32, std::uint32_t modifiers, bool pressed, bool preedit);

  void onThemeChanged();
  void requestLayout();
  void requestRedraw();
  void flushDeferredFrameRequests();

  void prepareFrame(bool needsUpdate, bool needsLayout);

  [[nodiscard]] Node* sceneRoot() noexcept { return &m_root; }

private:
  void layoutScene(std::uint32_t width, std::uint32_t height);
  void tryAuthenticate();
  void onAuthMessage(const std::optional<GreetdAuthMessage>& msg);
  void onAuthSuccess();
  void onAuthError(const std::string& error);
  void updateStatus(const std::string& text, bool isError);
  void toggleUserMenu();
  void toggleSessionMenu();
  void closeMenus();
  void rebuildUserMenu();
  void rebuildSessionMenu();
  void rebuildSchemeMenu();
  void clearUserMenu();
  void clearSessionMenu();
  void clearSchemeMenu();
  void enterPasswordStep(std::size_t userIndex);

  Node m_root;
  AnimationManager m_animations;
  GreeterWindow* m_window = nullptr;
  RenderContext* m_renderContext = nullptr;
  GreetdClient* m_greetdClient = nullptr;
  InputDispatcher m_inputDispatcher;

  Node* m_backdrop = nullptr;
  Node* m_brandPane = nullptr;
  ImageNode* m_brandLogo = nullptr;
  Label* m_brandTitleLabel = nullptr;
  Label* m_brandSubtitleLabel = nullptr;
  Label* m_formSubtitleLabel = nullptr;
  Node* m_panelDivider = nullptr;
  Label* m_titleLabel = nullptr;
  Box* m_loginPanel = nullptr;
  Box* m_userSelectBox = nullptr;
  Label* m_userSelectLabel = nullptr;
  Glyph* m_userSelectGlyph = nullptr;
  InputArea* m_userSelectArea = nullptr;
  Box* m_sessionSelectBox = nullptr;
  Label* m_sessionSelectLabel = nullptr;
  Glyph* m_sessionSelectGlyph = nullptr;
  InputArea* m_sessionSelectArea = nullptr;
  Box* m_schemeSelectBox = nullptr;
  Label* m_schemeSelectLabel = nullptr;
  Glyph* m_schemeSelectGlyph = nullptr;
  InputArea* m_schemeSelectArea = nullptr;
  Input* m_passwordField = nullptr;
  Button* m_loginButton = nullptr;
  Button* m_backButton = nullptr;
  Label* m_statusLabel = nullptr;
  std::vector<Button*> m_userRowButtons;
  std::vector<Glyph*> m_userRowArrows;

  std::string m_username;
  std::string m_password;
  std::string m_status;
  bool m_statusIsError = false;
  bool m_authenticating = false;
  bool m_authSessionStarted = false;
  std::function<void()> m_onExitRequested;
  TextureHandle m_brandLogoTexture;
  std::chrono::steady_clock::time_point m_lastAnimTick{};
  bool m_animTickInitialized = false;
  bool m_inInputDispatch = false;
  bool m_deferredLayoutRequest = false;
  bool m_deferredRedrawRequest = false;

  struct SessionOption {
    std::string name;
    std::string command;
  };

  std::vector<std::string> m_users;
  std::vector<SessionOption> m_sessions;
  std::size_t m_selectedUser = 0;
  std::size_t m_selectedSession = 0;
  bool m_userMenuOpen = false;
  bool m_sessionMenuOpen = false;
  bool m_schemeMenuOpen = false;
  bool m_passwordVisible = false;
  Box* m_userMenuPanel = nullptr;
  Box* m_sessionMenuPanel = nullptr;
  std::vector<Label*> m_userMenuLabels;
  std::vector<InputArea*> m_userMenuAreas;
  std::vector<Box*> m_userMenuRows;
  std::vector<Label*> m_sessionMenuLabels;
  std::vector<InputArea*> m_sessionMenuAreas;
  std::vector<Box*> m_sessionMenuRows;
  Box* m_schemeMenuPanel = nullptr;
  std::vector<Label*> m_schemeMenuLabels;
  std::vector<InputArea*> m_schemeMenuAreas;
  std::vector<Box*> m_schemeMenuRows;
  std::vector<std::string> m_schemeNames;
  std::size_t m_selectedScheme = 0;

  void loadUsers();
  void loadSessions();
  void refreshSelectionLabels();
};
