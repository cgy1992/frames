// The Raw class

#ifndef FRAMES_RAW
#define FRAMES_RAW

#include "frames/frame.h"

namespace Frames {
  class Raw : public Frame {
  public:
    FRAMES_VERB_DECLARE_BEGIN
      /// Signals that this Raw frame is ready to render.
      /** WARNING: Unlike most verbs, while this verb is being signaled, it is undefined behavior to call *any* non-Get function provided by Frames and associated with this Environment.*/
      FRAMES_VERB_DECLARE(Render, ());
    FRAMES_VERB_DECLARE_END

  private:
    FRAMES_DECLARE_RTTI();
    friend class Environment;

  public:
    static Raw *Create(Layout *parent, const std::string &name);
    
  private:
    Raw(Layout *parent, const std::string &name);
    virtual ~Raw();

    virtual void RenderElement(detail::Renderer *renderer) const;
  };
}

#endif
