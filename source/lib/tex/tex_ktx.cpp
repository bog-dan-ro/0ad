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
 * KTX Khronos Texture format https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec.
 */

#include "precompiled.h"

#include "lib/bits.h"
#include "lib/ogl.h"
#include "lib/timer.h"
#include "lib/allocators/shared_ptr.h"
#include "tex_codec.h"
#include "tex_utils.h"
#include "lib/byte_order.h"

namespace {
static const u8 KTX_Magic[] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static const u32 KTX_Endian = 0x04030201;
static const u32 KTX_Endian_Rev = 0x01020304;

struct KTX_Header {
	u8 identifier[12];
	u32 endianness;
	u32 glType;
	u32 glTypeSize;
	u32 glFormat;
	u32 glInternalFormat;
	u32 glBaseInternalFormat;
	u32 pixelWidth;
	u32 pixelHeight;
	u32 pixelDepth;
	u32 numberOfArrayElements;
	u32 numberOfFaces;
	u32 numberOfMipmapLevels;
	u32 bytesOfKeyValueData;
};

#define KTX_HEADER_SIZE 64
typedef int KTX_header_SIZE_ASSERT [sizeof(KTX_Header) == KTX_HEADER_SIZE];

bool checkKTX(KTX_Header *header)
{
	if (memcmp(KTX_Magic, header->identifier, 12))
		return false;

	if (KTX_Endian_Rev == header->endianness) {
		// the machine endian is different from file endian, we must swap the header values
		header->glType = swap32(header->glType);
		header->glTypeSize = swap32(header->glTypeSize);
		header->glFormat = swap32(header->glFormat);
		header->glInternalFormat = swap32(header->glInternalFormat);
		header->glBaseInternalFormat = swap32(header->glBaseInternalFormat);
		header->pixelWidth = swap32(header->pixelWidth);
		header->pixelHeight = swap32(header->pixelHeight);
		header->pixelDepth = swap32(header->pixelDepth);
		header->numberOfArrayElements = swap32(header->numberOfArrayElements);
		header->numberOfFaces = swap32(header->numberOfFaces);
		header->numberOfMipmapLevels = swap32(header->numberOfMipmapLevels);
		header->bytesOfKeyValueData = swap32(header->bytesOfKeyValueData);
	} else if (KTX_Endian != header->endianness) {
		// the header is invalid
		return false;
	}

	if (!header->numberOfMipmapLevels)
		header->numberOfMipmapLevels = 1;

	if (header->numberOfFaces != 1 && header->numberOfFaces != 6)
		return false;

	if (KTX_Endian_Rev == header->endianness &&
			header->glTypeSize != 2 && header->glTypeSize != 4) {
		return false;
	}
	return true;
}

Status mipmap_level(const Tex* t, size_t level, size_t face, size_t &w, size_t &h, u8 *&mipmapData, size_t &mipmapDataSize)
{
	ENSURE(level < t->m_numberOfMipmapLevels);
	ENSURE(face < t->m_numberOfFaces);

	w = t->m_Width;
	h = t->m_Height;

	mipmapData = t->get_data();
	KTX_Header *header = (KTX_Header*)mipmapData;
	mipmapData += sizeof(KTX_Header) + header->bytesOfKeyValueData;

	for(size_t l = 0; l <= level; l++)
	{
		const size_t faces = (l==level) ? face : header->numberOfFaces;
		for (size_t f = 0; f < faces; f++)
		{
			mipmapDataSize = *(u32 *)mipmapData;
			mipmapDataSize = (mipmapDataSize + 3) & ~(u32)3;
			mipmapData += mipmapDataSize + 4;

			w /= 2;
			h /= 2;
			// if the texture is non-square, one of the dimensions will become
			// 0 before the other. to satisfy OpenGL's expectations, change it
			// back to 1.
			if(w == 0) w = 1;
			if(h == 0) h = 1;
		}
	}

	mipmapDataSize = *(u32 *)mipmapData;
	mipmapDataSize = (mipmapDataSize + 3) & ~(u32)3;
	mipmapData += 4;
	return INFO::OK;
}

int glTypeSize(size_t glType)
{
	switch (glType) {
	case 0:
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_UNSIGNED_SHORT:
		return 2;
	}
	ENSURE(false);
	return 1;
}

int bppFromGlFormat(size_t glFormat)
{
	switch (glFormat) {
	case GL_ALPHA:
	case GL_RED:
	case GL_LUMINANCE:
		return 8;

	case GL_LUMINANCE_ALPHA:
	case GL_RG:
		return 16;

	case GL_RGB:
	case GL_BGR:
		return 24;

	case GL_RGBA:
	case GL_BGRA_EXT:
		return 32;
	}
	ENSURE(false);
	return 8;
}

} // namespace

Status TexCodecKtx::decode(u8* data, size_t size, Tex* RESTRICT t) const
{
	if (size < sizeof(KTX_Header))
		return ERR::TEX_INCOMPLETE_HEADER;

	KTX_Header *header = (KTX_Header*)data;
	if (!checkKTX(header))
		return ERR::TEX_FMT_INVALID;

	t->m_Ofs = 0;
	t->m_mipmapsLevel = &mipmap_level;
	t->m_Flags = 0;
	t->m_Width = header->pixelWidth;
	t->m_Height = header->pixelHeight;
	t->m_glType = header->glType;
	t->m_glFormat = header->glBaseInternalFormat;
	t->m_glInternalFormat = header->glInternalFormat;
	t->m_numberOfMipmapLevels = header->numberOfMipmapLevels;
	t->m_numberOfFaces = header->numberOfFaces;
	u32 hdrsize = sizeof(KTX_Header) + header->bytesOfKeyValueData;
	data += hdrsize;

	if (KTX_Endian_Rev == header->endianness) {
		size -= hdrsize;
		// we need to swap the data
		if (header->glTypeSize == 4) {
			u32 *ptr = (u32 *)data;
			for (u32 i = 0; i < size / 4; i++, ptr++)
				*ptr = swap32(*ptr);
		} else {
			u8 *dt = data;
			for (u32 level = 0; level < header->numberOfMipmapLevels; level++) {
				u32 faceSize = *(u32 *)dt;
				faceSize = swap32(faceSize);
				*(u32 *)dt = faceSize;
				faceSize = (faceSize + 3) & ~(u32)3;
				dt += 4;
				for (u32 face = 0; face < header->numberOfFaces; face++) {
					if (header->glTypeSize == 2) {
						u16 *ptr = (u16 *)dt;
						for (u32 i = 0; i < faceSize / 2; i++, ptr++)
							*ptr = swap16(*ptr);
					}
					dt += faceSize;
				}
			}
		}
		header->endianness = KTX_Endian;
	}
	if (t->m_glType)
		t->m_Bpp = bppFromGlFormat(t->m_glFormat);
	else
		t->m_Bpp = round_up((8*(*(u32 *)data))/(t->m_Width*t->m_Height), size_t(4));

	return INFO::OK;
}

Status TexCodecKtx::encode(Tex* RESTRICT t, DynArray* RESTRICT da) const
{
	size_t sz = t->img_size() + KTX_HEADER_SIZE + 8 * t->m_numberOfFaces * t->m_numberOfMipmapLevels;
	if (da->max_size_pa < sz) {
		da_free(da);
		da_alloc(da, sz);
		if (da->max_size_pa < sz)
			return ERR::NO_MEM;
	}

	KTX_Header header;
	memcpy(header.identifier, KTX_Magic, 12);
	header.endianness = KTX_Endian;
	header.glType = t->m_glType;
	header.glTypeSize = glTypeSize(t->m_glType);
	header.glFormat = t->m_glInternalFormat ? 0 : t->m_glFormat;
	header.glInternalFormat = t->m_glInternalFormat;
	header.glBaseInternalFormat = t->m_glFormat;
	header.pixelWidth = t->m_Width;
	header.pixelHeight = t->m_Height;
	header.pixelDepth = 0;
	header.numberOfArrayElements = 0;
	header.numberOfFaces = t->m_numberOfFaces;
	header.numberOfMipmapLevels = t->m_numberOfMipmapLevels;
	header.bytesOfKeyValueData = 0;
	da_append(da,&header, KTX_HEADER_SIZE);
	for (size_t l = 0; l < t->m_numberOfMipmapLevels; l++)
		for (size_t f = 0; f < t->m_numberOfFaces; f++) {
			size_t width = 0, height = 0;
			u8 *mipmapData = 0;
			size_t mipmapDataSize = 0;
			t->mipmapsLevel(l, f, width, height, mipmapData, mipmapDataSize);
			u32 sz = mipmapDataSize;
			da_append(da, &sz, 4);
			da_append(da, mipmapData, mipmapDataSize);
			mipmapDataSize = (mipmapDataSize + 3) & ~(u32)3;
			if (mipmapDataSize != sz)
				da_append(da, &header, mipmapDataSize - sz);
		}
	return INFO::OK;
}

TIMER_ADD_CLIENT(tc_ktx_transform);

Status TexCodecKtx::transform(Tex* t, size_t glFormat, int flags) const
{
	TIMER_ACCRUE(tc_ktx_transform);

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

bool TexCodecKtx::is_hdr(const u8* file) const
{
	return memcmp(file, KTX_Magic, 12) == 0;
}

bool TexCodecKtx::is_ext(const OsPath& extension) const
{
	return extension == L".ktx";
}

size_t TexCodecKtx::hdr_size(const u8* file) const
{
	const size_t hdr_size = sizeof(KTX_Header);
	if(file)
	{
		KTX_Header* hdr = (KTX_Header*)file;
		if (hdr->endianness != KTX_Endian)
			hdr->bytesOfKeyValueData = swap32(hdr->bytesOfKeyValueData);
		return hdr_size + hdr->bytesOfKeyValueData;
	}
	return hdr_size;
}
