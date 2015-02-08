/* Copyright (c) 2010 Wildfire Games
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
 * DDS (DirectDraw Surface) codec.
 */

#include "precompiled.h"

#include "lib/byte_order.h"
#include "lib/ogl.h"
#include "lib/timer.h"
#include "lib/allocators/shared_ptr.h"
#include "tex_codec.h"
#include "tex_utils.h"

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

//-----------------------------------------------------------------------------
// DDS file format
//-----------------------------------------------------------------------------

// bit values and structure definitions taken from 
// http://msdn.microsoft.com/en-us/library/ee417785(VS.85).aspx

#pragma pack(push, 1)

// DDS_PIXELFORMAT.dwFlags
// we've seen some DXT3 files that don't have this set (which is nonsense;
// any image lacking alpha should be stored as DXT1). it's authoritative
// if fourcc is DXT1 (there's no other way to tell DXT1 and TEX_COMPRESSED_A apart)
// and ignored otherwise.
#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_FOURCC      0x00000004
#define DDPF_RGB         0x00000040

struct DDS_PIXELFORMAT
{
	u32 dwSize;                       // size of structure (32)
	u32 dwFlags;                      // indicates which fields are valid
	u32 dwFourCC;                     // (DDPF_FOURCC) FOURCC code, "DXTn"
	u32 dwRGBBitCount;                // (DDPF_RGB) bits per pixel
	u32 dwRBitMask;
	u32 dwGBitMask;
	u32 dwBBitMask;
	u32 dwABitMask;                   // (DDPF_ALPHAPIXELS)
};


// DDS_HEADER.dwFlags (none are optional)
#define DDSD_CAPS        0x00000001
#define DDSD_HEIGHT      0x00000002
#define DDSD_WIDTH       0x00000004
#define DDSD_PITCH       0x00000008 // used when texture is uncompressed
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE  0x00080000 // used when texture is compressed
#define DDSD_DEPTH       0x00800000

// DDS_HEADER.dwCaps
#define DDSCAPS_MIPMAP   0x00400000 // optional
#define DDSCAPS_TEXTURE	 0x00001000 // required

struct DDS_HEADER
{
	// (preceded by the FOURCC "DDS ")
	u32 dwSize;                    // size of structure (124)
	u32 dwFlags;                   // indicates which fields are valid
	u32 dwHeight;                  // (DDSD_HEIGHT) height of main image (pixels)
	u32 dwWidth;                   // (DDSD_WIDTH ) width  of main image (pixels)
	u32 dwPitchOrLinearSize;       // (DDSD_LINEARSIZE) size [bytes] of top level
	                               // (DDSD_PITCH) bytes per row (%4 = 0)
	u32 dwDepth;                   // (DDSD_DEPTH) vol. textures: vol. depth
	u32 dwMipMapCount;             // (DDSD_MIPMAPCOUNT) total # levels
	u32 dwReserved1[11];           // reserved
	DDS_PIXELFORMAT ddpf;          // (DDSD_PIXELFORMAT) surface description
	u32 dwCaps;                    // (DDSD_CAPS) misc. surface flags
	u32 dwCaps2;
	u32 dwCaps3;
	u32 dwCaps4;
	u32 dwReserved2;               // reserved
};

#pragma pack(pop)

// extract all information from DDS pixel format and store in bpp, flags.
// pf points to the DDS file's header; all fields must be endian-converted
// before use.
// output parameters invalid on failure.
static Status decode_pf(const DDS_PIXELFORMAT* pf, Tex* RESTRICT t)
{
	t->m_Bpp = 0;
	t->m_Flags = 0;

	t->m_glType = 0;
	t->m_glFormat = 0;
	t->m_glInternalFormat = 0;

	// check struct size
	if(read_le32(&pf->dwSize) != sizeof(DDS_PIXELFORMAT))
		WARN_RETURN(ERR::TEX_INVALID_SIZE);

	// determine type
	const size_t pf_flags = (size_t)read_le32(&pf->dwFlags);
	// .. uncompressed RGB/RGBA
	if(pf_flags & DDPF_RGB)
	{
		const size_t pf_bpp    = (size_t)read_le32(&pf->dwRGBBitCount);
		const size_t pf_r_mask = (size_t)read_le32(&pf->dwRBitMask);
		const size_t pf_g_mask = (size_t)read_le32(&pf->dwGBitMask);
		const size_t pf_b_mask = (size_t)read_le32(&pf->dwBBitMask);
		const size_t pf_a_mask = (size_t)read_le32(&pf->dwABitMask);

		// (checked below; must be set in case below warning is to be
		// skipped)
		t->m_Bpp = pf_bpp;

		if(pf_flags & DDPF_ALPHAPIXELS)
		{
			// something weird other than RGBA or BGRA
			if(pf_a_mask != 0xFF000000)
				WARN_RETURN(ERR::TEX_FMT_INVALID);
		}

		// make sure component ordering is 0xBBGGRR = RGB (see below)
		if(pf_r_mask != 0xFF || pf_g_mask != 0xFF00 || pf_b_mask != 0xFF0000)
		{
			// DDS_PIXELFORMAT in theory supports any ordering of R,G,B,A.
			// we need to upload to OpenGL, which can only receive BGR(A) or
			// RGB(A). the former still requires conversion (done by driver),
			// so it's slower. since the very purpose of supporting uncompressed
			// DDS is storing images in a format that requires no processing,
			// we do not allow any weird orderings that require runtime work.
			// instead, the artists must export with the correct settings.
			WARN_RETURN(ERR::TEX_FMT_INVALID);
		}

		switch (pf_bpp) {
		case 24:
			t->m_glFormat = GL_RGB;
			break;
		case 32:
			t->m_glFormat = GL_RGBA;
			break;
		default:
			WARN_RETURN(ERR::TEX_FMT_INVALID);
			break;
		}
		t->m_glType = GL_UNSIGNED_BYTE;
		t->m_glInternalFormat = t->m_glFormat;
	}
	// .. uncompressed 8bpp greyscale
	else if((pf_flags & DDPF_ALPHAPIXELS) && !(pf_flags & DDPF_FOURCC))
	{
		const size_t pf_bpp    = (size_t)read_le32(&pf->dwRGBBitCount);
		const size_t pf_a_mask = (size_t)read_le32(&pf->dwABitMask);

		t->m_Bpp = pf_bpp;

		if(pf_bpp != 8)
			WARN_RETURN(ERR::TEX_FMT_INVALID);

		if(pf_a_mask != 0xFF)
			WARN_RETURN(ERR::TEX_FMT_INVALID);
		t->m_glType = GL_UNSIGNED_BYTE;
		t->m_glFormat = GL_ALPHA;
		t->m_glInternalFormat = t->m_glFormat;
	}
	// .. compressed
	else if(pf_flags & DDPF_FOURCC)
	{
		// set effective bpp and store DXT format in flags & TEX_COMPRESSED.
		// no endian conversion necessary - FOURCC() takes care of that.
		switch(pf->dwFourCC)
		{
		case FOURCC('D','X','T','1'):
			t->m_Bpp = 4;
			if(pf_flags & DDPF_ALPHAPIXELS) {
				t->m_glFormat = GL_RGBA;
				t->m_glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			} else {
				t->m_glFormat = GL_RGB;
				t->m_glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			}
			break;

		case FOURCC('D','X','T','3'):
			t->m_Bpp = 8;
			t->m_glFormat = GL_RGBA;
			t->m_glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			break;

		case FOURCC('D','X','T','5'):
			t->m_Bpp = 8;
			t->m_glFormat = GL_RGBA;
			t->m_glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			break;
		default:
			WARN_RETURN(ERR::TEX_FMT_INVALID);
		}
	}
	// .. neither uncompressed nor compressed - invalid
	else
		WARN_RETURN(ERR::TEX_FMT_INVALID);

	return INFO::OK;
}

static Status mipmap_level(const Tex* t, size_t level, size_t face, size_t &w, size_t &h, u8* &mipmapData, size_t &mipmapDataSize)
{
	ENSURE(level < t->m_numberOfMipmapLevels);
	ENSURE(face < t->m_numberOfFaces);

	w = t->m_Width;
	h = t->m_Height;

	mipmapData = t->get_data();

	const size_t dataPadding = t->m_glType ? 1 : 4;

	for(size_t i = 0; i < level; i++)
	{
		// used to skip past this mip level in <data>
		const size_t level_dataSize = t->m_numberOfFaces * (size_t)(round_up(w, dataPadding) * round_up(h, dataPadding) * t->m_Bpp / 8);

		mipmapData += level_dataSize;

		w /= 2;
		h /= 2;
		// if the texture is non-square, one of the dimensions will become
		// 0 before the other. to satisfy OpenGL's expectations, change it
		// back to 1.
		if(w == 0) w = 1;
		if(h == 0) h = 1;
	}

	mipmapDataSize = (size_t)(round_up(w, dataPadding) * round_up(h, dataPadding) * t->m_Bpp / 8);
	mipmapData += face * mipmapDataSize;

	return INFO::OK;
}


// extract all information from DDS header and store in w, h, bpp, flags.
// sd points to the DDS file's header; all fields must be endian-converted
// before use.
// output parameters invalid on failure.
static Status decode_sd(const DDS_HEADER* sd, Tex* RESTRICT t)
{
	// check header size
	if(read_le32(&sd->dwSize) != sizeof(*sd))
		WARN_RETURN(ERR::CORRUPTED);

	// flags (indicate which fields are valid)
	const size_t sd_flags = (size_t)read_le32(&sd->dwFlags);
	// .. not all required fields are present
	// note: we can't guess dimensions - the image may not be square.
	const size_t sd_req_flags = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
	if((sd_flags & sd_req_flags) != sd_req_flags)
		WARN_RETURN(ERR::TEX_INCOMPLETE_HEADER);

	// image dimensions
	t->m_Height = (size_t)read_le32(&sd->dwHeight);
	t->m_Width = (size_t)read_le32(&sd->dwWidth);

	// pixel format
	RETURN_STATUS_IF_ERR(decode_pf(&sd->ddpf, t));

	// if the image is not aligned with the S3TC block size, it is stored
	// with extra pixels on the bottom left to fill up the space, so we need
	// to account for those when calculating how big it should be
	size_t stored_h, stored_w;
	if (!t->m_glType)
	{
		stored_h = Align<4>(t->m_Height);
		stored_w = Align<4>(t->m_Width);
	}
	else
	{
		stored_h = t->m_Height;
		stored_w = t->m_Width;
	}

	// verify pitch or linear size, if given
	const size_t pitch = stored_w*t->m_Bpp/8;
	const size_t sd_pitch_or_size = (size_t)read_le32(&sd->dwPitchOrLinearSize);
	if(sd_flags & DDSD_PITCH)
	{
		if(sd_pitch_or_size != Align<4>(pitch))
			DEBUG_WARN_ERR(ERR::CORRUPTED);
	}
	if(sd_flags & DDSD_LINEARSIZE)
	{
		// some DDS tools mistakenly store the total size of all levels,
		// so allow values close to that as well
		const ssize_t totalSize = ssize_t(pitch*stored_h*1.333333f);
		if(sd_pitch_or_size != pitch*stored_h && abs(ssize_t(sd_pitch_or_size)-totalSize) > 64)
			DEBUG_WARN_ERR(ERR::CORRUPTED);
	}
	// note: both flags set would be invalid; no need to check for that,
	// though, since one of the above tests would fail.

	// mipmaps
	if(sd_flags & DDSD_MIPMAPCOUNT)
	{
		const size_t mipmap_count = (size_t)read_le32(&sd->dwMipMapCount);
		if(mipmap_count)
		{
			// mipmap chain is incomplete
			// note: DDS includes the base level in its count, hence +1.
			if(mipmap_count != ceil_log2(std::max(t->m_Width, t->m_Height))+1)
				WARN_RETURN(ERR::TEX_FMT_INVALID);
			t->m_numberOfFaces = 1;
			t->m_numberOfMipmapLevels = mipmap_count;
		}
	} else {
		t->m_numberOfFaces = 1;
		t->m_numberOfMipmapLevels = 1;
	}

	// check for volume textures
	if(sd_flags & DDSD_DEPTH)
	{
		const size_t depth = (size_t)read_le32(&sd->dwDepth);
		if(depth)
			WARN_RETURN(ERR::NOT_SUPPORTED);
	}

	// check caps
	// .. this is supposed to be set, but don't bail if not (pointless)
	ENSURE(sd->dwCaps & DDSCAPS_TEXTURE);
	// .. sanity check: warn if mipmap flag not set (don't bail if not
	// because we've already made the decision).
	const bool mipmap_cap = (sd->dwCaps & DDSCAPS_MIPMAP) != 0;
	const bool mipmap_flag = (t->m_numberOfMipmapLevels > 1) != 0;
	ENSURE(mipmap_cap == mipmap_flag);
	// note: we do not check for cubemaps and volume textures (not supported)
	// because the file may still have useful data we can read.

	t->m_mipmapsLevel = &mipmap_level;
	return INFO::OK;
}


//-----------------------------------------------------------------------------

bool TexCodecDds::is_hdr(const u8* file) const
{
	return *(u32*)file == FOURCC('D','D','S',' ');
}


bool TexCodecDds::is_ext(const OsPath& extension) const
{
	return extension == L".dds";
}


size_t TexCodecDds::hdr_size(const u8* UNUSED(file)) const
{
	return 4+sizeof(DDS_HEADER);
}


Status TexCodecDds::decode(rpU8 data, size_t UNUSED(size), Tex* RESTRICT t) const
{
	const DDS_HEADER* sd = (const DDS_HEADER*)(data+4);
	RETURN_STATUS_IF_ERR(decode_sd(sd, t));
	return INFO::OK;
}


Status TexCodecDds::encode(Tex* RESTRICT UNUSED(t), DynArray* RESTRICT UNUSED(da)) const
{
	// note: do not return ERR::NOT_SUPPORTED et al. because that would
	// break tex_write (which assumes either this, 0 or errors are returned).
	return INFO::TEX_CODEC_CANNOT_HANDLE;
}


TIMER_ADD_CLIENT(tc_dds_transform);

Status TexCodecDds::transform(Tex* t, size_t glFormat, int flags) const
{
	TIMER_ACCRUE(tc_dds_transform);

	if (!is_hdr(t->m_Data.get()))
		return INFO::TEX_CODEC_CANNOT_HANDLE;


	// requesting removal of mipmaps
	if(t->m_numberOfMipmapLevels > 1 && flags & TEX_MIPMAPS)
	{
		// we don't need to actually change anything except the flag - the
		// mipmap levels will just be treated as trailing junk
		t->m_numberOfMipmapLevels = 1;
	}

	// requesting decompression
	if(!t->m_glType && t->m_glInternalFormat)
	{
		Status ret = TexUtils::decompress(t);
		if (ret != INFO::OK)
			return ret;
		return t->transform(glFormat, flags);
	}
	// both are DXT (unsupported; there are no flags we can change while
	// compressed) or requesting compression (not implemented) or
	// both not DXT (nothing we can do) - bail.
	return INFO::TEX_CODEC_CANNOT_HANDLE;
}
