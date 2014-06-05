
#ifndef FRAMES_TEST_LIB_DX11
#define FRAMES_TEST_LIB_DX11

#include "lib.h"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

class TestWindowDX11 : public TestWindow {
public:
  TestWindowDX11(int width, int height);
  ~TestWindowDX11();

  virtual void Swap();
  virtual void HandleEvents();

  virtual Frames::Configuration::RendererPtr RendererGet();

  virtual void ClearRenderTarget();
  virtual std::vector<unsigned char> Screenshot();

private:
  HWND m_window;
  IDXGISwapChain *m_swap;
  ID3D11Device *m_device;
  ID3D11DeviceContext *m_context;
  ID3D11Texture2D *m_backBuffer;
  ID3D11Texture2D *m_captureBuffer;
  ID3D11RenderTargetView *m_renderTarget;
};

#endif