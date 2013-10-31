// The Environment class

#ifndef FRAME_ENVIRONMENT
#define FRAME_ENVIRONMENT

#include "frame/configuration.h"
#include "frame/input.h"
#include "frame/noncopyable.h"
#include "frame/point.h"
#include "frame/utility.h"

#include <deque>
#include <vector>
#include <set>
#include <map>

struct lua_State;

namespace Frame {
  class Frame;
  class Layout;
  class Renderer;
  class TextManager;
  class TextureManager;
  class EventTypeBase;

  class Environment : Noncopyable {
  public:
    Environment();  // init to default
    Environment(const Configuration &config);
    ~Environment();

    // ==== Update to the state of the world, incoming events
    // Bool return values indicate that the event was consumed, if applicable
    bool Input(const InputEvent &ie); // Usable as a single-package class, intended for the OS helper functions or for people who want a single data pathway
    
    // Per-piece update functions
    void MouseMove(int x, int y);
    bool MouseDown(int button);
    bool MouseUp(int button);
    bool MouseWheel(int delta);

    void MouseClear();  // mouse no longer in the scene at all

    bool KeyDown(const KeyEvent &key);
    bool KeyType(const std::string &type);
    bool KeyRepeat(const KeyEvent &key);
    bool KeyUp(const KeyEvent &key);

    // "Hey, the resolution has been changed/the window has been resized"
    void ResizeRoot(int x, int y);  
    
    // ==== State as of the current event that's being handled
    const Point &GetMouse() const { return m_mouse; }
    bool IsShift() const;
    bool IsCtrl() const;
    bool IsAlt() const;
    
    // ==== Focus
    Layout *GetFocus() { return m_focus; }
    const Layout *GetFocus() const { return m_focus; }
    void SetFocus(Layout *layout);
    void ClearFocus();

    // ==== Rendering
    void Render(const Layout *root = 0);
    
    // ==== Introspection
    Layout *GetRoot() { return m_root; }
    Layout *GetFrameUnder(float x, float y);

    const Configuration &GetConfiguration() { return m_config; }

    // ==== Scripting languages
    // Lua
    void LuaRegister(lua_State *L, bool hasErrorHandle = false); // if error handle is true, top element of the stack will be popped
    template<typename T> void LuaRegisterFrame(lua_State *L); // needed only if you create third-party frames - Frame will automatically register its internal frame types
    void LuaRegisterEvent(lua_State *L, EventTypeBase *feb);
    void LuaUnregister(lua_State *L);

    // ==== Internal only, do not call below this line (TODO make more private)
    TextManager *GetTextManager() { return m_text_manager; }
    TextureManager *GetTextureManager() { return m_texture_manager; }

    void LogError(const std::string &log) { if (m_config.logger) m_config.logger->LogError(log); }
    void LogDebug(const std::string &log) { if (m_config.logger) m_config.logger->LogDebug(log); }

    class Performance {
    public:
      Performance(Environment *env, float r, float g, float b);
      ~Performance();

    private:
      Environment *m_env;
      void *m_handle;
    };
  private:
    friend class Layout;
    friend class Frame;

    // Utility functions and parameters
    void Init(const Configuration &config);
    template<typename T> void LuaRegisterFrameLookup(lua_State *L);

    void MarkInvalidated(Layout *layout);
    void UnmarkInvalidated(Layout *layout); // This is currently very slow.
    std::deque<Layout *> m_invalidated;

    // Layout sanity
    void LayoutStack_Push(const Layout *layout, Axis axis, float pt);
    void LayoutStack_Push(const Layout *layout, Axis axis);
    void LayoutStack_Pop();
    void LayoutStack_Error();
    struct LayoutStack_Entry {
      const Layout *layout;
      Axis axis;
      float point;
    };
    std::vector<LayoutStack_Entry> m_layoutStack;
    
    // Maintenance
    void DestroyingLayout(Layout *layout);

    // Configuration
    Configuration m_config;
    Configuration::Logger *m_config_logger_owned;
    Configuration::TextureFromId *m_config_tfi_owned;
    Configuration::StreamFromId *m_config_sfi_owned;
    Configuration::PathFromId *m_config_pfi_owned;
    Configuration::TextureFromStream *m_config_tfs_owned;
    Configuration::Clipboard *m_config_clipboard_owned;

    // Managers
    Renderer *m_renderer;
    TextManager *m_text_manager;
    TextureManager *m_texture_manager;

    // Root
    Layout *m_root;
    
    Layout *m_over;
    Layout *m_focus;
    std::map<int, Layout *> m_buttonDown;
    Point m_mouse;
    KeyEvent m_lastEvent; // stores the shift/ctrl/alt states

    // Lua
    class LuaStackChecker {
    public:
      LuaStackChecker(lua_State *L, Environment *env, int offset = 0);
      ~LuaStackChecker();

    private:
      int m_depth;
      lua_State *m_L;
      Environment *m_env;
    };
    std::set<lua_State *> m_lua_environments;
    void Lua_PushErrorHandler(lua_State *L);
    static int luaF_errorDefault(lua_State *L);
  };
}

#endif