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

#ifndef TEXTURE_CONFIG
#define TEXTURE_CONFIG

#include "frames/ptr.h"

namespace Frames {
  class Environment;

  class Texture;
  typedef Ptr<Texture> TexturePtr;

  /// Represents the raw data forming a graphics texture.
  /** Used to give data to Frames so Frames can create a texture out of it. This class is intended to provide data in multiple forms, from raw binary to highly compressed hardware textures.
  
  This is currently a temporary class and can be used only to provide initialization data for a texture. */
  class Texture : public Refcountable<Texture> {
  public:
    // ---- Types

    /// Pixel format. Not all formats make sense with all Types.
    enum Format {
      FORMAT_RGBA_8, ///< Color data with alpha channel. 8 bits per channel, 32 bits per pixel. Laid out as RGBA.
      FORMAT_RGB_8, ///< Color data, no alpha channel. 8 bits per channel, 24 bits per pixel. Laid out as RGB. Packed. Will be converted to RGBA for actual textures. Probably slower than FORMAT_RGBA.
      FORMAT_R_8, ///< Red only. 8 bits per channel, 8 bits per pixel.
      FORMAT_COUNT,
    };

    /// Generic container class used for renderer-specific information.
    class Contextual : public Refcountable<Contextual> {
    public:
      Contextual() { }
      virtual ~Contextual() { }
    };
    typedef Ptr<Contextual> ContextualPtr;

    // ---- Creation

    /// Creates a Texture with a given width, height, and format, with the Texture entirely responsible for allocation and deallocation.
    /** Texture will be created containing all 0 values. Use RawDataGet() to modify the data before using this Texture. */
    static TexturePtr CreateRawManaged(Environment *env, int width, int height, Format format);
    /// Creates a Texture with a given width, height, and format, referencing an existing block of data.
    /** If takeOwnership is true, the Texture will automatically deallocate data using the Environment's allocator. */
    static TexturePtr CreateRawUnmanaged(Environment *env, int width, int height, Format format, unsigned char *data, int stride, bool takeOwnership = false);

    /// Creates a Texture with a given width, height, and format, referencing a renderer-specific data package.
    static TexturePtr CreateContextual(Environment *env, int width, int height, Format format, ContextualPtr data);

    // ---- Generic data

    /// Texture type.
    enum Type {
      NIL, ///< Invalid texture used as a default value.
      RAW, ///< Raw byte array containing decompressed image data in scanline order.
      CONTEXTUAL, ///< Context-sensitive type defined by the renderer. Read your renderer documentation for details.
    };
    /// Returns the Type.
    Type TypeGet() const { return m_type; }

    /// Returns the \ref Format "pixel format".
    Format FormatGet() const { return m_format; }
    /// Returns the width in pixels.
    int WidthGet() const { return m_width; }
    /// Returns the height in pixels.
    int HeightGet() const { return m_height; }
    
    // ---- Raw accessors

    /// Returns the raw data.
    /** Undefined results if this Texture type is not RAW. */
    unsigned char *RawDataGet();
    /// Returns the raw data.
    /** Undefined results if this Texture type is not RAW. */
    const unsigned char *RawDataGet() const;
    /// Returns the raw data's stride.
    /** Stride is the memory offset, in bytes, between rows of the texture. On densely-packed textures this will be WidthGet() * RawBPPGet(FormatGet()). It may be larger on textures with row padding.
    
    Undefined results if this Texture type is not RAW. */
    int RawStrideGet() const;

    // ---- Renderer-specific accessors

    const ContextualPtr &ContextualGet() const;

    // ---- Helper functions

    /// Returns the number of bytes per pixel for a given Format, assuming Raw texture type.
    static int RawBPPGet(Format format);
    
  private:
    friend class Refcountable<Texture>;

    Texture();
    ~Texture();

    Type m_type;

    Format m_format;
    int m_width;
    int m_height;

    unsigned char *m_raw_data;
    int m_raw_stride;
    bool m_raw_owned;

    ContextualPtr m_contextual;

    Environment *m_env;
  };
}

#endif

