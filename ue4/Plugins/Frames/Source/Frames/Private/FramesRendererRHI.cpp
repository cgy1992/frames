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

#include "FramesPCH.h"

#include "FramesRendererRHI.h"

#include "ShaderParameterUtils.h"
#include "RenderingThread.h"
#include "GlobalShader.h"
#include "RHIStaticStates.h"
#include "Runtime/Launch/Resources/Version.h"

#include "frames/configuration.h"
#include "frames/detail.h"
#include "frames/detail_format.h"
#include "frames/environment.h"
#include "frames/rect.h"

#include <vector>
#include <algorithm>

using namespace std;

namespace Frames {
  namespace Configuration {
    class CfgRendererRHI : public Renderer {
    public:
      CfgRendererRHI(ERHIFeatureLevel::Type featureLevel) : m_featureLevel(featureLevel) {
      }

      virtual detail::Renderer *Create(Environment *env) const override {
        return new detail::RendererRHI(env, m_featureLevel);
      }

    private:
      ERHIFeatureLevel::Type m_featureLevel;
    };

    Configuration::RendererPtr Configuration::RendererRHI(ERHIFeatureLevel::Type featureLevel) {
      return Configuration::RendererPtr(new CfgRendererRHI(featureLevel));
    }
  }

  namespace detail {
    // COMPATIBILITY SHIMS
    #if !defined(ENGINE_MAJOR_VERSION) || !defined(ENGINE_MINOR_VERSION)
      #error Cannot find UE4 version, compilation will fail.
    #endif

    #if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 5
      static TShaderMap<FGlobalShaderType> *GetGlobalShaderMap_Shim(ERHIFeatureLevel::Type featureLevel) {
        return GetGlobalShaderMap();
      };
      static void SetGlobalBoundShaderState_Shim(FRHICommandListImmediate& RHICmdList,
          ERHIFeatureLevel::Type featureLevel,
	        FGlobalBoundShaderState& BoundShaderState,
	        FVertexDeclarationRHIParamRef VertexDeclaration,
	        FShader* VertexShader,
	        FShader* PixelShader,
	        FShader* GeometryShader = nullptr) {
        SetGlobalBoundShaderState(RHICmdList, BoundShaderState, VertexDeclaration, VertexShader, PixelShader, GeometryShader);
      }
    #else
      static TShaderMap<FGlobalShaderType> *GetGlobalShaderMap_Shim(ERHIFeatureLevel::Type featureLevel) {
        return GetGlobalShaderMap(featureLevel);
      };
      static void SetGlobalBoundShaderState_Shim(FRHICommandList& RHICmdList,
          ERHIFeatureLevel::Type featureLevel,
	        FGlobalBoundShaderState& BoundShaderState,
	        FVertexDeclarationRHIParamRef VertexDeclaration,
	        FShader* VertexShader,
	        FShader* PixelShader,
	        FShader* GeometryShader = nullptr) {
        SetGlobalBoundShaderState(RHICmdList, featureLevel, BoundShaderState, VertexDeclaration, VertexShader, PixelShader, GeometryShader);
      }
    #endif

    // I guess this is how shaders work in the crazy world of ue4
    class FFramesVS : public FGlobalShader
    {
      DECLARE_SHADER_TYPE(FFramesVS, Global);
    public:

      static bool ShouldCache(EShaderPlatform Platform) { return true; }

      FFramesVS(const ShaderMetaType::CompiledShaderInitializerType &Initializer) :
        FGlobalShader(Initializer)
      {
        m_size.Bind(Initializer.ParameterMap, TEXT("size"), SPF_Mandatory);
      }
      FFramesVS() {}

      void SetParameterSize(FRHICommandList &RHICmdList, int width, int height) {
        SetShaderValue(RHICmdList, GetVertexShader(), m_size, FVector2D(width, height));
      }

      virtual bool Serialize(FArchive& Ar) {
        bool outdated = FGlobalShader::Serialize(Ar);
        Ar << m_size;

        return outdated;
      }

    private:
      FShaderParameter m_size;
    };
    IMPLEMENT_SHADER_TYPE(, FFramesVS, TEXT("FramesShader"), TEXT("VS"), SF_Vertex);

    class FFramesPS : public FGlobalShader
    {
      DECLARE_SHADER_TYPE(FFramesPS, Global);
    public:

      static bool ShouldCache(EShaderPlatform Platform) { return true; }

      FFramesPS(const ShaderMetaType::CompiledShaderInitializerType &Initializer) :
        FGlobalShader(Initializer)
      {
        m_textureMode.Bind(Initializer.ParameterMap, TEXT("sampleMode"), SPF_Mandatory);
        m_texture.Bind(Initializer.ParameterMap, TEXT("sprite"), SPF_Mandatory);
      }
      FFramesPS() {}

      void SetParameterTexture(FRHICommandList &RHICmdList, FTexture2DRHIParamRef tex, bool alpha) {
        if (tex) {
          if (alpha) {
            SetShaderValue(RHICmdList, GetPixelShader(), m_textureMode, 2);
          } else {
            SetShaderValue(RHICmdList, GetPixelShader(), m_textureMode, 1);
          }
          SetTextureParameter(RHICmdList, GetPixelShader(), m_texture, tex);
        } else {
          SetShaderValue(RHICmdList, GetPixelShader(), m_textureMode, 0);
        }
      }

      virtual bool Serialize(FArchive& Ar) {
        bool outdated = FGlobalShader::Serialize(Ar);
        Ar << m_textureMode;
        Ar << m_texture;

        return outdated;
      }

    private:
      FShaderParameter m_textureMode;
      FShaderResourceParameter m_texture;
    };
    IMPLEMENT_SHADER_TYPE(, FFramesPS, TEXT("FramesShader"), TEXT("PS"), SF_Pixel);

    /*
    static const char sShader[] =
      "cbuffer Size : register(b0) { float width : packoffset(c0.x); float height : packoffset(c0.y); };\n"  // size of screen; changes rarely
      "cbuffer Item : register(b1) { int sampleMode : packoffset(c0); };\n"  // 0 means don't sample. 1 means do sample and multiply. 2 means do sample and multiply; pretend .rgb is 1.f.
      "Texture2D <float4> sprite : register(t0);\n"  // sprite texture to reference if it is being referenced
      "SamplerState spriteSample : register(s0);\n" // sampler to use for sprite texture
      "\n"
      "struct VIn { float2 position : POSITION; float2 tex : TEXCOORD0; float4 color : COLOR; };\n"
      "struct VOut { float4 position : SV_POSITION; float2 tex : TEXCOORD0; float4 color : COLOR; };\n"
      "VOut VS(VIn input) { VOut output; float2 cp = input.position; cp.x /= width; cp.y /= -height; cp *= 2; cp += float2(-1.f, 1.f); output.position = float4(cp, 0.f, 1.f); output.tex = input.tex; output.color = input.color; return output; }\n"
      "float4 PS(VOut input) : SV_TARGET { if (sampleMode == 1) input.color *= sprite.Sample(spriteSample, input.tex); if (sampleMode == 2) input.color.a *= sprite.Sample(spriteSample, input.tex).r; return input.color; }\n";
      */

    TextureBackingRHI::TextureBackingRHI(Environment *env, int width, int height, Texture::Format format) : TextureBacking(env, width, height, format) {
      EPixelFormat rhiformat = PF_Unknown;
      if (format == Texture::FORMAT_RGBA_8) {
        rhiformat = PF_R8G8B8A8;
      } else if (format == Texture::FORMAT_RGB_8) {
        rhiformat = PF_R8G8B8A8;
      } else if (format == Texture::FORMAT_R_8) {
        rhiformat = PF_G8;  // why is this g, not r
      } else {
        EnvironmentGet()->LogError(detail::Format("Unrecognized raw type %d in texture", format));
        return;
      }

      m_rhi = new Data;
      
      ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
        Frames_TextureBacking_Constructor,
        Data *, rhi, m_rhi,
        int, width, width,
        int, height, height,
        EPixelFormat, rhiformat, rhiformat,
      {
        FRHIResourceCreateInfo cinfo;
        rhi->m_tex = RHICreateTexture2D(width, height, rhiformat, 1, 1, TexCreate_ShaderResource, cinfo);
      });
    }

    TextureBackingRHI::TextureBackingRHI(Environment *env, FTexture2DRHIParamRef rhi) : TextureBacking(env, rhi->GetSizeX(), rhi->GetSizeY(), Texture::FORMAT_RGBA_8) {
      m_rhi = new Data;
      m_rhi->m_tex = rhi;
    }

    TextureBackingRHI::~TextureBackingRHI() {
      ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        Frames_TextureBacking_Destructor,
        Data *, rhi, m_rhi,
      {
        delete rhi; // texture contained gets cleaned up by the destructor
      });
    }

    void TextureBackingRHI::Write(int sx, int sy, const TexturePtr &tex) {
      if (tex->TypeGet() != Texture::RAW) {
        EnvironmentGet()->LogError(detail::Format("Unrecognized type %d in texture", tex->TypeGet()));
        return;
      }

      if (tex->FormatGet() != FormatGet()) {
        EnvironmentGet()->LogError(detail::Format("Mismatched texture formats in TextureBacking::Write. Attempted write %d to %d", tex->FormatGet(), FormatGet()));
        return;
      }

      if (tex->RawStrideGet() != Texture::RawBPPGet(tex->FormatGet()) * tex->WidthGet()) {
        EnvironmentGet()->LogError(detail::Format("Stride conversion necessary in TextureBacking::Write - NYI, please make this Texture's stride %d", Texture::RawBPPGet(tex->FormatGet()) * tex->WidthGet()));
        return;
      }

      struct TBWrite {
        int sx;
        int sy;
        int width;
        int height;
        int stride;
        std::vector<unsigned char> data;
      } tbw;

      tbw.sx = sx;
      tbw.sy = sy;
      tbw.width = tex->WidthGet();
      tbw.height = tex->HeightGet();

      if (tex->FormatGet() == Texture::FORMAT_RGB_8) {
        // we get to do some conversion! yaaaay.
        int pixels = tex->WidthGet() * tex->HeightGet();
        tbw.data.resize(pixels * Texture::RawBPPGet(Texture::FORMAT_RGBA_8));
        const unsigned char *read = tex->RawDataGet();
        unsigned char *write = &tbw.data[0];
        
        for (int i = 0; i < pixels; ++i) {
          *write++ = *read++;
          *write++ = *read++;
          *write++ = *read++;
          *write++ = 255;
        }

        tbw.stride = (int)tbw.data.size() / tex->HeightGet(); // yes, this is a wasted div; it's not really going to be an issue, though.
      } else {     
        tbw.stride = tex->RawStrideGet();
        tbw.data = std::vector<unsigned char>(tex->RawDataGet(), tex->RawDataGet() + tbw.height * tbw.stride);
      }

      ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        Frames_TextureBacking_Write,
        Data *, rhi, m_rhi,
        TBWrite, tbw, tbw,
      {
        RHIUpdateTexture2D(rhi->m_tex, 0, FUpdateTextureRegion2D(tbw.sx, tbw.sy, 0, 0, tbw.width, tbw.height), tbw.stride, tbw.data.data());
      });
    }

    RendererRHI::RendererRHI(Environment *env, ERHIFeatureLevel::Type featureLevel) :
      Renderer(env),
      m_rhi(0),
      m_request(0),
      m_verticesQuadcount(0),
      m_currentTexture(0),
      m_featureLevel(featureLevel)
    {
      m_rhi = new Data;

      ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        Frames_Constructor,
        Data *, rhi, m_rhi,
      {
        FVertexDeclarationElementList elements;
        elements.Add(FVertexElement(0, offsetof(Renderer::Vertex, p), VET_Float2, 0, sizeof(Renderer::Vertex)));
        elements.Add(FVertexElement(0, offsetof(Renderer::Vertex, t), VET_Float2, 1, sizeof(Renderer::Vertex)));
        elements.Add(FVertexElement(0, offsetof(Renderer::Vertex, c), VET_Float4, 2, sizeof(Renderer::Vertex)));

        rhi->m_vertexDecl = RHICreateVertexDeclaration(elements);
      });

      CreateBuffers(1 << 16); // maximum size that will fit in a ushort; this is currently ultra-inefficient, figure out a better way to update
    }

    RendererRHI::~RendererRHI() {
      ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        Frames_Destructor,
        Data *, rhi, m_rhi,
      {
        delete rhi; // texture contained gets cleaned up by the destructor
      });
    }

    void RendererRHI::Begin(int width, int height) {
      Renderer::Begin(width, height);

      m_currentTexture = nullptr;

      ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
        Frames_Begin,
        Data *, rhi, m_rhi,
        int, width, width,
        int, height, height,
        ERHIFeatureLevel::Type, featureLevel, m_featureLevel,
      {
        TShaderMapRef<FFramesVS> VertexShader(GetGlobalShaderMap_Shim(featureLevel));
        TShaderMapRef<FFramesPS> PixelShader(GetGlobalShaderMap_Shim(featureLevel));

        static FGlobalBoundShaderState boundShaderState;
        SetGlobalBoundShaderState_Shim(RHICmdList, featureLevel, boundShaderState, rhi->m_vertexDecl, *VertexShader, *PixelShader);
        
        VertexShader->SetParameterSize(RHICmdList, width, height);
        PixelShader->SetParameterTexture(RHICmdList, 0, false);

        RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
        RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
        RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
      });
    }

    void RendererRHI::End() {
      FlushRequestData();

      ENQUEUE_UNIQUE_RENDER_COMMAND(
        Frames_End,
      {
        // DX11 doesn't seem to require this, but OpenGL demands it
        RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
      });
    }

    Renderer::Vertex *RendererRHI::Request(int quads) {
      if (quads > m_verticesQuadcount) {
        EnvironmentGet()->LogError("Exceeded valid quad count in a single draw call; splitting NYI");
        return 0;
      }

      if (!m_request) {
        m_request = new RequestData;
      }

      int preSize = m_request->data.size();
      m_request->data.resize(preSize + quads * 4);

      return m_request->data.data() + preSize;
    }

    void RendererRHI::Return(int quads /*= -1*/) {
      if (!m_request)
      {
        EnvironmentGet()->LogError("Return called without inflight request");
        return;
      }

      if (quads != -1) {
        // truncate the vector based on quads
        m_request->quads += quads;
        m_request->data.resize(m_request->quads * 4);
      } else {
        // update quads based on vector
        m_request->quads = m_request->data.size() / 4;
      }
    }

    TextureBackingPtr RendererRHI::TextureCreate(int width, int height, Texture::Format mode) {
      return TextureBackingPtr(new TextureBackingRHI(EnvironmentGet(), width, height, mode));
    }

    TextureBackingPtr RendererRHI::TextureCreate(const Texture::ContextualPtr &contextual) {
      UE4TextureContextual *ue4tc = static_cast<UE4TextureContextual *>(contextual.Get());
      return TextureBackingPtr(new TextureBackingRHI(EnvironmentGet(), ue4tc->m_tex));
    }

    void RendererRHI::TextureSet(const detail::TextureBackingPtr &tex) {
      TextureBackingRHI *backing = tex.Get() ? static_cast<TextureBackingRHI*>(tex.Get()) : 0;
      if (m_currentTexture != backing) {
        FlushRequestData();

        m_currentTexture = backing;
        
        ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(
          Frames_TextureSet,
          Data *, rhi, m_rhi,
          TextureBackingRHI::Data *, tex, m_currentTexture ? m_currentTexture->DataGet() : 0,
          Texture::Format, format, m_currentTexture ? m_currentTexture->FormatGet() : Texture::FORMAT_R_8,  // fallback value is irrelevant
          ERHIFeatureLevel::Type, featureLevel, m_featureLevel,
        {
          TShaderMapRef<FFramesPS> PixelShader(GetGlobalShaderMap_Shim(featureLevel));

          PixelShader->SetParameterTexture(RHICmdList, tex ? tex->m_tex : 0, format == Texture::FORMAT_R_8);
        });
      }
    }

    void RendererRHI::ScissorSet(const Rect &rect) {
      ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        Frames_ScissorSet,
        Rect, rect, rect,
      {
        RHICmdList.SetScissorRect(true, (int)floor(rect.s.x + 0.5f), (int)floor(rect.s.y + 0.5f), (int)floor(rect.e.x + 0.5f), (int)floor(rect.e.y + 0.5f));
      });
    }

    void RendererRHI::CreateBuffers(int len) {
      ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        Frames_TextureSet,
        Data *, rhi, m_rhi,
        int, len, len,
      {
        int quadLen = len / 4;
        
        {
          vector<unsigned short> elements(quadLen * 6);
          int writepos = 0;
          for (int i = 0; i < quadLen; ++i) {
            elements[writepos++] = i * 4 + 0;
            elements[writepos++] = i * 4 + 1;
            elements[writepos++] = i * 4 + 3;
            elements[writepos++] = i * 4 + 1;
            elements[writepos++] = i * 4 + 2;
            elements[writepos++] = i * 4 + 3;
          }

          FRHIResourceCreateInfo cinfo;
          rhi->m_indices = RHICreateIndexBuffer(sizeof(unsigned short), elements.size() * sizeof(unsigned short), BUF_Static, cinfo);

           void *data = RHILockIndexBuffer(rhi->m_indices, 0, elements.size() * sizeof(unsigned short), RLM_WriteOnly);
          memcpy(data, elements.data(), elements.size() * sizeof(unsigned short));
          RHIUnlockIndexBuffer(rhi->m_indices);
        }
      });
      
      m_verticesQuadcount = len / 4;
    }

    void RendererRHI::FlushRequestData() {
      if (!m_request) {
        return;
      }

      if (!m_request->quads) {
        delete m_request;
        m_request = nullptr;
        return;
      }

      ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        Frames_Begin,
        Data *, rhi, m_rhi,
        RequestData *, request, m_request,
      {
        {
          FVertexBufferRHIParamRef vertexBuffer = rhi->GetVertexBuffer(request->quads * 4);

          void *data = RHILockVertexBuffer(vertexBuffer, 0, request->quads * sizeof(Vertex) * 4, RLM_WriteOnly);
        
          memcpy(data, request->data.data(), request->quads * sizeof(Vertex) * 4);

          RHIUnlockVertexBuffer(vertexBuffer);

          RHICmdList.SetStreamSource(0, vertexBuffer, sizeof(Renderer::Vertex), 0);
        }

        RHICmdList.DrawIndexedPrimitive(rhi->m_indices, PT_TriangleList, 0, 0, request->quads * 4, 0, request->quads * 2, 1);

        delete request;
      });
      
      m_request = nullptr; // will be cleaned up in render thread
    }

    FVertexBufferRHIParamRef RendererRHI::Data::GetVertexBuffer(int size) {
      int targetSize = size;

      // Round to next power of two
      targetSize--;
      targetSize |= targetSize >> 1;
      targetSize |= targetSize >> 2;
      targetSize |= targetSize >> 4;
      targetSize |= targetSize >> 8;
      targetSize |= targetSize >> 16;
      targetSize++;

      {
        // We want to allow things that are, in general, 3*2^n also, so:
        // divide by four, subtract, compare
        int testSize = targetSize >> 2;
        if (targetSize - testSize >= size) {
          targetSize = targetSize - testSize;
        }
      }

      if (targetSize <= 0) {
        Configuration::Get().LoggerGet()->LogError("Nonpositive target size in GetVertexBuffer; recovering");
        targetSize = 1;
      }

      // Create if it doesn't yet exist
      if (!m_vertices.count(targetSize)) {
        FRHIResourceCreateInfo cinfo;
        m_vertices[targetSize] = RHICreateVertexBuffer(targetSize * sizeof(Vertex), BUF_Volatile, cinfo);
      }

      return m_vertices[targetSize];
    }

    RendererRHI::RequestData::RequestData() : quads(0), data(0) { }
    RendererRHI::RequestData::~RequestData() { }
  }
}
