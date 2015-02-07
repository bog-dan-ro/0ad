/* Copyright (c) 2015 Wildfire Games
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * support routines for 2d texture access/writing.
 */

#include "precompiled.h"
#include "tex.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>

#include "lib/timer.h"
#include "lib/bits.h"
#include "lib/ogl.h"
#include "lib/allocators/shared_ptr.h"
#include "lib/sysdep/cpu.h"

#include "tex_codec.h"


static const StatusDefinition texStatusDefinitions[] = {
	{ ERR::TEX_FMT_INVALID, L"Invalid/unsupported texture format", 0 },
	{ ERR::TEX_INVALID_COLOR_TYPE, L"Invalid color type", 0 },
	{ ERR::TEX_NOT_8BIT_PRECISION, L"Not 8-bit channel precision", 0 },
	{ ERR::TEX_INVALID_LAYOUT, L"Unsupported texel layout, e.g. right-to-left", 0 },
	{ ERR::TEX_COMPRESSED, L"Unsupported texture compression", 0 },
	{ WARN::TEX_INVALID_DATA, L"Warning: invalid texel data encountered", 0 },
	{ ERR::TEX_INVALID_SIZE, L"Texture size is incorrect", 0 },
	{ INFO::TEX_CODEC_CANNOT_HANDLE, L"Texture codec cannot handle the given format", 0 }
};
STATUS_ADD_DEFINITIONS(texStatusDefinitions);


//-----------------------------------------------------------------------------
// validation
//-----------------------------------------------------------------------------

// be careful not to use other tex_* APIs here because they call us.

Status Tex::validate() const
{
	if(m_Flags & TEX_UNDEFINED_FLAGS)
		WARN_RETURN(ERR::_1);

	// pixel data (only check validity if the image is still in memory;
	// ogl_tex frees the data after uploading to GL)
	if(m_Data)
	{
		// file size smaller than header+pixels.
		// possible causes: texture file header is invalid,
		// or file wasn't loaded completely.
		if(m_DataSize < m_Ofs + m_Width*m_Height*m_Bpp/8)
			WARN_RETURN(ERR::_2);
	}

	// bits per pixel
	// (we don't bother checking all values; a sanity check is enough)
	if(m_Bpp % 4 || m_Bpp > 32)
		WARN_RETURN(ERR::_3);

	// flags
	if ((!m_glType && !m_glInternalFormat) || !m_glFormat)
		WARN_RETURN(ERR::_4);

	// .. orientation
	const size_t orientation = m_Flags & TEX_ORIENTATION;
	if(orientation == (TEX_BOTTOM_UP|TEX_TOP_DOWN))
		WARN_RETURN(ERR::_5);

	return INFO::OK;
}

#define CHECK_TEX(t) RETURN_STATUS_IF_ERR((t->validate()))

//-----------------------------------------------------------------------------
// mipmaps
//-----------------------------------------------------------------------------

static Status tex_util_mipmap_level(const Tex* t, size_t level, size_t face, size_t &w, size_t &h, u8 *&mipmapData, size_t &mipmapDataSize)
{
	ENSURE(level < t->m_numberOfMipmapLevels);
	ENSURE(face < t->m_numberOfFaces);

	w = t->m_Width;
	h = t->m_Height;

	mipmapData = t->get_data();

	for(size_t i = 0; i < level; i++)
	{
		// used to skip past this mip level in <data>
		const size_t level_dataSize = t->m_numberOfFaces * w * h * (t->m_Bpp / 8);

		mipmapData += level_dataSize;

		w /= 2;
		h /= 2;
		// if the texture is non-square, one of the dimensions will become
		// 0 before the other. to satisfy OpenGL's expectations, change it
		// back to 1.
		if(w == 0) w = 1;
		if(h == 0) h = 1;
	}

	mipmapDataSize = w * h * (t->m_Bpp / 8);
	mipmapData += face * mipmapDataSize;

	return INFO::OK;
}


void tex_util_foreach_mipmap(const Tex *t, int levels_to_skip, MipmapCB cb, void* RESTRICT cbData)
{
	ENSURE(levels_to_skip >= 0 && size_t(levels_to_skip) < t->m_numberOfMipmapLevels);

	size_t level_w, level_h;
	u8* level_data;
	size_t level_dataSize;
	for (size_t level = levels_to_skip; level < t->m_numberOfMipmapLevels; level++) {
		for (size_t face = 0; face < t->m_numberOfFaces; face++) {
			if (t->mipmapsLevel(level, face, level_w, level_h, level_data, level_dataSize) < 0)
				return;
			cb(level - levels_to_skip, face, level_w, level_h, level_data, level_dataSize, cbData);
		}
	}
}


struct CreateLevelData
{
	size_t num_components;

	size_t prev_level_w;
	size_t prev_level_h;
	const u8* prev_level_data;
	size_t prev_level_dataSize;
};

// uses 2x2 box filter
static void create_level(size_t level, size_t UNUSED(face), size_t level_w, size_t level_h, const u8* RESTRICT level_data, size_t level_dataSize, void* RESTRICT cbData)
{
	CreateLevelData* cld = (CreateLevelData*)cbData;
	const size_t src_w = cld->prev_level_w;
	const size_t src_h = cld->prev_level_h;
	const u8* src = cld->prev_level_data;
	u8* dst = (u8*)level_data;

	// base level - must be copied over from source buffer
	if(level == 0)
	{
		ENSURE(level_dataSize == cld->prev_level_dataSize);
		memcpy(dst, src, level_dataSize);
	}
	else
	{
		const size_t num_components = cld->num_components;
		const size_t dx = num_components, dy = dx*src_w;

		// special case: image is too small for 2x2 filter
		if(cld->prev_level_w == 1 || cld->prev_level_h == 1)
		{
			// image is either a horizontal or vertical line.
			// their memory layout is the same (packed pixels), so no special
			// handling is needed; just pick max dimension.
			for(size_t y = 0; y < std::max(src_w, src_h); y += 2)
			{
				for(size_t i = 0; i < num_components; i++)
				{
					*dst++ = (src[0]+src[dx]+1)/2;
					src += 1;
				}

				src += dx;	// skip to next pixel (since box is 2x2)
			}
		}
		// normal
		else
		{
			for(size_t y = 0; y < src_h; y += 2)
			{
				for(size_t x = 0; x < src_w; x += 2)
				{
					for(size_t i = 0; i < num_components; i++)
					{
						*dst++ = (src[0]+src[dx]+src[dy]+src[dx+dy]+2)/4;
						src += 1;
					}

					src += dx;	// skip to next pixel (since box is 2x2)
				}

				src += dy;	// skip to next row (since box is 2x2)
			}
		}

		ENSURE(dst == level_data + level_dataSize);
		ENSURE(src == cld->prev_level_data + cld->prev_level_dataSize);
	}

	cld->prev_level_data = level_data;
	cld->prev_level_dataSize = level_dataSize;
	cld->prev_level_w = level_w;
	cld->prev_level_h = level_h;
}


static Status add_mipmaps(Tex* t)
{
	ENSURE(t->m_glType);
	// this code assumes the image is of POT dimension; we don't
	// go to the trouble of implementing image scaling because
	// the only place this is used (ogl_tex_upload) requires POT anyway.
	const size_t w = t->m_Width, h = t->m_Height, bpp = t->m_Bpp;
	if(!is_pow2(w) || !is_pow2(h))
		WARN_RETURN(ERR::TEX_INVALID_SIZE);

	t->m_numberOfMipmapLevels = ceil_log2(std::max(t->m_Width, t->m_Height)) + 1;
	const size_t mipmap_size = t->img_size();
	shared_ptr<u8> mipmapData;
	AllocateAligned(mipmapData, mipmap_size);
	CreateLevelData cld = { bpp/8, w, h, t->m_Data.get(), t->m_DataSize };
	t->m_Data = mipmapData;
	t->m_DataSize = mipmap_size;
	t->m_Ofs = 0;
	tex_util_foreach_mipmap(t, 0, create_level, &cld);
	return INFO::OK;
}

Tex::Tex()
{
	m_glType = 0;
	m_glFormat = 0;
	m_glInternalFormat = 0;
	m_numberOfMipmapLevels = 1;
	m_numberOfFaces = 1;
	m_mipmapsLevel = &tex_util_mipmap_level;
}

bool Tex::hasAlpha(size_t glFormat)
{
	switch (glFormat) {
	case GL_ALPHA:
	case GL_LUMINANCE_ALPHA:
	case GL_RGBA:
	case GL_BGRA_EXT:
		return true;
	}
	return false;
}

size_t Tex::glFormatWithAlpha() const
{
	switch (m_glFormat) {
	case GL_RGB:
	case GL_RGBA:
		return GL_RGBA;
	case GL_BGR:
	case GL_BGRA_EXT:
		return GL_BGRA_EXT;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// pixel format conversion (transformation)
//-----------------------------------------------------------------------------

TIMER_ADD_CLIENT(tc_plain_transform);

// handles BGR and row flipping in "plain" format (see below).
//
// called by codecs after they get their format-specific transforms out of
// the way. note that this approach requires several passes over the image,
// but is much easier to maintain than providing all<->all conversion paths.
//
// somewhat optimized (loops are hoisted, cache associativity accounted for)

static void flipTexture(Tex* t)
{
	const size_t w = t->m_Width, h = t->m_Height, bpp = t->m_Bpp;
	u8* const srcStorage = t->get_data();
	const size_t srcSize = t->img_size();
	shared_ptr<u8> dstStorage;
	AllocateAligned(dstStorage, srcSize);

	// setup row source/destination pointers (simplifies outer loop)
	u8* dst = (u8*)dstStorage.get();
	const u8* src;
	const size_t pitch = w * bpp / 8;	// source bpp (not necessarily dest bpp)
	// .. avoid y*pitch multiply in row loop; instead, add row_ofs.
	ssize_t row_ofs = (ssize_t)pitch;

	src = (const u8*)srcStorage + srcSize - pitch;	// last row
	row_ofs = -(ssize_t)pitch;
	for(size_t y = 0; y < h; y++)
	{
		memcpy(dst, src, pitch);
		dst += pitch;
		src += row_ofs;
	}
	t->m_Data = dstStorage;
	t->m_Ofs = 0;
}

static Status rgb2brg(Tex* t, size_t glFormat)
{
	// extract texture info
	const size_t w = t->m_Width, h = t->m_Height;
	u8* const srcStorage = t->get_data();
	const size_t srcSize = t->img_size();
	size_t dstSize = srcSize;
	shared_ptr<u8> dstStorage;
	AllocateAligned(dstStorage, dstSize);

	// setup row source/destination pointers (simplifies outer loop)
	u8* dst = (u8*)dstStorage.get();
	const u8* src = (const u8*)dstStorage.get();
	memcpy(dst, srcStorage, srcSize);

	if(t->m_Bpp == 24)
	{
		for(size_t y = 0; y < h; y++)
		{
			for(size_t x = 0; x < w; x++)
			{
				// need temporaries in case src == dst (i.e. not flipping)
				const u8 b = src[0], g = src[1], r = src[2];
				dst[0] = r; dst[1] = g; dst[2] = b;
				dst += 3;
				src += 3;
			}
		}
	}
	// RGBA <-> BGRA
	else if(t->m_Bpp == 32)
	{
		for(size_t y = 0; y < h; y++)
		{
			for(size_t x = 0; x < w; x++)
			{
				// need temporaries in case src == dst (i.e. not flipping)
				const u8 b = src[0], g = src[1], r = src[2], a = src[3];
				dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
				dst += 4;
				src += 4;
			}
		}
	}
	else
	{
		debug_warn(L"unsupported transform");
		return INFO::TEX_CODEC_CANNOT_HANDLE;
	}
	t->m_glFormat = glFormat;
	t->m_Data = dstStorage;
	t->m_DataSize = dstSize;
	t->m_Ofs = 0;
	return INFO::OK;
}

static Status rgb2rgba(Tex *t)
{
	// extract texture info
	const size_t w = t->m_Width, h = t->m_Height;
	u8* const srcStorage = t->get_data();
	const size_t srcSize = t->img_size();
	size_t dstSize = (srcSize / 3) * 4;
	shared_ptr<u8> dstStorage;
	AllocateAligned(dstStorage, dstSize);

	// setup row source/destination pointers (simplifies outer loop)
	u8* dst = (u8*)dstStorage.get();
	const u8* src = (const u8*)srcStorage;

	for(size_t y = 0; y < h; y++)
	{
		for(size_t x = 0; x < w; x++)
		{
			// need temporaries in case src == dst (i.e. not flipping)
			const u8 r = src[0], g = src[1], b = src[2];
			dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 0xFF;
			dst += 4;
			src += 3;
		}
	}

	t->m_Data = dstStorage;
	t->m_DataSize = dstSize;
	t->m_Bpp = 32;
	t->m_Ofs = 0;
	return INFO::OK;
}

static Status bgr2rgba(Tex *t)
{
	// extract texture info
	const size_t w = t->m_Width, h = t->m_Height;
	u8* const srcStorage = t->get_data();
	const size_t srcSize = t->img_size();
	size_t dstSize = (srcSize / 3) * 4;
	shared_ptr<u8> dstStorage;
	AllocateAligned(dstStorage, dstSize);

	// setup row source/destination pointers (simplifies outer loop)
	u8* dst = (u8*)dstStorage.get();
	const u8* src = (const u8*)srcStorage;

	for(size_t y = 0; y < h; y++)
	{
		for(size_t x = 0; x < w; x++)
		{
			// need temporaries in case src == dst (i.e. not flipping)
			const u8 b = src[0], g = src[1], r = src[2];
			dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = 0xFF;
			dst += 4;
			src += 3;
		}
	}

	t->m_Data = dstStorage;
	t->m_DataSize = dstSize;
	t->m_Ofs = 0;
	return INFO::OK;
}

static Status plain_transform(Tex* t, size_t glFormat, int transformFlags)
{
TIMER_ACCRUE(tc_plain_transform);
	if (t->m_glFormat == glFormat && !transformFlags)
		return INFO::OK;

	if (transformFlags & TEX_ORIENTATION)
		flipTexture(t);

	if (t->m_glFormat != glFormat) {
		switch (t->m_glFormat) {
		case GL_RGB:
			switch (glFormat) {
			case GL_RGBA:
				RETURN_STATUS_IF_ERR(rgb2rgba(t));
				break;
			case GL_BGRA_EXT:
				RETURN_STATUS_IF_ERR(rgb2rgba(t));
				RETURN_STATUS_IF_ERR(rgb2brg(t, glFormat));
				break;
			}
			break;
		case GL_BGR:
			switch (glFormat) {
			case GL_RGB:
				RETURN_STATUS_IF_ERR(rgb2brg(t, glFormat));
				break;
			case GL_RGBA:
				RETURN_STATUS_IF_ERR(bgr2rgba(t));
				break;
			default:
				return INFO::TEX_CODEC_CANNOT_HANDLE;
			}
			break;

		case GL_RGBA:
			switch (glFormat) {
			case GL_BGRA_EXT:
				RETURN_STATUS_IF_ERR(rgb2brg(t, glFormat));
				break;
			default:
				return INFO::TEX_CODEC_CANNOT_HANDLE;
			}
			break;

		case GL_BGRA_EXT:
			switch (glFormat) {
			case GL_RGBA:
				RETURN_STATUS_IF_ERR(rgb2brg(t, glFormat));
				break;
			default:
				return INFO::TEX_CODEC_CANNOT_HANDLE;
			}
			break;

		default:
			return INFO::TEX_CODEC_CANNOT_HANDLE;
		}
	}

	if (transformFlags & TEX_MIPMAPS) {
		if (t->m_numberOfMipmapLevels > 1)
			t->m_numberOfMipmapLevels = 1;
		else
			RETURN_STATUS_IF_ERR(add_mipmaps(t));
	}

	t->m_glInternalFormat = t->m_glFormat;
	t->m_mipmapsLevel = 0;

	CHECK_TEX(t);
	return INFO::OK;
}


TIMER_ADD_CLIENT(tc_transform);


Status Tex::transform(size_t glFormat, int transformFlags)
{
	TIMER_ACCRUE(tc_transform);
	CHECK_TEX(this);

	if (!glFormat)
		return INFO::TEX_CODEC_CANNOT_HANDLE;

	if (m_glType) // already uncompressed
		return plain_transform(this, glFormat, transformFlags);

	return tex_codec_transform(this, glFormat, transformFlags);
}

//-----------------------------------------------------------------------------
// image orientation
//-----------------------------------------------------------------------------

// see "Default Orientation" in docs.

static int global_orientation = TEX_TOP_DOWN;

// set the orientation (either TEX_BOTTOM_UP or TEX_TOP_DOWN) to which
// all loaded images will automatically be converted
// (excepting file formats that don't specify their orientation, i.e. DDS).
void tex_set_global_orientation(int o)
{
	ENSURE(o == TEX_TOP_DOWN || o == TEX_BOTTOM_UP);
	global_orientation = o;
}


static void flip_to_global_orientation(Tex* t)
{
	// (can't use normal CHECK_TEX due to void return)
	WARN_IF_ERR(t->validate());

	size_t orientation = t->m_Flags & TEX_ORIENTATION;
	// if codec knows which way around the image is (i.e. not DDS):
	if(orientation)
	{
		// flip image if necessary
		size_t transforms = orientation ^ global_orientation;
		WARN_IF_ERR(plain_transform(t, t->m_glFormat, transforms));
	}

	// indicate image is at global orientation. this is still done even
	// if the codec doesn't know: the default orientation should be chosen
	// to make that work correctly (see "Default Orientation" in docs).
	t->m_Flags = (t->m_Flags & ~TEX_ORIENTATION) | global_orientation;

	// (can't use normal CHECK_TEX due to void return)
	WARN_IF_ERR(t->validate());
}


// indicate if the orientation specified by <src_flags> matches
// dst_orientation (if the latter is 0, then the global_orientation).
// (we ask for src_flags instead of src_orientation so callers don't
// have to mask off TEX_ORIENTATION)
bool tex_orientations_match(size_t src_flags, size_t dst_orientation)
{
	const size_t src_orientation = src_flags & TEX_ORIENTATION;
	if(dst_orientation == 0)
		dst_orientation = global_orientation;
	return (src_orientation == dst_orientation);
}


//-----------------------------------------------------------------------------
// misc. API
//-----------------------------------------------------------------------------

// indicate if <filename>'s extension is that of a texture format
// supported by Tex::load. case-insensitive.
//
// rationale: Tex::load complains if the given file is of an
// unsupported type. this API allows users to preempt that warning
// (by checking the filename themselves), and also provides for e.g.
// enumerating only images in a file picker.
// an alternative might be a flag to suppress warning about invalid files,
// but this is open to misuse.
bool tex_is_known_extension(const VfsPath& pathname)
{
	const ITexCodec* dummy;
	// found codec for it => known extension
	const OsPath extension = pathname.Extension();
	if(tex_codec_for_filename(extension, &dummy) == INFO::OK)
		return true;

	return false;
}


// store the given image data into a Tex object; this will be as if
// it had been loaded via Tex::load.
//
// rationale: support for in-memory images is necessary for
//   emulation of glCompressedTexImage2D and useful overall.
//   however, we don't want to  provide an alternate interface for each API;
//   these would have to be changed whenever fields are added to Tex.
//   instead, provide one entry point for specifying images.
//
// we need only add bookkeeping information and "wrap" it in
// our Tex struct, hence the name.
Status Tex::wrap(size_t glFormat, size_t w, size_t h, size_t bpp, size_t flags, const shared_ptr<u8>& data, size_t ofs)
{
	m_Width    = w;
	m_Height   = h;
	m_Bpp      = bpp;
	m_Flags    = flags;
	m_Data     = data;
	m_DataSize = ofs + w*h*bpp/8;
	m_Ofs      = ofs;
	m_glFormat = glFormat;
	m_glType   = GL_UNSIGNED_BYTE;
	m_glInternalFormat = glFormat;
	m_numberOfFaces = 1;
	m_numberOfMipmapLevels = 1;

	CHECK_TEX(this);
	return INFO::OK;
}


// free all resources associated with the image and make further
// use of it impossible.
void Tex::free()
{
	// do not validate - this is called from Tex::load if loading
	// failed, so not all fields may be valid.

	m_Data.reset();

	// do not zero out the fields! that could lead to trouble since
	// ogl_tex_upload followed by ogl_tex_free is legit, but would
	// cause OglTex_validate to fail (since its Tex.w is == 0).
}


//-----------------------------------------------------------------------------
// getters
//-----------------------------------------------------------------------------

// returns a pointer to the image data (pixels), taking into account any
// header(s) that may come before it.
u8* Tex::get_data() const
{
	// (can't use normal CHECK_TEX due to u8* return value)
	WARN_IF_ERR(validate());

	u8* p = m_Data.get();
	if(!p)
		return 0;
	return p + m_Ofs;
}

// returns colour of 1x1 mipmap level
u32 Tex::get_average_colour() const
{
	// require mipmaps
	if(m_numberOfMipmapLevels == 1)
		return 0;

#if OS_ANDROID
	return 0;
#endif

#warning FIXME very inneficient

	Tex basetex = *this;
	// convert to BGRA
	WARN_IF_ERR(basetex.transform(GL_BGRA_EXT));

	size_t w, h, last_level_size;
	u8* levelData = 0;
	WARN_IF_ERR(basetex.mipmapsLevel(m_numberOfMipmapLevels - 1, m_numberOfFaces -1, w, h, levelData, last_level_size));

	// extract components into u32
	u8 b = levelData[0];
	u8 g = levelData[1];
	u8 r = levelData[2];
	u8 a = levelData[3];
	return b + (g << 8) + (r << 16) + (a << 24);
}


static void add_level_size(size_t UNUSED(level), size_t UNUSED(face), size_t UNUSED(level_w), size_t UNUSED(level_h), const u8* RESTRICT UNUSED(level_data), size_t level_dataSize, void* RESTRICT cbData)
{
	size_t* ptotal_size = (size_t*)cbData;
	*ptotal_size += level_dataSize;
}

// return total byte size of the image pixels. (including mipmaps!)
// this is preferable to calculating manually because it's
// less error-prone (e.g. confusing bits_per_pixel with bytes).
size_t Tex::img_size() const
{
	// (can't use normal CHECK_TEX due to size_t return value)
	WARN_IF_ERR(validate());
	size_t out_size = 0;
	tex_util_foreach_mipmap(this, 0, add_level_size, &out_size);
	return out_size;
}

Status Tex::mipmapsLevel(size_t level, size_t face, size_t &width, size_t &height, u8 *&mipmapData, size_t &mipmapDataSize) const
{
	MipmapLevel mipmapLevel = m_mipmapsLevel ? m_mipmapsLevel : &tex_util_mipmap_level;
	return mipmapLevel(this, level, face, width, height, mipmapData, mipmapDataSize);
}


// return the minimum header size (i.e. offset to pixel data) of the
// file format indicated by <fn>'s extension (that is all it need contain:
// e.g. ".bmp"). returns 0 on error (i.e. no codec found).
// this can be used to optimize calls to tex_write: when allocating the
// buffer that will hold the image, allocate this much extra and
// pass the pointer as base+hdr_size. this allows writing the header
// directly into the output buffer and makes for zero-copy IO.
size_t tex_hdr_size(const VfsPath& filename)
{
	const ITexCodec* c;
	
	const OsPath extension = filename.Extension();
	WARN_RETURN_STATUS_IF_ERR(tex_codec_for_filename(extension, &c));
	return c->hdr_size(0);
}


//-----------------------------------------------------------------------------
// read/write from memory and disk
//-----------------------------------------------------------------------------

Status Tex::decode(const shared_ptr<u8>& Data, size_t DataSize)
{
	const ITexCodec* c;
	RETURN_STATUS_IF_ERR(tex_codec_for_header(Data.get(), DataSize, &c));

	// make sure the entire header is available
	const size_t min_hdr_size = c->hdr_size(0);
	if(DataSize < min_hdr_size)
		WARN_RETURN(ERR::TEX_INCOMPLETE_HEADER);
	const size_t hdr_size = c->hdr_size(Data.get());
	if(DataSize < hdr_size)
		WARN_RETURN(ERR::TEX_INCOMPLETE_HEADER);

	m_Data = Data;
	m_DataSize = DataSize;
	m_Ofs = hdr_size;

	RETURN_STATUS_IF_ERR(c->decode((rpU8)Data.get(), DataSize, this));

	// sanity checks
	if(!m_Width || !m_Height || m_Bpp > 32)
		WARN_RETURN(ERR::TEX_FMT_INVALID);
	if(m_DataSize < m_Ofs + img_size())
		WARN_RETURN(ERR::TEX_INVALID_SIZE);

	flip_to_global_orientation(this);

	CHECK_TEX(this);

	return INFO::OK;
}


Status Tex::encode(const OsPath& extension, DynArray* da)
{
	CHECK_TEX(this);

	// we could be clever here and avoid the extra alloc if our current
	// memory block ensued from the same kind of texture file. this is
	// most likely the case if in_img == get_data() + c->hdr_size(0).
	// this would make for zero-copy IO.

	const size_t max_out_size = img_size()*4 + 256*KiB;
	RETURN_STATUS_IF_ERR(da_alloc(da, max_out_size));

	const ITexCodec* c;
	WARN_RETURN_STATUS_IF_ERR(tex_codec_for_filename(extension, &c));

	// encode into <da>
	Status err = c->encode(this, da);
	if(err < 0)
	{
		(void)da_free(da);
		WARN_RETURN(err);
	}

	return INFO::OK;
}
