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

#include "lib.h"

#include "lib_opengl.h"
#include "lib_dx11.h"
#include "lib_null.h"

#include <gtest/gtest.h>

#include <SDL.h>
#include <frames/detail_format.h>
#include <frames/environment.h>
#include <frames/event.h>
#include <frames/layout.h>
#include <frames/loader.h>
#include <frames/stream.h>
#include <frames/texture.h>

#include <png.h>
#include <zlib.h>

#include <cstdio>
#include <fstream>

void ClampScreenshotAlpha(std::vector<unsigned char> *pixels) {
  for (int i = 3; i < (int)pixels->size(); i += 4) {
    (*pixels)[i] = 255;
  }
}

struct TestNames {
  std::string testName;
  std::string resultName;
  std::string diffName;
};

static std::string s_testNameLast = "";
static std::map<std::string, int> s_familyIds;

TestNames GetTestNames(const std::string &family, const std::string &extension) {
  TestNames rv;

  std::string baseName = Frames::detail::Format("ref/%s_%s", ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name(), ::testing::UnitTest::GetInstance()->current_test_info()->name());

  if (baseName != s_testNameLast) {
    s_testNameLast = baseName;
    s_familyIds.clear();
  }

  std::string testFilePrefix = Frames::detail::Format("%s_%s%s%d", baseName, family, family.empty() ? "" : "_", s_familyIds[family]++);

  rv.testName = testFilePrefix + "_ref%d" + extension;
  rv.resultName = testFilePrefix + "_result" + extension;
  rv.diffName = testFilePrefix + "_diff" + extension;

  // write to the "input" file if that file doesn't exist
  std::string master = Frames::detail::Format(rv.testName.c_str(), 0);
  if (!std::ifstream(master.c_str())) {
    rv.resultName = master;
  }

  return rv;
}

void ResetTestNames() {
  s_testNameLast = "";
  s_familyIds.clear();
}

TestCompare::TestCompare(const std::string &suffix /*= ""*/) : m_suffix(suffix) {
}
TestCompare::~TestCompare() {
  TestNames testNames = GetTestNames(m_suffix, ".txt");

  // Multiple valid refs, NYI
  std::string tname = Frames::detail::Format(testNames.testName.c_str(), 0);

  // Grab our source file (or try to)
  std::string testsrc;
  bool existed = false;
  {
    std::ifstream in(tname.c_str(), std::ios::binary); // Binary so we don't have to worry about \r\n's showing up in our event results
    if (in) {
      existed = true;
      testsrc = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
  }

  // Write result to disk
  if (existed || !m_records.empty()) {
    std::ofstream out(testNames.resultName.c_str(), std::ios::binary);
    if (out) {
      out << m_records;
    } else {
      ADD_FAILURE() << "Cannot write result " << testNames.resultName;
    }
  }

  EXPECT_TRUE(m_records == testsrc);
}

void TestCompare::Append(const std::string &text) {
  m_records += text;
  m_records += "\n";
}

TestLogger::TestLogger() : m_allowErrors(false), m_seenErrors(false), m_compare("error") { }
TestLogger::~TestLogger() {
  if (m_allowErrors) {
    EXPECT_TRUE(m_seenErrors);
  }
}

void TestLogger::LogError(const std::string &log) {
  if (!m_allowErrors) {
    GTEST_FAIL() << log;
  } else {
    printf("[ERR-EXPCT] %s\n", log.c_str());
    m_compare.Append(log);
  }

  m_seenErrors = true;
}

void TestLogger::LogDebug(const std::string &log) {
  printf("[DBG] %s\n", log.c_str());
}

void TestLogger::AllowErrors() {
  m_allowErrors = true;
}

class TestPathMunger : public Frames::Configuration::PathFromId {
public:
  virtual std::string Process(Frames::Environment *env, const std::string &id) FRAMES_OVERRIDE {
    if (strstr(id.c_str(), "data/")) {
      // horrible hack to deal with built-in fonts
      return std::string("c:/windows/fonts") + (id.c_str() + 4);
    } else {
      return "resources/" + id;
    }
  }
};

TestEnvironment::TestEnvironment(bool startUI, int width, int height) : m_env(0), m_tenv(0) {
  if (startUI) {
    if (RendererIdGet() == "ogl3_2_core") {
      m_tenv = new TestWindowSDL(width, height, 3, 2, SDL_GL_CONTEXT_PROFILE_CORE);
    } else if (RendererIdGet() == "ogl3_2_compat") {
      m_tenv = new TestWindowSDL(width, height, 3, 2, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    } else if (RendererIdGet() == "dx11_fl10") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_10_0, TestWindowDX11::MODE_HAL);
    } else if (RendererIdGet() == "dx11_fl10_dbg") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_10_0, TestWindowDX11::MODE_DEBUG);
    } else if (RendererIdGet() == "dx11_fl10_ref") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_10_0, TestWindowDX11::MODE_REFERENCE);
    } else if (RendererIdGet() == "dx11_fl11") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_11_0, TestWindowDX11::MODE_HAL);
    } else if (RendererIdGet() == "dx11_fl11_dbg") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_11_0, TestWindowDX11::MODE_DEBUG);
    } else if (RendererIdGet() == "dx11_fl11_ref") {
      m_tenv = new TestWindowDX11(width, height, D3D_FEATURE_LEVEL_11_0, TestWindowDX11::MODE_REFERENCE);
    } else if (RendererIdGet() == "null") {
      m_tenv = new TestWindowNull(width, height);
    } else {
      ADD_FAILURE() << Frames::detail::Format("Invalid gtest renderer flag %s", RendererIdGet().c_str());
    }
  } else {
    m_tenv = new TestWindowNull(width, height);
  }

  m_logger = Frames::Ptr<TestLogger>(new TestLogger());

  Frames::Configuration::Global cglobal;
  cglobal.LoggerSet(m_logger);
  Frames::Configuration::Set(cglobal);

  Frames::Configuration::Local config;
  config.FontDefaultIdSet("LindenHill.otf");
  config.LoggerSet(m_logger);
  config.PathFromIdSet(Frames::Ptr<TestPathMunger>(new TestPathMunger()));
  config.RendererSet(m_tenv->RendererGet());
  m_env = Frames::Environment::Create(config);

  m_env->ResizeRoot(WidthGet(), HeightGet()); // set this up so we can check coordinates, otherwise we'll currently assume there are no coordinates
}

TestEnvironment::~TestEnvironment() {
  m_env.Reset();
  delete m_tenv;

  // Reset the configuration system so the logger goes out of scope
  Frames::Configuration::Set(Frames::Configuration::Global());
}

int TestEnvironment::WidthGet() const {
  return m_tenv->WidthGet();
}

int TestEnvironment::HeightGet() const {
  return m_tenv->HeightGet();
}

void TestEnvironment::Swap() {
  return m_tenv->Swap();
}

void TestEnvironment::HandleEvents() {
  return m_tenv->HandleEvents();
}

void TestEnvironment::AllowErrors() {
  m_logger->AllowErrors();
}

VerbLog::VerbLog(TestCompare *compare, const std::string &descr /*= ""*/) : m_compare(compare) {
  if (!descr.empty()) {
    m_prefix = descr + ": ";
  }

  m_compare->Append(m_prefix + "Begin log"); // an empty record vector is treated specially
}

VerbLog::~VerbLog() {
  for (int i = 0; i < (int)m_detachers.size(); ++i) {
    delete m_detachers[i];
  }
}

void VerbLog::RecordEvent(Frames::Handle *handle) {
  RecordResult(handle, "");
}

void VerbLog::RecordEvent(Frames::Handle *handle, int p1) {
  RecordResult(handle, Frames::detail::Format("%d", p1));
}

void VerbLog::RecordEvent(Frames::Handle *handle, const Frames::Vector &p1) {
  RecordResult(handle, Frames::detail::Format("%s", p1));
}

void VerbLog::RecordEvent(Frames::Handle *handle, Frames::Input::Key p1) {
  RecordResult(handle, Frames::detail::Format("%s", Frames::Input::StringFromKey(p1)));
}

void VerbLog::RecordEvent(Frames::Handle *handle, const std::string &p1) {
  RecordResult(handle, Frames::detail::Format("%s", p1));
}

void VerbLog::RecordResult(Frames::Handle *handle, const std::string &params) {
  std::string current;
  if (handle->TargetGet() != handle->TargetContextGet() || handle->VerbGet() != handle->VerbContextGet()) {
    current = Frames::detail::Format(" (currently %s on %s)", handle->VerbContextGet()->NameGet(), handle->TargetContextGet()->DebugNameGet());
  }

  std::string param;
  if (!params.empty()) {
    param = Frames::detail::Format(" (%s)", params);
  }

  m_compare->Append(Frames::detail::Format("%sEvent %s%s on %s%s", m_prefix, handle->VerbGet()->NameGet(), param, handle->TargetGet()->DebugNameGet(), current));
}

void WritePng(const std::string &filename, const std::vector<unsigned char> &data, int width, int height) {
  FILE *fp = fopen(filename.c_str(), "wb");

  if (fp) {
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
    png_set_filter(png_ptr, 0, PNG_ALL_FILTERS);

    png_set_IHDR(png_ptr, info_ptr, width, height,
      8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
      PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    std::vector<const unsigned char *> rows;
    for (int i = 0; i < height; ++i) {
      rows.push_back(&data[0] + i * width * 4);
    }

    png_write_image(png_ptr, (png_bytepp)&rows[0]); // come on, why couldn't png_bytepp have been const

    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);
  } else {
    ADD_FAILURE() << "Cannot write result " << filename;
  }
}

void TestSnapshot(TestEnvironment &env, const SnapshotConfig &csf) {
  // Do the render
  env.ClearRenderTarget();
  env->ResizeRoot(env.WidthGet(), env.HeightGet());
  env->Render();

  TestNames testNames;
  if (csf.FileGet().empty()) {
    testNames = GetTestNames("screen", ".png");
  } else {
    testNames.testName = csf.FileGet() + "_ref%d.png";
    std::string master = Frames::detail::Format(testNames.testName.c_str(), 0);
    // write to the "input" file if that file doesn't exist
    if (!std::ifstream(master.c_str())) {
      testNames.resultName = master;
    } else {
      testNames.resultName = csf.FileGet() + "_result" + ".png";
    }
    testNames.diffName = csf.FileGet() + "_diff" + ".png";
  }
  
  // We now have our test filename

  // Grab a screenshot
  std::vector<unsigned char> pixels = env.Screenshot();

  if (pixels.empty() && RendererIdGet() == "null") {
    return; // jolly good, then
  }

  // Write result to disk
  WritePng(testNames.resultName, pixels, env.WidthGet(), env.HeightGet());

  // Grab our source file (or try to)
  int epsilon = std::numeric_limits<int>::max();
  int critical = std::numeric_limits<int>::max();
  std::multiset<int> diffs;
  std::vector<unsigned char> diffdata;
  std::string match;
  {
    int refid = 0;
    while (true) {
      std::string fname = Frames::detail::Format(testNames.testName.c_str(), refid);
      std::vector<unsigned char> reference;
      Frames::StreamPtr stream = Frames::StreamFile::Create(fname);
      if (stream) {
        Frames::TexturePtr tex = Frames::Loader::PNG::Load(*env, stream);
        EXPECT_EQ(Frames::Texture::RAW, tex->TypeGet());
        EXPECT_EQ(Frames::Texture::FORMAT_RGBA_8, tex->FormatGet());
        EXPECT_EQ(4, Frames::Texture::RawBPPGet(Frames::Texture::FORMAT_RGBA_8));
        EXPECT_EQ(tex->WidthGet() * 4, tex->RawStrideGet());
        reference.resize(tex->WidthGet() * tex->HeightGet() * 4);
        memcpy(&reference[0], tex->RawDataGet(), tex->WidthGet() * tex->HeightGet() * 4);
      }

      if (reference.empty()) {
        break;
      }

      EXPECT_EQ(pixels.size(), reference.size()) << " - wrong size on " << fname;

      if (reference.size() == pixels.size()) {
        int tepsilon = 0;
        int tcritical = 0;
        std::multiset<int> tdiffs;
        std::vector<unsigned char> tdiffdata;
        tdiffdata.resize(pixels.size());

        for (int i = 0; i < (int)pixels.size(); ++i) {
          const int tpixel = (int)pixels[i];
          int diff = abs(tpixel - (int)reference[i]);
          bool shifted = false;

          if (diff && csf.NearestGet()) {
            // This is a bit wacky - we define a range as the minimum and maximum values of this pixel and all adjacent pixels, then compare the actual pixel to the range
            // This is so we avoid a situation where a pixel ends up being at a midpoint in a sharp gradient, but because it's not close to any of the pixels involved in that gradient, it's flagged as invalid
            int low = reference[i];
            int high = reference[i];
            const int dx[] = {0, 0, 1, -1};
            const int dy[] = {1, -1, 0, 0};
            for (int ofs = 0; ofs < 4; ++ofs) {
              int target = i + (dx[ofs] + dy[ofs] * env.WidthGet()) * 4;
              if (target >= 0 && target < (int)reference.size()) {
                low = std::min(low, (int)reference[target]);
                high = std::max(high, (int)reference[target]);
              }
            }
            int tdiff = std::min(abs(tpixel - low), abs(tpixel - high));
            if (pixels[i] >= low && pixels[i] <= high) {
              tdiff = 0;
            }
            if (tdiff < diff) {
              diff = tdiff;
              shifted = true;
            }
          }
          
          tdiffdata[i] = (unsigned char)std::min(diff * 4, 255);
          if (diff > csf.DeltaGet()) {
            tcritical++;
            tdiffs.insert(diff);
            if (tdiffs.size() > 10) {
              tdiffs.erase(tdiffs.begin());
            }
          } else if (diff || shifted) {
            tepsilon++;
          }
        }

        if (tcritical < critical || tcritical == critical && tepsilon < epsilon) {
          epsilon = tepsilon;
          critical = tcritical;
          diffs = tdiffs;
          diffdata = tdiffdata;
          match = fname;
        }
      }

      ++refid;
    }
  }

  EXPECT_FALSE(match.empty());

  if (epsilon || critical) {
    ClampScreenshotAlpha(&diffdata);
    WritePng(testNames.diffName, diffdata, env.WidthGet(), env.HeightGet());
  }

  if (epsilon) {
    GTEST_LOG_(WARNING) << epsilon << " epsilon pixels compared to " << match;
  }
  EXPECT_EQ(0, critical) << "Critical pixels detected, compared to " << match;
  if (!diffs.empty()) {
    std::string tdiffs = "Pixel diffs: ";
    for (std::multiset<int>::const_iterator itr = diffs.begin(); itr != diffs.end(); ++itr) {
      if (itr != diffs.begin()) {
        tdiffs += ", ";
      }
      tdiffs += Frames::detail::Format("%d", *itr);
    }
    printf("%s\n", tdiffs.c_str());
  }
}

void HaltAndRender(TestEnvironment &env) {
  while (true) {
    // Do the render
    env.ClearRenderTarget();
    env->ResizeRoot(env.WidthGet(), env.HeightGet());
    env->Render();
    env.Swap();
    env.HandleEvents();
  }
}

static std::string s_renderer;
void RendererIdSet(const std::string &renderer) {
  s_renderer = renderer;
  ResetTestNames();
}
std::string &RendererIdGet() {
  return s_renderer;
}
