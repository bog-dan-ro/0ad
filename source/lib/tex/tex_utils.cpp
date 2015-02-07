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

#include "precompiled.h"

#include "tex_utils.h"
#include "tex.h"
#include "lib/bits.h"
#include "lib/byte_order.h"
#include "lib/ogl.h"
#include "lib/allocators/shared_ptr.h"

namespace {
// NOTE: the convention is bottom-up for DDS, but there's no way to tell.


//-----------------------------------------------------------------------------
// S3TC decompression
//-----------------------------------------------------------------------------

// note: this code may not be terribly efficient. it's only used to
// emulate hardware S3TC support - if that isn't available, performance
// will suffer anyway due to increased video memory usage.


// for efficiency, we precalculate as much as possible about a block
// and store it here.
class S3tcBlock
{
public:
	S3tcBlock(size_t dxt, const u8* RESTRICT block)
		: dxt(dxt)
	{
		// (careful, 'dxt != 1' doesn't work - there's also DXT1a)
		const u8* a_block = block;
		const u8* c_block = (dxt == 3 || dxt == 5)? block+8 : block;

		PrecalculateAlpha(dxt, a_block);
		PrecalculateColor(dxt, c_block);
	}

	void WritePixel(size_t pixel_idx, u8* RESTRICT out) const
	{
		ENSURE(pixel_idx < 16);

		// pixel index -> color selector (2 bit) -> color
		const size_t c_selector = access_bit_tbl(c_selectors, pixel_idx, 2);
		for(int i = 0; i < 3; i++)
			out[i] = (u8)c[c_selector][i];

		// if no alpha, done
		if(dxt == 1)
			return;

		size_t a;
		if(dxt == 3)
		{
			// table of 4-bit alpha entries
			a = access_bit_tbl(a_bits, pixel_idx, 4);
			a |= a << 4; // expand to 8 bits (replicate high into low!)
		}
		else if(dxt == 5)
		{
			// pixel index -> alpha selector (3 bit) -> alpha
			const size_t a_selector = access_bit_tbl(a_bits, pixel_idx, 3);
			a = dxt5_a_tbl[a_selector];
		}
		// (dxt == TEX_COMPRESSED_A)
		else
			a = c[c_selector][A];
		out[A] = (u8)(a & 0xFF);
	}

private:
	// pixel colors are stored as size_t[4]. size_t rather than u8 protects from
	// overflow during calculations, and padding to an even size is a bit
	// more efficient (even though we don't need the alpha component).
	enum RGBA {	R, G, B, A };

	static inline void mix_2_3(size_t dst[4], size_t c0[4], size_t c1[4])
	{
		for(int i = 0; i < 3; i++) dst[i] = (c0[i]*2 + c1[i] + 1)/3;
	}

	static inline void mix_avg(size_t dst[4], size_t c0[4], size_t c1[4])
	{
		for(int i = 0; i < 3; i++) dst[i] = (c0[i]+c1[i])/2;
	}

	template<typename T>
	static inline size_t access_bit_tbl(T tbl, size_t idx, size_t bit_width)
	{
		size_t val = (tbl >> (idx*bit_width)) & bit_mask<T>(bit_width);
		return val;
	}

	// extract a range of bits and expand to 8 bits (by replicating
	// MS bits - see http://www.mindcontrol.org/~hplus/graphics/expand-bits.html ;
	// this is also the algorithm used by graphics cards when decompressing S3TC).
	// used to convert 565 to 32bpp RGB.
	static inline size_t unpack_to_8(u16 c, size_t bits_below, size_t num_bits)
	{
		const size_t num_filler_bits = 8-num_bits;
		const size_t field = (size_t)bits(c, bits_below, bits_below+num_bits-1);
		const size_t filler = field >> (num_bits-num_filler_bits);
		return (field << num_filler_bits) | filler;
	}

	void PrecalculateAlpha(size_t dxt, const u8* RESTRICT a_block)
	{
		// read block contents
		const u8 a0 = a_block[0], a1 = a_block[1];
		a_bits = read_le64(a_block);	// see below

		if(dxt == 5)
		{
			// skip a0,a1 bytes (data is little endian)
			a_bits >>= 16;

			const bool is_dxt5_special_combination = (a0 <= a1);
			u8* a = dxt5_a_tbl;	// shorthand
			if(is_dxt5_special_combination)
			{
				a[0] = a0;
				a[1] = a1;
				a[2] = (4*a0 + 1*a1 + 2)/5;
				a[3] = (3*a0 + 2*a1 + 2)/5;
				a[4] = (2*a0 + 3*a1 + 2)/5;
				a[5] = (1*a0 + 4*a1 + 2)/5;
				a[6] = 0;
				a[7] = 255;
			}
			else
			{
				a[0] = a0;
				a[1] = a1;
				a[2] = (6*a0 + 1*a1 + 3)/7;
				a[3] = (5*a0 + 2*a1 + 3)/7;
				a[4] = (4*a0 + 3*a1 + 3)/7;
				a[5] = (3*a0 + 4*a1 + 3)/7;
				a[6] = (2*a0 + 5*a1 + 3)/7;
				a[7] = (1*a0 + 6*a1 + 3)/7;
			}
		}
	}


	void PrecalculateColor(size_t dxt, const u8* RESTRICT c_block)
	{
		// read block contents
		// .. S3TC reference colors (565 format). the color table is generated
		//    from some combination of these, depending on their ordering.
		u16 rc[2];
		for(int i = 0; i < 2; i++)
			rc[i] = read_le16(c_block + 2*i);
		// .. table of 2-bit color selectors
		c_selectors = read_le32(c_block+4);

		const bool is_dxt1_special_combination = (dxt == 1 || dxt == 7) && rc[0] <= rc[1];

		// c0 and c1 are the values of rc[], converted to 32bpp
		for(int i = 0; i < 2; i++)
		{
			c[i][R] = unpack_to_8(rc[i], 11, 5);
			c[i][G] = unpack_to_8(rc[i],  5, 6);
			c[i][B] = unpack_to_8(rc[i],  0, 5);
		}

		// c2 and c3 are combinations of c0 and c1:
		if(is_dxt1_special_combination)
		{
			mix_avg(c[2], c[0], c[1]);			// c2 = (c0+c1)/2
			for(int i = 0; i < 3; i++) c[3][i] = 0;	// c3 = black
			c[3][A] = (dxt == 7)? 0 : 255;		// (transparent if TEX_COMPRESSED_A)
		}
		else
		{
			mix_2_3(c[2], c[0], c[1]);			// c2 = 2/3*c0 + 1/3*c1
			mix_2_3(c[3], c[1], c[0]);			// c3 = 1/3*c0 + 2/3*c1
		}
	}

	// the 4 color choices for each pixel (RGBA)
	size_t c[4][4];	// c[i][RGBA_component]

	// (DXT5 only) the 8 alpha choices
	u8 dxt5_a_tbl[8];

	// alpha block; interpretation depends on dxt.
	u64 a_bits;

	// table of 2-bit color selectors
	u32 c_selectors;

	size_t dxt;
};


struct S3tcDecompressInfo
{
	size_t dxt;
	size_t s3tc_block_size;
	size_t out_Bpp;
	u8* out;
};

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

size_t glFormat2Dxt(size_t format)
{
	switch(format) {
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		return 7;
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		return 1;
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		return 3;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		return 5;
	}
	return 0;
}

void s3tc_decompress_level(size_t UNUSED(level), size_t UNUSED(face), size_t level_w, size_t level_h,
								  const u8* RESTRICT level_data, size_t level_data_size, void* RESTRICT cbData)
{
	S3tcDecompressInfo* di = (S3tcDecompressInfo*)cbData;
	const size_t dxt             = di->dxt;
	const size_t s3tc_block_size = di->s3tc_block_size;

	// note: 1x1 images are legitimate (e.g. in mipmaps). they report their
	// width as such for glTexImage, but the S3TC data is padded to
	// 4x4 pixel block boundaries.
	const size_t blocks_w = DivideRoundUp(level_w, size_t(4));
	const size_t blocks_h = DivideRoundUp(level_h, size_t(4));
	const u8* s3tc_data = level_data;
	ENSURE(level_data_size % s3tc_block_size == 0);

	for(size_t block_y = 0; block_y < blocks_h; block_y++)
	{
		for(size_t block_x = 0; block_x < blocks_w; block_x++)
		{
			S3tcBlock block(dxt, s3tc_data);
			s3tc_data += s3tc_block_size;

			size_t pixel_idx = 0;
			for(int y = 0; y < 4; y++)
			{
				// this is ugly, but advancing after x, y and block_y loops
				// is no better.
				u8* out = (u8*)di->out + ((block_y*4+y)*blocks_w*4 + block_x*4) * di->out_Bpp;
				for(int x = 0; x < 4; x++)
				{
					block.WritePixel(pixel_idx, out);
					out += di->out_Bpp;
					pixel_idx++;
				}
			}
		}
	}

	ENSURE(s3tc_data == level_data + level_data_size);
	di->out += blocks_w*blocks_h * 16 * di->out_Bpp;
}


// decompress the given image (which is known to be stored as DXTn)
// effectively in-place. updates Tex fields.
Status s3tc_decompress(Tex* t)
{
	// alloc new image memory
	// notes:
	// - dxt == 1 is the only non-alpha case.
	// - adding or stripping alpha channels during transform is not
	//   our job; we merely output the same pixel format as given
	//   (tex.cpp's plain transform could cover it, if ever needed).
	const size_t dxt = glFormat2Dxt(t->m_glInternalFormat);
	const size_t out_bpp = (dxt != 1)? 32 : 24;
	const size_t out_size = t->img_size() * out_bpp / t->m_Bpp;
	shared_ptr<u8> decompressedData;
	AllocateAligned(decompressedData, out_size, pageSize);

	const size_t s3tc_block_size = (dxt == 3 || dxt == 5)? 16 : 8;
	S3tcDecompressInfo di = { dxt, s3tc_block_size, out_bpp/8, decompressedData.get() };
	const int levels_to_skip = 0;
	tex_util_foreach_mipmap(t, levels_to_skip, s3tc_decompress_level, &di);
	t->m_Data = decompressedData;
	t->m_DataSize = out_size;
	t->m_Ofs = 0;
	t->m_Bpp = out_bpp;
	t->m_glInternalFormat = t->m_glFormat;
	t->m_glType = GL_UNSIGNED_BYTE;
	t->m_mipmapsLevel = 0;
	return INFO::OK;
}


#define GL_COMPRESSED_R11_EAC                            0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                     0x9271
#define GL_COMPRESSED_RG11_EAC                           0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                    0x9273
#define GL_COMPRESSED_RGB8_ETC2                          0x9274
#define GL_COMPRESSED_SRGB8_ETC2                         0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2      0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2     0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                     0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC              0x9279

}

namespace TexUtils {

Status decompress(Tex *t)
{
	if (glFormat2Dxt(t->m_glInternalFormat))
		return s3tc_decompress(t);
	return INFO::TEX_CODEC_CANNOT_HANDLE;
}

}// namespace TexUtils
