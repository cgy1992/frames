
#include "frames/environment.h"
#include "frames/environment_register.h"

#include "frames/frame.h"
#include "frames/renderer.h"
#include "frames/text_manager.h"
#include "frames/texture_manager.h"

// these exist just for the lua init
#include "frames/text.h"
#include "frames/texture.h"
#include "frames/mask.h"
#include "frames/raw.h"

#include <GL/gl.h>

namespace Frames {
  Environment::Environment() {
    Configuration config;
    Init(config);
  }

  Environment::Environment(const Configuration &config) {
    Init(config);
  }

  Environment::~Environment() {
    // Unwrap all our Lua hooks, while we're still stable
    while (!m_lua_environments.empty()) {
      LuaUnregister(*m_lua_environments.begin());
    }

    m_root->Obliterate();

    // this flushes everything out of memory
    while (!m_invalidated.empty()) {
      Layout *layout = m_invalidated.front();
      m_invalidated.pop_front();

      layout->Resolve();
    }

    delete m_text_manager;
    delete m_renderer;
    delete m_texture_manager;
    delete m_config_logger_owned;
    delete m_config_tfi_owned;
    delete m_config_sfi_owned;
    delete m_config_pfi_owned;
    delete m_config_tfs_owned;
    delete m_config_clipboard_owned;
  }

  void Environment::ResizeRoot(int x, int y) {
    m_root->SetWidth((float)x);
    m_root->SetHeight((float)y);
  }

  bool Environment::Input(const InputEvent &ie) {
    if (ie.GetMetaKnown()) {
      m_lastEvent.shift = ie.GetMetaShift();
      m_lastEvent.ctrl = ie.GetMetaCtrl();
      m_lastEvent.alt = ie.GetMetaAlt();
    }
    
    if (ie.GetMouseposKnown()) {
      if (ie.GetMouseposValid()) {
        MouseMove(ie.GetMouseposX(), ie.GetMouseposY());
      } else {
        LogError("No support for invalid mousepos yet");
      }
    }
    
    bool consumed = true;
    
    if (ie.GetMode() == InputEvent::MODE_KEYDOWN) {
      m_lastEvent.key = ie.GetKey();
      consumed = KeyDown(m_lastEvent);
    } else if (ie.GetMode() == InputEvent::MODE_KEYUP) {
      m_lastEvent.key = ie.GetKey();
      consumed = KeyUp(m_lastEvent);
    } else if (ie.GetMode() == InputEvent::MODE_KEYREPEAT) {
      m_lastEvent.key = ie.GetKey();
      consumed = KeyRepeat(m_lastEvent);
    } else if (ie.GetMode() == InputEvent::MODE_MOUSEDOWN) {
      consumed = MouseDown(ie.GetMouseButton());
    } else if (ie.GetMode() == InputEvent::MODE_MOUSEUP) {
      consumed = MouseUp(ie.GetMouseButton());
    } else if (ie.GetMode() == InputEvent::MODE_MOUSEWHEEL) {
      LogError("No support for mousewheel yet");
    } else if (ie.GetMode() == InputEvent::MODE_TYPE) {
      consumed = KeyType(ie.GetType());
    }
    
    return consumed;
  }
  
  void Environment::MouseMove(int ix, int iy) {
    // convert to internal floatingpoint
    float x = (float)ix;
    float y = (float)iy;

    Layout *updated = GetFrameUnder(x, y);
    m_mouse = Point(x, y);

    bool mouseover = false;
    if (updated != m_over) {
      Layout *last = m_over;
      m_over = updated; // NOTE: ORDER IS IMPORTANT. Either MouseOut or MouseOver can destroy frames. We need to set m_over *first*, in case either of our calls destroy the new frame and m_over needs to be cleared!

      if (last) {
        last->EventTrigger(Event::MouseOut);
      }

      mouseover = true;
    }

    // Do our mousemove messages
    if (m_over) {
      m_over->EventTrigger(Event::MouseMove, Point(x, y));
    }

    // Do our mousemoveout messages
    for (std::map<int, Layout *>::const_iterator itr = m_buttonDown.begin(); itr != m_buttonDown.end(); ++itr) {
      if (itr->second && itr->second != m_over) {
        itr->second->EventTrigger(Event::MouseMoveoutside, Point(x, y));
      }
    }

    if (mouseover && m_over) {
      m_over->EventTrigger(Event::MouseOver);
    }
  }

  bool Environment::MouseDown(int button) {
    if (m_buttonDown[button]) {
      LogError(Utility::Format("Received a mouse down message for button %d while in the middle of a click. Fabricating fake MouseUp message in order to preserve ordering guarantees.", button));
      MouseUp(button);
    }

    if (m_over) {
      m_buttonDown[button] = m_over;

      m_over->EventTrigger(Event::MouseButtonDown, button);
      if (button == 0) {
        m_over->EventTrigger(Event::MouseLeftDown);
      } else if (button == 1) {
        m_over->EventTrigger(Event::MouseRightDown);
      } else if (button == 2) {
        m_over->EventTrigger(Event::MouseMiddleDown);
      }

      return true;
    }
    return false;
  }

  bool Environment::MouseUp(int button) {
    // be careful: any event can cause m_over or any m_buttonDown to be cleared
    bool consumed = false;

    if (m_over) {
      consumed = true;
      
      m_over->EventTrigger(Event::MouseButtonUp, button);
      if (m_over) {
        if (button == 0) {
          m_over->EventTrigger(Event::MouseLeftUp);
        } else if (button == 1) {
          m_over->EventTrigger(Event::MouseRightUp);
        } else if (button == 2) {
          m_over->EventTrigger(Event::MouseMiddleUp);
        }
      }

      if (m_over && m_buttonDown[button] == m_over) {
        m_over->EventTrigger(Event::MouseButtonClick, button);
        if (m_over) {
          if (button == 0) {
            m_over->EventTrigger(Event::MouseLeftClick);
          } else if (button == 1) {
            m_over->EventTrigger(Event::MouseRightClick);
          } else if (button == 2) {
            m_over->EventTrigger(Event::MouseMiddleClick);
          }
        }
      }
    }

    Layout *mbd = m_buttonDown[button];
    if (mbd && mbd != m_over) {
      mbd->EventTrigger(Event::MouseButtonUpoutside, button);

      if (mbd) {
        if (button == 0) {
          mbd->EventTrigger(Event::MouseLeftUpoutside);
        } else if (button == 1) {
          mbd->EventTrigger(Event::MouseRightUpoutside);
        } else if (button == 2) {
          mbd->EventTrigger(Event::MouseMiddleUpoutside);
        }
      }
    }

    m_buttonDown[button] = 0; // We explicitly do *not* erase items from this set ever! This lets us iterate over the set while removing elements from it.

    return consumed;
  }
  //void Environment::MouseWheel(int delta);

  //void Environment::MouseClear();  // mouse no longer in the scene at all*/

  bool Environment::KeyDown(const KeyEvent &key) {
    m_lastEvent = key;
    if (m_focus) {
      m_focus->EventTrigger(Event::KeyDown, key);
      return true;
    }
    return false;
  }

  bool Environment::KeyType(const std::string &type) {
    if (m_focus) {
      m_focus->EventTrigger(Event::KeyType, type);
      return true;
    }
    return false;
  }

  bool Environment::KeyRepeat(const KeyEvent &key) {
    m_lastEvent = key;
    if (m_focus) {
      m_focus->EventTrigger(Event::KeyRepeat, key);
      return true;
    }
    return false;
  }

  bool Environment::KeyUp(const KeyEvent &key) {
    m_lastEvent = key;
    if (m_focus) {
      m_focus->EventTrigger(Event::KeyUp, key);
      return true;
    }
    return false;
  }

  bool Environment::IsShift() const {
    return m_lastEvent.shift;
  }

  bool Environment::IsCtrl() const {
    return m_lastEvent.ctrl;
  }

  bool Environment::IsAlt() const {
    return m_lastEvent.alt;
  }

  void Environment::SetFocus(Layout *layout) {
    if (layout->GetEnvironment() != this) {
      LogError("Attempted to set focus to frame with incorrect environment");
    } else {
      m_focus = layout;
    }
  }

  void Environment::ClearFocus() {
    m_focus = 0;
  }

  void Environment::Render(const Layout *root) {
    Performance perf(this, 0.3f, 0.5f, 0.3f);

    if (!root) {
      root = m_root;
    }

    if (root->GetEnvironment() != this) {
      GetConfiguration().logger->LogError("Attempt to render a frame through an unrelated environment");
      return;
    }

    {
      Performance perf(this, 1, 0, 0);
      // We want to batch up events if possible (todo: is this the case? which is faster - flushing as they go, or flushing all at once?) so, two nested loops
      while (!m_invalidated.empty()) {
        while (!m_invalidated.empty()) {
          Layout *layout = m_invalidated.front();
          m_invalidated.pop_front();

          layout->Resolve();
        }

        // send events here
      }
    }

    {
      Performance perf(this, 0.5f, 0.8f, 0.5f);

      {
        Performance perf(this, 0.4f, 0.2f, 0.2f);
        m_renderer->Begin((int)m_root->GetWidth(), (int)m_root->GetHeight());
      }

      {
        Performance perf(this, 0.8f, 0.6f, 0.6f);
        root->Render(m_renderer);
      }

      {
        Performance perf(this, 0.4f, 0.2f, 0.2f);
        m_renderer->End();
      }
    }
  }

  Layout *Environment::GetFrameUnder(float x, float y) {
    // TODO: de-invalidate
    return m_root->GetFrameUnder(x, y);
  }

  void Environment::LuaRegister(lua_State *L, bool hasErrorHandle) {
    LuaStackChecker lsc(L, this, hasErrorHandle ? -1 : 0);

    if (!hasErrorHandle) {
      // insert our default handler
      
      // add the "this" pointer and the traceback function
      lua_pushlightuserdata(L, this);
      lua_getglobal(L, "debug");
      lua_getfield(L, -1, "traceback");
      lua_remove(L, -2);  // remove the "debug" table
      lua_pushcclosure(L, luaF_errorDefault, 2);
    }

    // ==================
    // FUNCTION METATABLES
    
    // insert our framespec metatable table
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_mt");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_mt");
    }
    lua_pop(L, 1);

    // insert the EventHandler metatable - this used to require Frame_ehl to be inserted first! (doesn't now because it doesn't do anything)
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_ehmt");
    if (lua_isnil(L, -1)) {
      // TODO: Actually create the EHMT
      //EventHandle::INTERNAL_l_CreateMetatable(L);
      lua_newtable(L);  // make this go away
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_ehmt");
    }
    lua_pop(L, 1);
    
    // ==================
    //  FRAME REGISTRY
    
    // insert our frame registry table - luatable to lightuserdata. Split up by type, so we can do type checking and handle MI pointer conversion. Maybe make this optional? I think MI can be handled differently.
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_rg");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_rg");
    }
    lua_pop(L, 1);

    // insert our frame reverse registry table - lightuserdata to luatable. Does not need to be split up by type, we'll always be pushing it as a Layout*.
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_rrg");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_rrg");
    }
    lua_pop(L, 1);

    // ==================
    //  FRAME EVENT HANDLERS
    
    // insert our frame event handler table - registry ID to event callback. Managed with luaL_ref
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_fevh");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_fevh");
    }
    lua_pop(L, 1);

    // insert our reverse frame event handler table - event callback to registry ID
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_rfevh");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_rfevh");
    }
    lua_pop(L, 1);

    // insert our frame event handler count table - registry ID to refcount
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_cfevh");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_cfevh");
    }
    lua_pop(L, 1);
    
    // ==================
    //  FRAME EVENTS
    
    // insert our frame event ID table - table to lightuserdata pointer
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_fev");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_fev");
    }
    lua_pop(L, 1);
    
    // ==================
    //  FRAME EVENT HANDLES
    
    // insert the EventHandler lookup table - table to lightuserdata pointer
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_ehl");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_ehl");
    }
    lua_pop(L, 1);
    
    // ==================
    //  FRAME MISC
    
    // insert our environment registry table - lightuserdata to error handle.
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_env");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_env");
    }
    lua_pop(L, 1);
    
    // insert the "root" lua environment
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_lua");
    if (lua_isnil(L, -1)) {
      lua_pushlightuserdata(L, L); // lol
      lua_setfield(L, LUA_REGISTRYINDEX, "Frame_lua");
    }
    lua_pop(L, 1);

    // and now insert *us* into the env table
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_env");
    lua_pushlightuserdata(L, this);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);  // toot

    // insert our global table
    lua_getfield(L, LUA_GLOBALSINDEX, "Frames");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, LUA_GLOBALSINDEX, "Frames");
    }
    lua_pop(L, 1);

    // we'll need this for Root
    LuaRegisterFrameLookup<Layout>(L);

    // And insert the Root member into the frames
    lua_getglobal(L, "Frames");

    // Don't overwrite the old one (this is important - our error compensation for trying to push a frame into Lua from an unregistered Frame environment will register it, and swapping out Frame.Root without warning is really really bad!)
    lua_getfield(L, -1, "Root");
    if (lua_isnil(L, -1)) {
      m_root->luaF_push(L);
      lua_setfield(L, -3, "Root");
    }

    lua_pop(L, 3);  // the nil/root still on the stack, the Frame global, and the error handler

    m_lua_environments.insert(L);
    
    // Register our built-in frames
    LuaRegisterFrame<Frame>(L);
    LuaRegisterFrame<Text>(L);
    LuaRegisterFrame<Texture>(L);
    LuaRegisterFrame<Mask>(L);
    LuaRegisterFrame<Raw>(L);
    
    // Register our built-in frame events
    LuaRegisterEvent(L, &Event::Move);
    LuaRegisterEvent(L, &Event::Size);

    LuaRegisterEvent(L, &Event::MouseOver);
    LuaRegisterEvent(L, &Event::MouseMove);
    LuaRegisterEvent(L, &Event::MouseMoveoutside);
    LuaRegisterEvent(L, &Event::MouseOut);

    LuaRegisterEvent(L, &Event::MouseLeftUp);
    LuaRegisterEvent(L, &Event::MouseLeftUpoutside);
    LuaRegisterEvent(L, &Event::MouseLeftDown);
    LuaRegisterEvent(L, &Event::MouseLeftClick);

    LuaRegisterEvent(L, &Event::MouseMiddleUp);
    LuaRegisterEvent(L, &Event::MouseMiddleUpoutside);
    LuaRegisterEvent(L, &Event::MouseMiddleDown);
    LuaRegisterEvent(L, &Event::MouseMiddleClick);

    LuaRegisterEvent(L, &Event::MouseRightUp);
    LuaRegisterEvent(L, &Event::MouseRightUpoutside);
    LuaRegisterEvent(L, &Event::MouseRightDown);
    LuaRegisterEvent(L, &Event::MouseRightClick);

    LuaRegisterEvent(L, &Event::MouseButtonUp);
    LuaRegisterEvent(L, &Event::MouseButtonUpoutside);
    LuaRegisterEvent(L, &Event::MouseButtonDown);
    LuaRegisterEvent(L, &Event::MouseButtonClick);

    LuaRegisterEvent(L, &Event::MouseWheel);

    LuaRegisterEvent(L, &Event::KeyDown);
    LuaRegisterEvent(L, &Event::KeyType);
    LuaRegisterEvent(L, &Event::KeyRepeat);
    LuaRegisterEvent(L, &Event::KeyUp);
      
    LuaRegisterEvent(L, &Event::Render);
  }
  
  void Environment::LuaRegisterEvent(lua_State *L, EventTypeBase *feb) {
    LuaStackChecker lsc(L, this);
    
    lua_getfield(L, LUA_GLOBALSINDEX, "Frames");
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_newtable(L);
      lua_pushvalue(L, -1);
      lua_setfield(L, LUA_GLOBALSINDEX, "Frames");
    }
    
    lua_getfield(L, -1, "Event");
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_newtable(L);
      lua_pushvalue(L, -1);
      lua_setfield(L, -3, "Event");
    }
    lua_remove(L, -2);
    
    // ... Event
    
    const char *name = feb->GetName();
    int chunkstart = 0;
    int chunkend = 1;
    while (name[chunkend]) {
      while (name[chunkend] && !isupper(name[chunkend])) {
        ++chunkend;
      }
      
      if (!name[chunkend]) {
        break;
      }
      
      // ... Event
      
      lua_pushlstring(L, name + chunkstart, chunkend - chunkstart);
      lua_rawget(L, -2);
      if (lua_isnil(L, -1)) {
        // ... Event nil
        lua_pop(L, 1);
        // ... Event
        lua_newtable(L);
        lua_pushlstring(L, name + chunkstart, chunkend - chunkstart);
        lua_pushvalue(L, -2);
        // ... Event container Name container
        lua_rawset(L, -4);
        // ... Event container
      }
      // ... Event container
      lua_remove(L, -2);
      // ... container
      chunkstart = chunkend;
      ++chunkend;
    }
    
    // ... Parent
    
    lua_pushlstring(L, name + chunkstart, chunkend - chunkstart);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      // Already registered? Maybe! TODO check to see if this is the right item, scream if it's not
      lua_pop(L, 2);
      return;
    }
    lua_pop(L, 1);
    
    lua_pushlstring(L, name + chunkstart, chunkend - chunkstart);
    
    // ... Parent name
    
    lua_newuserdata(L, 0);
    
    // ... Parent name handle
    
    lua_pushvalue(L, -1);
    
    // ... Parent name handle handle
    
    lua_insert(L, -4);
    
    // ... handle Parent name handle
    
    lua_rawset(L, -3);
    
    // ... handle Parent
    
    // handle is inserted in the public hierarchy!
    
    lua_pop(L, 1);
    
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_fev");
    lua_insert(L, -2);
    
    // ... Frame_fev handle
    
    lua_pushlightuserdata(L, feb);
    lua_rawset(L, -3);
    
    // ... Frame_fev
    
    lua_pop(L, 1);
  }

  void Environment::LuaUnregister(lua_State *L) {
    LuaStackChecker lsc(L, this);

    m_root->luaF_ClearEvents_Recursive(L);

    // It's somewhat unclear if this should be O(n) in the number of frames in the frames environment, or O(n) in the number of frames in the lua environment
    // We're doing lua environment for now, partially just because it's easier.

    // This bit is a little finicky. We want to do it without triggering allocations (if there's one thing that is a nightmare, it's allocations during shutdown)
    // However, lua tables aren't guaranteed to keep their ordering if the contents change.
    // So do we a little extra work, and use some extra CPU cycles, here to prevent changes.

    // Grab the lightuserdata to luatable lookup
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_rrg");

    // Grab the luatable to lightuserdata lookup
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_rg");

    if (lua_isnil(L, -1) || lua_isnil(L, -2)) {
      lua_pop(L, 2);
      LogError("Attempting to unregister an environment that was never registered");
      return;
    }

    int ct = 0; // number of frames we're deallocating

    // We're going to iterate through the reverse lookup. For each frame that shares this environment, we're going to first mark it with a "false" value instead of a "true" value
    // We're also going to iterate through the forward lookup and remove it from every forward element. This could probably be done more quickly.
    // If you somehow find that unregistering is a bottleneck let me know and we'll fix it.

    lua_pushnil(L); // first key
    // for this loop, the stack is ... Frame_rrg Frame_rg key/frameuserdata 
    while (lua_next(L, -3) != 0) {
      // stack: ... Frame_rrg Frame_rg key/frameuserdata frametable
      // See if this is our environment. If it's possible for things to not be Layouts, we may need to tweak this later.
      Layout *layout = (Layout *)lua_touserdata(L, -2);
      if (layout->GetEnvironment() == this) {
        // This one is getting killed
        // First, we want to plow through the forward registry and strip it as necessary

        lua_pushnil(L);
        // for this loop, the stack is ... Frame_rrg Frame_rg key/frameuserdata frametable forwardkey
        while (lua_next(L, -4) != 0) {
          // stack: ... Frame_rrg Frame_rg key/frameuserdata frametable forwardkey lookuptable

          // just straight-up set it to nil
          lua_pushvalue(L, -3);
          lua_pushnil(L);
          lua_rawset(L, -3);

          // throw away the lookuptable

          lua_pop(L, 1);
        }

        // current stack:
        // stack: ... Frame_rrg Frame_rg key/frameuserdata frametable

        // set the reverse registry to false
        lua_pushvalue(L, -2);
        lua_pushboolean(L, false);
        // stack: ... Frame_rrg Frame_rg key/frameuserdata frametable frameuserdata false
        lua_rawset(L, -6);

        // stack: ... Frame_rrg Frame_rg key/frameuserdata frametable
        // Finally, increment our count
        ct++;
      }

      // Throw away the value, get ready for the next iteration
      lua_pop(L, 1);
    }

    // stack: ... Frame_rrg Frame_rg
    lua_pop(L, 1);
    // stack: ... Frame_rrg

    int passes = 0;

    // Now we need to clean up the reverse registry table
    while (ct) {
      passes++;

      int ct_last = ct;

      lua_pushnil(L);
      if (!lua_next(L, -2)) {
        break;  // no elements, this shouldn't happen, but we'll pop an error later
      }

      // this is a weird loop
      while (true) {
        // stack: ... Frame_rrg key/frameuserdata frametable

        if (lua_isboolean(L, -1)) {
          // this should be removed, but first, we need to reserve the next element. but we don't need the boolean anymore!
          // stack: ... Frame_rrg key/frameuserdata true
          lua_pop(L, 1);

          // stack: ... Frame_rrg key/frameuserdata
          lua_pushvalue(L, -1);

          // stack: ... Frame_rrg key/frameuserdata key/frameuserdata
          if (lua_next(L, -3) == 0) {
            // push two nils in place instead, just to preserve the ordering
            lua_pushnil(L);
            lua_pushnil(L);
          }

          // stack: ... Frame_rrg key/frameuserdata nextkey nextvalue
          lua_pushvalue(L, -3);

          // stack: ... Frame_rrg key/frameuserdata nextkey nextvalue key/frameuserdata
          lua_remove(L, -4);

          // stack: ... Frame_rrg nextkey nextvalue key/frameuserdata
          
          // now zero it out in rrg
          lua_pushnil(L);
          // stack: ... Frame_rrg nextkey nextvalue key/frameuserdata nil
          lua_rawset(L, -5);

          // decrement ct
          ct--;

          // now we just have nextkey/nextvalue hanging around, almost done
          if (lua_isnil(L, -1) || !ct) {
            // we're actually done
            // stack: ... Frame_rrg key/frameuserdata frametable
            lua_pop(L, 2);
            // stack: ... Frame_rrg
            break;
          }
        } else {
          // this shouldn't be removed
          // incrementing is easy
          lua_pop(L, 1);
          // stack: ... Frame_rrg key/frameuserdata
          if (!lua_next(L, -2)) {
            // stack: ... Frame_rrg key/frameuserdata frametable
            break;
          }
        }
      }

      if (ct_last == ct) {
        // we somehow got through that loop without stripping anything, stop
        break;
      }
    }
    
    if (ct) {
      LogError(Utility::Format("Failed to clean up when doing a full unregister, %d remaining", ct));
    }

    if (passes > 1) {
      LogError(Utility::Format("Failed to clean up in a single pass, took %d", passes));
    }

    lua_pop(L, 1);

    m_lua_environments.erase(L); // and we're done!
  }
  
  Environment::Performance::Performance(Environment *env, float r, float g, float b) : m_env(env) { // handle left uninitialized intentionally
    if (m_env->GetConfiguration().performance) {
      m_handle = m_env->GetConfiguration().performance->Push(r, g, b);
    }
  }
  Environment::Performance::~Performance() {
    if (m_env->GetConfiguration().performance) {
      m_env->GetConfiguration().performance->Pop(m_handle);
    }
  }

  void Environment::Init(const Configuration &config) {
    m_config_logger_owned = 0;
    m_config_tfi_owned = 0;
    m_config_sfi_owned = 0;
    m_config_pfi_owned = 0;
    m_config_tfs_owned = 0;
    m_config_clipboard_owned = 0;

    m_config = config;

    if (!m_config.logger) {
      m_config_logger_owned = new Configuration::Logger();
      m_config.logger = m_config_logger_owned;
    }

    if (!m_config.textureFromId) {
      m_config_tfi_owned = new Configuration::TextureFromId();
      m_config.textureFromId = m_config_tfi_owned;
    }

    if (!m_config.streamFromId) {
      m_config_sfi_owned = new Configuration::StreamFromId();
      m_config.streamFromId = m_config_sfi_owned;
    }

    if (!m_config.pathFromId) {
      m_config_pfi_owned = new Configuration::PathFromId();
      m_config.pathFromId = m_config_pfi_owned;
    }

    if (!m_config.textureFromStream) {
      m_config_tfs_owned = new Configuration::TextureFromStream();
      m_config.textureFromStream = m_config_tfs_owned;
    }

    if (!m_config.clipboard) {
      m_config_clipboard_owned = new Configuration::Clipboard();
      m_config.clipboard = m_config_clipboard_owned;
    }

    m_root = new Layout(0, this);
    m_root->SetNameStatic("Root");

    m_over = 0;
    m_focus = 0;

    m_renderer = new Renderer(this);
    m_text_manager = new TextManager(this);
    m_texture_manager = new TextureManager(this);
  }

  void Environment::MarkInvalidated(Layout *layout) {
    m_invalidated.push_back(layout);
  }

  void Environment::UnmarkInvalidated(Layout *layout) {
    std::deque<Layout *>::iterator itr = find(m_invalidated.begin(), m_invalidated.end(), layout);
    if (itr == m_invalidated.end()) {
      LogError("Internal problem, attempted to unmark and failed");
    } else {
      m_invalidated.erase(itr);
    }
  }

  void Environment::LayoutStack_Push(const Layout *layout, Axis axis, float pt) {
    LayoutStack_Entry entry = {layout, axis, pt};
    m_layoutStack.push_back(entry);
  }

  void Environment::LayoutStack_Push(const Layout *layout, Axis axis) {
    LayoutStack_Entry entry = {layout, axis, Utility::Undefined};
    m_layoutStack.push_back(entry);
  }

  void Environment::LayoutStack_Pop() {
    m_layoutStack.pop_back();
  }

  void Environment::LayoutStack_Error() {
    if (m_layoutStack.empty()) {
      LogError("Layout loop dependency message received, but stack is empty. This should never happen.");
      return;
    }

    LogError(Utility::Format("Layout loop dependency detected, axis %c:", (m_layoutStack[0].axis == X) ? 'X' : 'Y'));
    for (int i = m_layoutStack.size(); i > 0; --i) {
      LayoutStack_Entry entry = m_layoutStack[i - 1];
      if (Utility::IsUndefined(entry.point))
        LogError(Utility::Format("  %s: size", entry.layout->GetNameFull().c_str()));
      else if (entry.point == 0 && entry.axis == X)
        LogError(Utility::Format("  %s: LEFT", entry.layout->GetNameFull().c_str()));
      else if (entry.point == 0 && entry.axis == Y)
        LogError(Utility::Format("  %s: TOP", entry.layout->GetNameFull().c_str()));
      else if (entry.point == 0.5)
        LogError(Utility::Format("  %s: CENTER", entry.layout->GetNameFull().c_str()));
      else if (entry.point == 1 && entry.axis == X)
        LogError(Utility::Format("  %s: RIGHT", entry.layout->GetNameFull().c_str()));
      else if (entry.point == 1 && entry.axis == Y)
        LogError(Utility::Format("  %s: BOTTOM", entry.layout->GetNameFull().c_str()));
      else
        LogError(Utility::Format("  %s: %f", entry.layout->GetNameFull().c_str(), entry.point));
    }
  }

  void Environment::DestroyingLayout(Layout *layout) {
    if (m_over == layout) {
      m_over = 0;
      // TODO: refresh mouse position based on the new layout?
    }

    if (m_focus == layout) {
      m_focus = 0;
    }

    for (std::map<int, Layout *>::iterator itr = m_buttonDown.begin(); itr != m_buttonDown.end(); ++itr) {
      if (itr->second == layout) {
        itr->second = 0;  // We explicitly do *not* erase items from this set ever! This lets us iterate over the set while removing elements from it.
      }
    }
  }

  Environment::LuaStackChecker::LuaStackChecker(lua_State *L, Environment *env, int offset) : m_depth(lua_gettop(L) + offset), m_L(L), m_env(env) {
  }

  Environment::LuaStackChecker::~LuaStackChecker() {
    if (m_depth != lua_gettop(m_L)) {
      m_env->LogError(Utility::Format("Lua stack size mismatch (%d -> %d)", m_depth, lua_gettop(m_L)));
    }
  }

  void Environment::Lua_PushErrorHandler(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "Frame_env");
    lua_pushlightuserdata(L, this);
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }

  /*static*/ int Environment::luaF_errorDefault(lua_State *L) {
    lua_pushliteral(L, "\n");
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_call(L, 0, 1);
    // now we have our stack trace

    lua_concat(L, 3); // and now we have a full error message

    Environment *env = (Environment*)lua_touserdata(L, lua_upvalueindex(1));
    env->LogError(lua_tostring(L, -1));

    return 1; // not that this will be used, but
  }
}

