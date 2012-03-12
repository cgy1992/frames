// The Texture class

#ifndef FRAMES_TEXTURE
#define FRAMES_TEXTURE

#include "frames/frame.h"
#include "frames/texture_manager.h"

namespace Frames {
  class Texture : public Frame {
  public:
    static Texture *CreateBare(Layout *parent);
    static Texture *CreateTagged_imp(const char *filename, int line, Layout *parent);

    static const char *GetStaticType();

    void SetTexture(const std::string &id);

  private:
    Texture(Layout *parent);
    virtual ~Texture();

    virtual void RenderElement(Renderer *renderer) const;

    std::string m_text;
    TextureChunkPtr m_texture;
  };
}

#endif
