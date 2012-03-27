
#include "frames/texture.h"

#include "frames/environment.h"
#include "frames/lua.h"
#include "frames/renderer.h"
#include "frames/texture_manager.h"

#include <GL/GLew.h>

namespace Frames {
  Texture *Texture::CreateBare(Layout *parent) {
    return new Texture(parent);
  }
  Texture *Texture::CreateTagged_imp(const char *filename, int line, Layout *parent) {
    Texture *rv = new Texture(parent);
    rv->SetNameStatic(filename);
    rv->SetNameId(line);
    return rv;
  }

  /*static*/ const char *Texture::GetStaticType() {
    return "Texture";
  }

  void Texture::SetTexture(const std::string &id) {
    // work work work
    m_texture_id = id;
    m_texture = GetEnvironment()->GetTextureManager()->TextureFromId(id);
    
    SetWidthDefault(m_texture->GetWidth());
    SetHeightDefault(m_texture->GetHeight());
  }

  /*static*/ void Texture::l_RegisterFunctions(lua_State *L) {
    Frame::l_RegisterFunctions(L);

    l_RegisterFunction(L, GetStaticType(), "SetTexture", l_SetTexture);
    l_RegisterFunction(L, GetStaticType(), "GetTexture", l_GetTexture);
  }

  static const Color white = Color(1, 1, 1, 1);
  void Texture::RenderElement(Renderer *renderer) const {
    Frame::RenderElement(renderer);

    if (m_texture) {
      renderer->SetTexture(m_texture.get());

      float u = GetTop();
      float d = GetBottom();
      float l = GetLeft();
      float r = GetRight();

      Renderer::Vertex *v = renderer->Request(4);

      v[0].p.x = l; v[0].p.y = u;
      v[1].p.x = r; v[1].p.y = u;
      v[2].p.x = r; v[2].p.y = d;
      v[3].p.x = l; v[3].p.y = d;

      v[0].t = m_texture->GetBounds().s;
      v[2].t = m_texture->GetBounds().e;
      
      v[1].t.x = v[2].t.x;
      v[1].t.y = v[0].t.y;
      
      v[3].t.x = v[0].t.x;
      v[3].t.y = v[2].t.y;

      v[0].c = white;
      v[1].c = white;
      v[2].c = white;
      v[3].c = white;

      renderer->Return(GL_QUADS);
    }
  }

  Texture::Texture(Layout *parent) :
      Frame(parent)
  { };
  Texture::~Texture() { };

  /*static*/ int Texture::l_SetTexture(lua_State *L) {
    l_checkparams(L, 2);
    Texture *self = l_checkframe<Texture>(L, 1);

    self->SetTexture(luaL_checkstring(L, 2));

    return 0;
  }

  /*static*/ int Texture::l_GetTexture(lua_State *L) {
    l_checkparams(L, 1);
    Texture *self = l_checkframe<Texture>(L, 1);

    lua_pushstring(L, self->GetTexture().c_str());

    return 1;
  }
}
