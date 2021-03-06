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

#include "lib_opengl.h"

#include <frames/renderer_opengl.h>

#define GLEW_STATIC
#include <GL/GLew.h>

TestWindowSDL::TestWindowSDL(int width, int height, int major, int minor, int profile) : TestWindow(width, height), m_win(0), m_glContext(0) {
  EXPECT_EQ(0, SDL_Init(SDL_INIT_VIDEO));

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  m_win = SDL_CreateWindow("Frames test harness", 100, 100, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  EXPECT_TRUE(m_win != NULL);

  m_glContext = SDL_GL_CreateContext(m_win);
  EXPECT_TRUE(m_glContext != 0);
}

TestWindowSDL::~TestWindowSDL() {
  EXPECT_EQ(GL_NO_ERROR, glGetError()); // verify no GL issues

  if (m_glContext) {
    SDL_GL_DeleteContext(m_glContext);
  }

  if (m_win) {
    SDL_DestroyWindow(m_win);
  }

  SDL_Quit();
}

void TestWindowSDL::Swap() {
  SDL_GL_SwapWindow(m_win);
}

void TestWindowSDL::HandleEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // We actually just ignore all events
  }
}

Frames::Configuration::RendererPtr TestWindowSDL::RendererGet() {
  return Frames::Configuration::RendererOpengl();
}

void TestWindowSDL::ClearRenderTarget() {
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
}

std::vector<unsigned char> TestWindowSDL::Screenshot() {
  std::vector<unsigned char> pixels;
  pixels.resize(4 * WidthGet() * HeightGet());

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glReadPixels(0, 0, WidthGet(), HeightGet(), GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
  EXPECT_EQ(GL_NO_ERROR, glGetError());

  // Annoyingly, OpenGL reads coordinates in math quadrant order, not scanline order like the rest of the civilized computer world
  // So . . . go ahead and invert the entire array
  for (int y = 0; y < HeightGet() / 2; ++y) {
    for (int x = 0; x < WidthGet() * 4; ++x) {
      std::swap(pixels[y * WidthGet() * 4 + x], pixels[(HeightGet() - 1 - y) * WidthGet() * 4 + x]);
    }
  }

  ClampScreenshotAlpha(&pixels);

  return pixels;
}