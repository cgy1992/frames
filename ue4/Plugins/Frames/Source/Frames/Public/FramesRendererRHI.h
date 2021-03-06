/*  Copyright 2014 Mandible Games
    
    This file is part of Frames.
    
    Please see the COPYING file for detailed licensing information.
    
    Frames is dual-licensed software. It is available under both a
    commercial license, and also under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    Frames is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Frames.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef FRAMES_UE4_RENDERER_RHI
#define FRAMES_UE4_RENDERER_RHI

// for whatever reason, ue4.3 broke several includes, so now we need to include this even though we don't really use it
// thanks obama

#include "RHI.h"
#include "RHIResources.h"

#include "frames/renderer.h"

#include "frames/configuration.h"

#include <map>

class FRHICommandList;

namespace Frames {
  namespace Configuration {
    /// Creates a Configuration::Renderer for UE4 RHI.
    RendererPtr FRAMES_API RendererRHI(ERHIFeatureLevel::Type featureLevel);
  }
  
  namespace detail {

    // Actual data is stored in "Data" subclasses; this class's lifetime is dictated more by the render queue system than anything else
    // Needed because we pass data to the renderer, and the parent class may theoretically have been torn down by then
    // We manually handle this object's lifetime, which is a pain, but so it goes.

    class FRAMES_API TextureBackingRHI : public TextureBacking {
    public:
      TextureBackingRHI(Environment *env, int width, int height, Texture::Format format);
      TextureBackingRHI(Environment *env, FTexture2DRHIParamRef rhi);
      
      ~TextureBackingRHI();

      virtual void Write(int sx, int sy, const TexturePtr &tex) override;

      struct Data : detail::Noncopyable {
        FTexture2DRHIRef m_tex;
      };
      Data *DataGet() const { return m_rhi; }

    private:
      Data *m_rhi;
    };

    // Used for passing pre-generated textures to Frames    
    class FRAMES_API UE4TextureContextual : public Frames::Texture::Contextual {
    public:
      FTexture2DRHIRef m_tex;
    };
    typedef Frames::Ptr<UE4TextureContextual> UE4TextureContextualPtr;

    class FRAMES_API RendererRHI : public Renderer {
    public:
      RendererRHI(Environment *env, ERHIFeatureLevel::Type featureLevel);
      ~RendererRHI();

      virtual void Begin(int width, int height) override;
      virtual void End() override;

      virtual Vertex *Request(int quads) override;
      virtual void Return(int quads = -1) override;  // also renders, count lets you optionally specify the number of quads

      virtual TextureBackingPtr TextureCreate(int width, int height, Texture::Format mode) override;
      virtual TextureBackingPtr TextureCreate(const Texture::ContextualPtr &contextual) override;
      virtual void TextureSet(const TextureBackingPtr &tex) override;

    private:
      void CreateBuffers(int len);
      void FlushRequestData();

      struct Data : detail::Noncopyable {
        FVertexDeclarationRHIRef m_vertexDecl;

        FVertexBufferRHIParamRef GetVertexBuffer(int size);
        std::map<int, FVertexBufferRHIRef> m_vertices;

        FIndexBufferRHIRef m_indices;
      };

      Data *m_rhi;

      struct RequestData : detail::Noncopyable {
        RequestData();
        ~RequestData();

        int quads;
        std::vector<Renderer::Vertex> data;
      };

      RequestData *m_request;

      int m_verticesQuadcount;

      TextureBackingRHI *m_currentTexture;

      ERHIFeatureLevel::Type m_featureLevel;

      virtual void ScissorSet(const Rect &rect) override;
    };
  }
}

#endif

