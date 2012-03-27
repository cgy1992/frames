// Renderer core

#ifndef FRAMES_RENDERER
#define FRAMES_RENDERER

#include <queue>

#include "frames/color.h"
#include "frames/rect.h"

namespace Frames {
  // fake opengl typedefs, used so we don't pull the entire header in
  typedef unsigned int GLuint;
  typedef unsigned short GLushort;
  typedef float GLfloat;

  class Environment;
  class Rect;
  class TextureBacking;
  class TextureChunk;

  class Renderer {
  public:
    struct Vertex {
      Point p;
      Point t;
      Color c;
    };

    Renderer(Environment *env);
    ~Renderer();

    void Begin(int width, int height);
    void End();

    Vertex *Request(int vertices);
    void Return(int mode, int count = -1);  // also renders, count lets you optionally specify the number of vertices

    void SetTexture();
    void SetTexture(TextureChunk *tex);
    void SetTexture(TextureBacking *tex);

    void PushScissor(const Rect &rect);
    void PopScissor();
    
    static bool WriteCroppedRect(Vertex *vertex, const Rect &screen, const Color &color, const Rect &bounds); // no fancy lerping
    static bool WriteCroppedTexRect(Vertex *vertex, const Rect &screen, const Rect &tex, const Color &color, const Rect &bounds);  // fancy lerping

  private:
    Environment *m_env; // just for debug functionality

    int m_width;
    int m_height;

    GLuint m_buffer;
    GLuint m_buffer_pos;
    
    GLuint m_last_pos;
    GLuint m_last_vertices;

    GLuint m_elements;

    void Internal_SetTexture(GLuint tex);
    GLuint m_currentTexture;

    void SetScissor(const Rect &rect);
    std::queue<Rect> m_scissor;
  };
};

#endif
