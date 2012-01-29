// The Environment class

#ifndef FRAMES_ENVIRONMENT
#define FRAMES_ENVIRONMENT

#include "frames/configuration.h"
#include "frames/noncopyable.h"
#include "frames/renderer.h"
#include "frames/utility.h"

#include <deque>
#include <vector>

namespace Frames {
  class Layout;

  class Environment : Noncopyable {
  public:
    Environment();  // init to default
    Environment(const Configuration &config);
    ~Environment();

    void ResizeRoot(int x, int y);
    
    void Render(const Layout *root = 0);
    
    Layout *GetRoot() { return m_root; }
    const Configuration &GetConfiguration() { return m_config; }

    // Internal only, do not call
    void LogError(const std::string &log) { if (m_config.logger) m_config.logger->LogError(log); }
    void LogDebug(const std::string &log) { if (m_config.logger) m_config.logger->LogDebug(log); }
  private:
    friend class Layout;

    // Utility functions and parameters
    void Init(const Configuration &config);

    void MarkInvalidated(const Layout *layout);
    std::deque<const Layout *> m_invalidated;

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

    // Configuration
    Configuration m_config;
    Configuration::Logger *m_config_logger_owned;

    // Managers
    Renderer m_renderer;

    // Root
    Layout *m_root;
  };
}

#endif
