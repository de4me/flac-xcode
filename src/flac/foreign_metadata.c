/* flac - Command-line FLAC encoder/decoder
 * Copyright (C) 2000-2009  Josh Coalson
 * Copyright (C) 2011-2025  Xiph.Org Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h> /* for FILE etc. */
#include <stdlib.h> /* for calloc() etc. */
#include <string.h> /* for memcmp() etc. */
#include "FLAC/assert.h"
#include "FLAC/metadata.h"
#include "share/alloc.h"
#include "share/compat.h"
#include "foreign_metadata.h"

#ifdef min
#undef min
#endif
#define min(x,y) ((x)<(y)?(x):(y))

const char *FLAC__FOREIGN_METADATA_APPLICATION_ID[FLAC__FOREIGN_METADATA_NUMBER_OF_RECOGNIZED_APPLICATION_IDS] = { "aiff" , "riff", "w64 " };

static FLAC__uint32 unpack32be_(const FLAC__byte *b)
{
	return ((FLAC__uint32)b[0]<<24) + ((FLAC__uint32)b[1]<<16) + ((FLAC__uint32)b[2]<<8) + (FLAC__uint32)b[3];
}

static FLAC__uint32 unpack32le_(const FLAC__byte *b)
{
	return (FLAC__uint32)b[0] + ((FLAC__uint32)b[1]<<8) + ((FLAC__uint32)b[2]<<16) + ((FLAC__uint32)b[3]<<24);
}

static FLAC__uint64 unpack64le_(const FLAC__byte *b)
{
	return (FLAC__uint64)b[0] + ((FLAC__uint64)b[1]<<8) + ((FLAC__uint64)b[2]<<16) + ((FLAC__uint64)b[3]<<24) + ((FLAC__uint64)b[4]<<32) + ((FLAC__uint64)b[5]<<40) + ((FLAC__uint64)b[6]<<48) + ((FLAC__uint64)b[7]<<56);
}

/* copies 'size' bytes from file 'fin' to 'fout', filling in *error with 'read_error' or 'write_error' as necessary */
static FLAC__bool copy_data_(FILE *fin, FILE *fout, size_t size, const char **error, const char * const read_error, const char * const write_error)
{
	FLAC__byte buffer[4096];
	size_t left;
	for(left = size; left > 0; ) {
		size_t need = min(sizeof(buffer), left);
		if(fread(buffer, 1, need, fin) < need) {
			if(error) *error = read_error;
			return false;
		}
		if(fwrite(buffer, 1, need, fout) < need) {
			if(error) *error = write_error;
			return false;
		}
		left -= need;
	}
	return true;
}

/* compare 'size' bytes from file 'fin' to 'fout', filling in *error with 'read_error' or 'write_error' as necessary */
static FLAC__bool compare_data_(FILE *fin, FILE *fout, size_t size, const char **error, const char * const read_error, const char * const write_error, const char * const compare_error)
{
	FLAC__byte buffer_in[4096];
	FLAC__byte buffer_out[4096]; /* sizes need to be the same */
	size_t left;
	for(left = size; left > 0; ) {
		size_t need = min(sizeof(buffer_in), left);
		if(fread(buffer_in, 1, need, fin) < need) {
			if(error) *error = read_error;
			return false;
		}
		if(fread(buffer_out, 1, need, fout) < need) {
			if(error) *error = write_error;
			return false;
		}
		if(memcmp(buffer_in, buffer_out, need)) {
			if(error) *error = compare_error;
			return false;
		}
		left -= need;
	}
	return true;
}

static FLAC__bool append_block_(foreign_metadata_t *fm, FLAC__off_t offset, FLAC__uint32 size, const char **error)
{
	foreign_block_t *fb;
	if(size >= ((1u << FLAC__STREAM_METADATA_LENGTH_LEN) - FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8)) {
		if(error) *error = "found foreign metadata chunk is too large (max is 16MiB per chunk)";
		return false;
	}
	fb = safe_realloc_nofree_muladd2_(fm->blocks, sizeof(foreign_block_t), /*times (*/fm->num_blocks, /*+*/1/*)*/);
	if(fb) {
		fb[fm->num_blocks].offset = offset;
		fb[fm->num_blocks].size = size;
		fm->num_blocks++;
		fm->blocks = fb;
		return true;
	}
	if(error) *error = "out of memory";
	return false;
}

static FLAC__bool read_from_aiff_(foreign_metadata_t *fm, FILE *f, const char **error)
{
	FLAC__byte buffer[12];
	FLAC__off_t offset, eof_offset;
	if((offset = ftello(f)) < 0) {
		if(error) *error = "ftello() error (001)";
		return false;
	}
	if(fread(buffer, 1, 12, f) < 12 || memcmp(buffer, "FORM", 4) || (memcmp(buffer+8, "AIFF", 4) && memcmp(buffer+8, "AIFC", 4))) {
		if(error) *error = "unsupported FORM layout (002)";
		return false;
	}
	if(!append_block_(fm, offset, 12, error))
		return false;
	eof_offset = (FLAC__off_t)8 + (FLAC__off_t)unpack32be_(buffer+4);
	while(!feof(f)) {
		FLAC__uint32 size;
		if((offset = ftello(f)) < 0) {
			if(error) *error = "ftello() error (003)";
			return false;
		}
		if((size = fread(buffer, 1, 8, f)) < 8) {
			if(size == 0 && feof(f))
				break;
			if(error) *error = "invalid AIFF file (004)";
			return false;
		}
		size = unpack32be_(buffer+4);
		/* check if pad byte needed */
		if(size & 1)
			size++;
		if(!memcmp(buffer, "COMM", 4)) {
			if(fm->format_block) {
				if(error) *error = "invalid AIFF file: multiple \"COMM\" chunks (005)";
				return false;
			}
			if(fm->audio_block) {
				if(error) *error = "invalid AIFF file: \"SSND\" chunk before \"COMM\" chunk (006)";
				return false;
			}
			fm->format_block = fm->num_blocks;
		}
		else if(!memcmp(buffer, "SSND", 4)) {
			if(fm->audio_block) {
				if(error) *error = "invalid AIFF file: multiple \"SSND\" chunks (007)";
				return false;
			}
			if(!fm->format_block) {
				if(error) *error = "invalid AIFF file: \"SSND\" chunk before \"COMM\" chunk (008)";
				return false;
			}
			fm->audio_block = fm->num_blocks;
			/* read #offset bytes */
			if(fread(buffer+8, 1, 4, f) < 4) {
				if(error) *error = "invalid AIFF file (009)";
				return false;
			}
			fm->ssnd_offset_size = unpack32be_(buffer+8);
			if(fseeko(f, -4, SEEK_CUR) < 0) {
				if(error) *error = "invalid AIFF file: seek error (010)";
				return false;
			}
			/* WATCHOUT: For SSND we ignore the blockSize and are not saving any
			 * unaligned part at the end of the chunk.  In retrospect it is pretty
			 * pointless to save the unaligned data before the PCM but now it is
			 * done and cast in stone.
			 */
		}
		if(!append_block_(fm, offset, 8 + (memcmp(buffer, "SSND", 4)? size : 8 + fm->ssnd_offset_size), error))
			return false;
		/* skip to next chunk */
		if(fseeko(f, size, SEEK_CUR) < 0) {
			if(error) *error = "invalid AIFF file: seek error (011)";
			return false;
		}
	}
	if(eof_offset != ftello(f)) {
		if(error) *error = "invalid AIFF file: unexpected EOF (012)";
		return false;
	}
	if(!fm->format_block) {
		if(error) *error = "invalid AIFF file: missing \"COMM\" chunk (013)";
		return false;
	}
	if(!fm->audio_block) {
		if(error) *error = "invalid AIFF file: missing \"SSND\" chunk (014)";
		return false;
	}
	return true;
}

static FLAC__bool read_from_wave_(foreign_metadata_t *fm, FILE *f, const char **error)
{
	FLAC__byte buffer[12];
	FLAC__off_t offset, eof_offset = -1, ds64_data_size = -1;
	if((offset = ftello(f)) < 0) {
		if(error) *error = "ftello() error (001)";
		return false;
	}
	if(fread(buffer, 1, 12, f) < 12 || (memcmp(buffer, "RIFF", 4) && memcmp(buffer, "RF64", 4)) || memcmp(buffer+8, "WAVE", 4)) {
		if(error) *error = "unsupported RIFF layout (002)";
		return false;
	}
	if(!memcmp(buffer, "RF64", 4))
		fm->is_rf64 = true;
	if(fm->is_rf64 && sizeof(FLAC__off_t) < 8) {
		if(error) *error = "RF64 is not supported on this compile (r00)";
		return false;
	}
	if(!append_block_(fm, offset, 12, error))
		return false;
	if(!fm->is_rf64 || unpack32le_(buffer+4) != 0xffffffff) {
		eof_offset = (FLAC__off_t)8 + (FLAC__off_t)unpack32le_(buffer+4);
		if(eof_offset & 1) /* fix odd RIFF size */
			eof_offset++;
	}
	while(!feof(f)) {
		FLAC__off_t size;
		if((offset = ftello(f)) < 0) {
			if(error) *error = "ftello() error (003)";
			return false;
		}
		if((size = fread(buffer, 1, 8, f)) < 8) {
			if(size == 0 && feof(f))
				break;
			if(error) *error = "invalid WAVE file (004)";
			return false;
		}
		size = unpack32le_(buffer+4);
		/* check if pad byte needed */
		if(size & 1)
			size++;
		if(!memcmp(buffer, "fmt ", 4)) {
			if(fm->format_block) {
				if(error) *error = "invalid WAVE file: multiple \"fmt \" chunks (005)";
				return false;
			}
			if(fm->audio_block) {
				if(error) *error = "invalid WAVE file: \"data\" chunk before \"fmt \" chunk (006)";
				return false;
			}
			fm->format_block = fm->num_blocks;
		}
		else if(!memcmp(buffer, "data", 4)) {
			if(fm->audio_block) {
				if(error) *error = "invalid WAVE file: multiple \"data\" chunks (007)";
				return false;
			}
			if(!fm->format_block) {
				if(error) *error = "invalid WAVE file: \"data\" chunk before \"fmt \" chunk (008)";
				return false;
			}
			fm->audio_block = fm->num_blocks;
			if(fm->is_rf64 && fm->num_blocks < 2) {
				if(error) *error = "invalid RF64 file: \"data\" chunk before \"ds64\" chunk (r01)";
				return false;
			}
		}
		if(!append_block_(fm, offset, 8 + (memcmp(buffer, "data", 4)? size : 0), error))
			return false;
		/* parse ds64 chunk if necessary */
		if(fm->is_rf64 && fm->num_blocks == 2) {
			FLAC__byte buffer2[7*4];
			if(memcmp(buffer, "ds64", 4)) {
				if(error) *error = "invalid RF64 file: \"ds64\" chunk does not immediately follow \"WAVE\" marker (r02)";
				return false;
			}
			/* unpack the size again since we don't want the padding byte effect */
			size = unpack32le_(buffer+4);
			if(size < (FLAC__off_t)sizeof(buffer2)) {
				if(error) *error = "invalid RF64 file: \"ds64\" chunk size is < 28 (r03)";
				return false;
			}
			if(size > (FLAC__off_t)sizeof(buffer2)) {
				if(error) *error = "RF64 file has \"ds64\" chunk with extra size table, which is not currently supported (r04)";
				return false;
			}
			if(fread(buffer2, 1, sizeof(buffer2), f) < sizeof(buffer2)) {
				if(error) *error = "unexpected EOF reading \"ds64\" chunk data in RF64 file (r05)";
				return false;
			}
			ds64_data_size = (FLAC__off_t)unpack64le_(buffer2+8);
			if(ds64_data_size == (FLAC__off_t)(-1)) {
				if(error) *error = "RF64 file has \"ds64\" chunk with data size == -1 (r08)";
				return false;
			}
			/* check if pad byte needed */
			if(ds64_data_size & 1)
				ds64_data_size++;
			/* @@@ [2^63 limit] */
			if(ds64_data_size < 0) {
				if(error) *error = "RF64 file too large (r09)";
				return false;
			}
			if(unpack32le_(buffer2+24)) {
				if(error) *error = "RF64 file has \"ds64\" chunk with extra size table, which is not currently supported (r06)";
				return false;
			}
			eof_offset = (FLAC__off_t)8 + (FLAC__off_t)unpack64le_(buffer2);
			/* @@@ [2^63 limit] */
			if((FLAC__off_t)unpack64le_(buffer2) < 0 || eof_offset < 0) {
				if(error) *error = "RF64 file too large (r07)";
				return false;
			}
		}
		else { /* skip to next chunk */
			if(fm->is_rf64 && !memcmp(buffer, "data", 4) && unpack32le_(buffer+4) == 0xffffffff) {
				if(fseeko(f, ds64_data_size, SEEK_CUR) < 0) {
					if(error) *error = "invalid RF64 file: seek error (r10)";
					return false;
				}
			}
			else {
				if(fseeko(f, size, SEEK_CUR) < 0) {
					if(error) *error = "invalid WAVE file: seek error (009)";
					return false;
				}
			}
		}
	}
	if(fm->is_rf64 && eof_offset == (FLAC__off_t)(-1)) {
		if(error) *error = "invalid RF64 file: all RIFF sizes are -1 (r11)";
		return false;
	}
	if(eof_offset != ftello(f)) {
		if(error) *error = "invalid WAVE file: unexpected EOF (010)";
		return false;
	}
	if(!fm->format_block) {
		if(error) *error = "invalid WAVE file: missing \"fmt \" chunk (011)";
		return false;
	}
	if(!fm->audio_block) {
		if(error) *error = "invalid WAVE file: missing \"data\" chunk (012)";
		return false;
	}
	return true;
}

static FLAC__bool read_from_wave64_(foreign_metadata_t *fm, FILE *f, const char **error)
{
	FLAC__byte buffer[40];
	FLAC__off_t offset, eof_offset = -1;
	if((offset = ftello(f)) < 0) {
		if(error) *error = "ftello() error (001)";
		return false;
	}
	if(
		fread(buffer, 1, 40, f) < 40 ||
		/* RIFF GUID 66666972-912E-11CF-A5D6-28DB04C10000 */
		memcmp(buffer, "\x72\x69\x66\x66\x2E\x91\xCF\x11\xA5\xD6\x28\xDB\x04\xC1\x00\x00", 16) ||
		/* WAVE GUID 65766177-ACF3-11D3-8CD1-00C04F8EDB8A */
		memcmp(buffer+24, "\x77\x61\x76\x65\xF3\xAC\xD3\x11\x8C\xD1\x00\xC0\x4F\x8E\xDB\x8A", 16)
	) {
		if(error) *error = "unsupported Wave64 layout (002)";
		return false;
	}
	if(sizeof(FLAC__off_t) < 8) {
		if(error) *error = "Wave64 is not supported on this compile (r00)";
		return false;
	}
	if(!append_block_(fm, offset, 40, error))
		return false;
	eof_offset = (FLAC__off_t)unpack64le_(buffer+16); /*@@@ [2^63 limit] */
	while(!feof(f)) {
		FLAC__uint64 size;
		if((offset = ftello(f)) < 0) {
			if(error) *error = "ftello() error (003)";
			return false;
		}
		if((size = fread(buffer, 1, 24, f)) < 24) {
			if(size == 0 && feof(f))
				break;
			if(error) *error = "invalid Wave64 file (004)";
			return false;
		}
		size = unpack64le_(buffer+16);
		/* check if pad bytes needed */
		if(size & 7)
			size = (size+7) & (~((FLAC__uint64)7));
		if(size < 24) {
			if(error) *error = "invalid Wave64 file: chunk length invalid";
			return false;
		}
		/* fmt GUID 20746D66-ACF3-11D3-8CD1-00C04F8EDB8A */
		if(!memcmp(buffer, "\x66\x6D\x74\x20\xF3\xAC\xD3\x11\x8C\xD1\x00\xC0\x4F\x8E\xDB\x8A", 16)) {
			if(fm->format_block) {
				if(error) *error = "invalid Wave64 file: multiple \"fmt \" chunks (005)";
				return false;
			}
			if(fm->audio_block) {
				if(error) *error = "invalid Wave64 file: \"data\" chunk before \"fmt \" chunk (006)";
				return false;
			}
			fm->format_block = fm->num_blocks;
		}
		/* data GUID 61746164-ACF3-11D3-8CD1-00C04F8EDB8A */
		else if(!memcmp(buffer, "\x64\x61\x74\x61\xF3\xAC\xD3\x11\x8C\xD1\x00\xC0\x4F\x8E\xDB\x8A", 16)) {
			if(fm->audio_block) {
				if(error) *error = "invalid Wave64 file: multiple \"data\" chunks (007)";
				return false;
			}
			if(!fm->format_block) {
				if(error) *error = "invalid Wave64 file: \"data\" chunk before \"fmt \" chunk (008)";
				return false;
			}
			fm->audio_block = fm->num_blocks;
		}
		if(!append_block_(fm, offset, memcmp(buffer, "\x64\x61\x74\x61\xF3\xAC\xD3\x11\x8C\xD1\x00\xC0\x4F\x8E\xDB\x8A", 16)? (FLAC__uint32)size : 16+8, error))
			return false;
		/* skip to next chunk */
		if(fseeko(f, size-24, SEEK_CUR) < 0) {
			if(error) *error = "invalid Wave64 file: seek error (009)";
			return false;
		}
	}
	if(eof_offset != ftello(f)) {
		if(error) *error = "invalid Wave64 file: unexpected EOF (010)";
		return false;
	}
	if(!fm->format_block) {
		if(error) *error = "invalid Wave64 file: missing \"fmt \" chunk (011)";
		return false;
	}
	if(!fm->audio_block) {
		if(error) *error = "invalid Wave64 file: missing \"data\" chunk (012)";
		return false;
	}
	return true;
}

static FLAC__bool write_to_flac_(foreign_metadata_t *fm, FILE *fin, FILE *fout, FLAC__Metadata_SimpleIterator *it, const char **error)
{
	FLAC__byte buffer[4];
	const uint32_t ID_LEN = FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8;
	size_t block_num = 0;
	FLAC__ASSERT(sizeof(buffer) >= ID_LEN);
	while(block_num < fm->num_blocks) {
		/* find next matching padding block */
		do {
			/* even on the first chunk's loop there will be a skippable STREAMINFO block, on subsequent loops we are first moving past the PADDING we just used */
			if(!FLAC__metadata_simple_iterator_next(it)) {
				if(error) *error = "no matching PADDING block found (004)";
				return false;
			}
		} while(FLAC__metadata_simple_iterator_get_block_type(it) != FLAC__METADATA_TYPE_PADDING);
		if(FLAC__metadata_simple_iterator_get_block_length(it) != ID_LEN+fm->blocks[block_num].size) {
			if(error) *error = "PADDING block with wrong size found (005)";
			return false;
		}
		/* transfer chunk into APPLICATION block */
		/* first set up the file pointers */
		if(fseeko(fin, fm->blocks[block_num].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in WAVE/AIFF file (006)";
			return false;
		}
		if(fseeko(fout, FLAC__metadata_simple_iterator_get_block_offset(it), SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file (007)";
			return false;
		}
		/* update the type */
		buffer[0] = FLAC__METADATA_TYPE_APPLICATION;
		if(FLAC__metadata_simple_iterator_is_last(it))
			buffer[0] |= 0x80; /*MAGIC number*/
		if(fwrite(buffer, 1, 1, fout) < 1) {
			if(error) *error = "write failed in FLAC file (008)";
			return false;
		}
		/* length stays the same so skip over it */
		if(fseeko(fout, FLAC__STREAM_METADATA_LENGTH_LEN/8, SEEK_CUR) < 0) {
			if(error) *error = "seek failed in FLAC file (009)";
			return false;
		}
		/* write the APPLICATION ID */
		memcpy(buffer, FLAC__FOREIGN_METADATA_APPLICATION_ID[fm->type], ID_LEN);
		if(fwrite(buffer, 1, ID_LEN, fout) < ID_LEN) {
			if(error) *error = "write failed in FLAC file (010)";
			return false;
		}
		/* transfer the foreign metadata */
		if(!copy_data_(fin, fout, fm->blocks[block_num].size, error, "read failed in WAVE/AIFF file (011)", "write failed in FLAC file (012)"))
			return false;
		block_num++;
	}
	return true;
}

static FLAC__bool read_from_flac_(foreign_metadata_t *fm, FILE *f, FLAC__Metadata_SimpleIterator *it, const char **error)
{
	FLAC__byte id[4], buffer[32];
	FLAC__off_t offset;
	FLAC__uint32 length;
	FLAC__bool first_block = true, type_found = false, ds64_found = false;

	FLAC__ASSERT(FLAC__STREAM_METADATA_APPLICATION_ID_LEN == sizeof(id)*8);

	while(FLAC__metadata_simple_iterator_next(it)) {
		if(FLAC__metadata_simple_iterator_get_block_type(it) != FLAC__METADATA_TYPE_APPLICATION)
			continue;
		if(!FLAC__metadata_simple_iterator_get_application_id(it, id)) {
			if(error) *error = "FLAC__metadata_simple_iterator_get_application_id() error (002)";
			return false;
		}
		if(first_block) {
			uint32_t i;
			for(i = 0; i < FLAC__FOREIGN_METADATA_NUMBER_OF_RECOGNIZED_APPLICATION_IDS; i++)
				if(memcmp(id, FLAC__FOREIGN_METADATA_APPLICATION_ID[i], sizeof(id)) == 0) {
					fm->type = i;
					first_block = false;
				}
			if(first_block) /* means no first foreign metadata block was found yet */
				continue;
		}
		else if(memcmp(id, FLAC__FOREIGN_METADATA_APPLICATION_ID[fm->type], sizeof(id)))
			continue;
		offset = FLAC__metadata_simple_iterator_get_block_offset(it);
		length = FLAC__metadata_simple_iterator_get_block_length(it);
		/* skip over header and app ID */
		offset += (FLAC__STREAM_METADATA_IS_LAST_LEN + FLAC__STREAM_METADATA_TYPE_LEN + FLAC__STREAM_METADATA_LENGTH_LEN) / 8;
		offset += sizeof(id);
		/* look for format or audio blocks */
		if(fseeko(f, offset, SEEK_SET) < 0) {
			if(error) *error = "seek error (003)";
			return false;
		}
		if(fread(buffer, 1, 4, f) != 4) {
			if(error) *error = "read error (004)";
			return false;
		}
		if(fm->num_blocks == 0) { /* first block? */
			/* Initialize bools */
			fm->is_wavefmtex = 0;
			fm->is_aifc = 0;
			fm->is_sowt = 0;
			fm->is_rf64 = 0 == memcmp(buffer, "RF64", 4);

			if(fm->type == FOREIGN_BLOCK_TYPE__RIFF && (0 == memcmp(buffer, "RIFF", 4) || fm->is_rf64))
				type_found = true;
			else if(fm->type == FOREIGN_BLOCK_TYPE__WAVE64 && 0 == memcmp(buffer, "riff", 4)) /* use first 4 bytes instead of whole GUID */
				type_found = true;
			else if(fm->type == FOREIGN_BLOCK_TYPE__AIFF && 0 == memcmp(buffer, "FORM", 4)) {
				type_found = true;
				if(fread(buffer+4, 1, 8, f) != 8) {
					if(error) *error = "read error (020)";
					return false;
				}
				fm->is_aifc = 0 == memcmp(buffer+8, "AIFC", 4);
			}
			else {
				if(error) *error = "unsupported foreign metadata found, may need newer FLAC decoder (005)";
				return false;
			}
		}
		else if(!type_found) {
			FLAC__ASSERT(0);
			/* double protection: */
			if(error) *error = "unsupported foreign metadata found, may need newer FLAC decoder (006)";
			return false;
		}
		else if(fm->type == FOREIGN_BLOCK_TYPE__RIFF) {
			if(!memcmp(buffer, "fmt ", 4)) {
				if(fm->format_block) {
					if(error) *error = "invalid WAVE metadata: multiple \"fmt \" chunks (007)";
					return false;
				}
				if(fm->audio_block) {
					if(error) *error = "invalid WAVE metadata: \"data\" chunk before \"fmt \" chunk (008)";
					return false;
				}
				fm->format_block = fm->num_blocks;
				if(fread(buffer+4, 1, 8, f) != 8) {
					if(error) *error = "read error (020)";
					return false;
				}
				fm->is_wavefmtex = 0 == memcmp(buffer+8, "\xfe\xff", 2);
			}
			else if(!memcmp(buffer, "data", 4)) {
				if(fm->audio_block) {
					if(error) *error = "invalid WAVE metadata: multiple \"data\" chunks (009)";
					return false;
				}
				if(!fm->format_block) {
					if(error) *error = "invalid WAVE metadata: \"data\" chunk before \"fmt \" chunk (010)";
					return false;
				}
				fm->audio_block = fm->num_blocks;
			}
			else if(fm->is_rf64 && fm->num_blocks == 1) {
				if(memcmp(buffer, "ds64", 4)) {
					if(error) *error = "invalid RF64 metadata: second chunk is not \"ds64\" (011)";
					return false;
				}
				ds64_found = true;
			}
		}
		else if(fm->type == FOREIGN_BLOCK_TYPE__WAVE64) {
			if(!memcmp(buffer, "fmt ", 4)) { /* use first 4 bytes instead of whole GUID */
				if(fm->format_block) {
					if(error) *error = "invalid Wave64 metadata: multiple \"fmt \" chunks (012)";
					return false;
				}
				if(fm->audio_block) {
					if(error) *error = "invalid Wave64 metadata: \"data\" chunk before \"fmt \" chunk (013)";
					return false;
				}
				fm->format_block = fm->num_blocks;
			}
			else if(!memcmp(buffer, "data", 4)) { /* use first 4 bytes instead of whole GUID */
				if(fm->audio_block) {
					if(error) *error = "invalid Wave64 metadata: multiple \"data\" chunks (014)";
					return false;
				}
				if(!fm->format_block) {
					if(error) *error = "invalid Wave64 metadata: \"data\" chunk before \"fmt \" chunk (015)";
					return false;
				}
				fm->audio_block = fm->num_blocks;
			}
		}
		else if(fm->type == FOREIGN_BLOCK_TYPE__AIFF) {
			if(!memcmp(buffer, "COMM", 4)) {
				if(fm->format_block) {
					if(error) *error = "invalid AIFF metadata: multiple \"COMM\" chunks (016)";
					return false;
				}
				if(fm->audio_block) {
					if(error) *error = "invalid AIFF metadata: \"SSND\" chunk before \"COMM\" chunk (017)";
					return false;
				}
				fm->format_block = fm->num_blocks;
				if(fm->is_aifc) {
					if(fread(buffer+4, 1, 26, f) != 26) {
						if(error) *error = "read error (020)";
						return false;
					}
					fm->is_sowt = 0 == memcmp(buffer+26, "sowt", 2);
					fm->aifc_comm_length = length;
				}
			}
			else if(!memcmp(buffer, "SSND", 4)) {
				if(fm->audio_block) {
					if(error) *error = "invalid AIFF metadata: multiple \"SSND\" chunks (018)";
					return false;
				}
				if(!fm->format_block) {
					if(error) *error = "invalid AIFF metadata: \"SSND\" chunk before \"COMM\" chunk (019)";
					return false;
				}
				fm->audio_block = fm->num_blocks;
				/* read SSND offset size */
				if(fread(buffer+4, 1, 8, f) != 8) {
					if(error) *error = "read error (020)";
					return false;
				}
				fm->ssnd_offset_size = unpack32be_(buffer+8);
			}
		}
		else {
			FLAC__ASSERT(0);
			/* double protection: */
			if(error) *error = "unsupported foreign metadata found, may need newer FLAC decoder (021)";
			return false;
		}
		if(!append_block_(fm, offset, FLAC__metadata_simple_iterator_get_block_length(it)-sizeof(id), error))
			return false;
	}
	if(fm->is_rf64 && !ds64_found) {
		if(error) *error = "invalid RF64 file: second chunk is not \"ds64\" (023)";
		return false;
	}
	if(!fm->format_block) {
		if(error)
			*error =
				fm->type==FOREIGN_BLOCK_TYPE__RIFF? "invalid WAVE file: missing \"fmt \" chunk (024)" :
				fm->type==FOREIGN_BLOCK_TYPE__WAVE64? "invalid Wave64 file: missing \"fmt \" chunk (025)" :
				"invalid AIFF file: missing \"COMM\" chunk (026)";
		return false;
	}
	if(!fm->audio_block) {
		if(error)
			*error =
				fm->type==FOREIGN_BLOCK_TYPE__RIFF? "invalid WAVE file: missing \"data\" chunk (027)" :
				fm->type==FOREIGN_BLOCK_TYPE__WAVE64? "invalid Wave64 file: missing \"data\" chunk (028)" :
				"invalid AIFF file: missing \"SSND\" chunk (029)";
		return false;
	}
	return true;
}

static FLAC__bool write_to_iff_(foreign_metadata_t *fm, FILE *fin, FILE *fout, FLAC__off_t offset1, FLAC__off_t offset2, FLAC__off_t offset3, const char **error)
{
	size_t i;
	if(fseeko(fout, offset1, SEEK_SET) < 0) {
		if(error) *error = "seek failed in WAVE/AIFF file";
		return false;
	}

	/* don't write first (RIFF/RF64/FORM) chunk, or ds64 chunk in the case of RF64 */
	for(i = fm->is_rf64?2:1; i < fm->format_block; i++) {
		if(fseeko(fin, fm->blocks[i].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!copy_data_(fin, fout, fm->blocks[i].size, error, "read failed in FLAC file", "write failed in WAVE/AIFF file"))
			return false;
	}

	if(fm->is_aifc) {
		/* Need to restore compression type name */
		if(fseeko(fout, 30, SEEK_CUR) < 0) {
			if(error) *error = "seek failed in AIFF-C file";
			return false;
		}
		if(fseeko(fin, fm->blocks[i].offset+30, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!copy_data_(fin, fout, fm->aifc_comm_length-34, error, "read failed in FLAC file", "write failed in WAVE/AIFF file"))
			return false;
		/* Now seek back */
		if(fseeko(fout, ((FLAC__int32)(fm->aifc_comm_length) * -1) + 4, SEEK_CUR) < 0) {
			if(error) *error = "seek failed in AIFF-C file";
			return false;
		}
	}
	if(fseeko(fout, offset2, SEEK_SET) < 0) {
		if(error) *error = "seek failed in WAVE/AIFF file (006)";
		return false;
	}
	for(i = fm->format_block+1; i < fm->audio_block; i++) {
		if(fseeko(fin, fm->blocks[i].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!copy_data_(fin, fout, fm->blocks[i].size, error, "read failed in FLAC file", "write failed in WAVE/AIFF file"))
			return false;
	}
	if(fseeko(fout, offset3, SEEK_SET) < 0) {
		if(error) *error = "seek failed in WAVE/AIFF file";
		return false;
	}
	for(i = fm->audio_block+1; i < fm->num_blocks; i++) {
		if(fseeko(fin, fm->blocks[i].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!copy_data_(fin, fout, fm->blocks[i].size, error, "read failed in FLAC file", "write failed in WAVE/AIFF file"))
			return false;
	}
	return true;
}

static FLAC__bool compare_with_iff_(foreign_metadata_t *fm, FILE *fin, FILE *fout, FLAC__off_t offset3, const char **error)
{
	size_t i;

	/* Compare blocks before audio data */
	for(i = 0; i <= (fm->audio_block); i++) {
		if(fseeko(fin, fm->blocks[i].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!compare_data_(fin, fout, fm->blocks[i].size, error, "read failed in FLAC file", "read failed in WAVE/AIFF file",
		      i==0?"stored main chunk length differs from written length":(
		      i==fm->format_block?"stored foreign format block differs from written block. Perhaps the file is being restored to a different format than that of the original file":(
		      i==fm->audio_block?"stored audio length differs from written length. Perhaps the file changed in length after being originally encoded":"restore of foreign metadata failed"))))
			return false;
	}

	/* Seek beyond audio */
	if(fseeko(fout, offset3, SEEK_SET) < 0) {
		if(error) *error = "seek failed in WAVE/AIFF file";
		return false;
	}
	for(; i < fm->num_blocks; i++) {
		if(fseeko(fin, fm->blocks[i].offset, SEEK_SET) < 0) {
			if(error) *error = "seek failed in FLAC file";
			return false;
		}
		if(!compare_data_(fin, fout, fm->blocks[i].size, error, "read failed in FLAC file", "read failed in WAVE/AIFF file", "restore of foreign metadata failed"))
			return false;
	}
	return true;
}

foreign_metadata_t *flac__foreign_metadata_new(foreign_block_type_t type)
{
	/* calloc() to zero all the member variables */
	foreign_metadata_t *x = calloc(1, sizeof(foreign_metadata_t));
	if(x) {
		x->type = type;
		x->is_rf64 = false;
	}
	return x;
}

void flac__foreign_metadata_delete(foreign_metadata_t *fm)
{
	if(fm) {
		if(fm->blocks)
			free(fm->blocks);
		free(fm);
	}
}

FLAC__bool flac__foreign_metadata_read_from_aiff(foreign_metadata_t *fm, const char *filename, const char **error)
{
	FLAC__bool ok;
	FILE *f = flac_fopen(filename, "rb");
	if(!f) {
		if(error) *error = "can't open AIFF file for reading (000)";
		return false;
	}
	ok = read_from_aiff_(fm, f, error);
	fclose(f);
	return ok;
}

FLAC__bool flac__foreign_metadata_read_from_wave(foreign_metadata_t *fm, const char *filename, const char **error)
{
	FLAC__bool ok;
	FILE *f = flac_fopen(filename, "rb");
	if(!f) {
		if(error) *error = "can't open WAVE file for reading (000)";
		return false;
	}
	ok = read_from_wave_(fm, f, error);
	fclose(f);
	return ok;
}

FLAC__bool flac__foreign_metadata_read_from_wave64(foreign_metadata_t *fm, const char *filename, const char **error)
{
	FLAC__bool ok;
	FILE *f = flac_fopen(filename, "rb");
	if(!f) {
		if(error) *error = "can't open Wave64 file for reading (000)";
		return false;
	}
	ok = read_from_wave64_(fm, f, error);
	fclose(f);
	return ok;
}

FLAC__bool flac__foreign_metadata_write_to_flac(foreign_metadata_t *fm, const char *infilename, const char *outfilename, const char **error)
{
	FLAC__bool ok;
	FILE *fin, *fout;
	FLAC__Metadata_SimpleIterator *it = FLAC__metadata_simple_iterator_new();
	if(!it) {
		if(error) *error = "out of memory (000)";
		return false;
	}
	if(!FLAC__metadata_simple_iterator_init(it, outfilename, /*read_only=*/true, /*preserve_file_stats=*/false)) {
		if(error) *error = "can't initialize iterator (001)";
		FLAC__metadata_simple_iterator_delete(it);
		return false;
	}
	if(0 == (fin = flac_fopen(infilename, "rb"))) {
		if(error) *error = "can't open WAVE/AIFF file for reading (002)";
		FLAC__metadata_simple_iterator_delete(it);
		return false;
	}
	if(0 == (fout = flac_fopen(outfilename, "r+b"))) {
		if(error) *error = "can't open FLAC file for updating (003)";
		FLAC__metadata_simple_iterator_delete(it);
		fclose(fin);
		return false;
	}
	ok = write_to_flac_(fm, fin, fout, it, error);
	FLAC__metadata_simple_iterator_delete(it);
	fclose(fin);
	fclose(fout);
	return ok;
}

FLAC__bool flac__foreign_metadata_read_from_flac(foreign_metadata_t *fm, const char *filename, const char **error)
{
	FLAC__bool ok;
	FILE *f;
	FLAC__Metadata_SimpleIterator *it = FLAC__metadata_simple_iterator_new();
	if(!it) {
		if(error) *error = "out of memory (000)";
		return false;
	}
	if(!FLAC__metadata_simple_iterator_init(it, filename, /*read_only=*/true, /*preserve_file_stats=*/false)) {
		if(error) *error = "can't initialize iterator (001)";
		FLAC__metadata_simple_iterator_delete(it);
		return false;
	}
	if(0 == (f = flac_fopen(filename, "rb"))) {
		if(error) *error = "can't open FLAC file for reading (002)";
		FLAC__metadata_simple_iterator_delete(it);
		return false;
	}
	ok = read_from_flac_(fm, f, it, error);
	FLAC__metadata_simple_iterator_delete(it);
	fclose(f);
	return ok;
}

FLAC__bool flac__foreign_metadata_write_to_iff(foreign_metadata_t *fm, const char *infilename, const char *outfilename, FLAC__off_t offset1, FLAC__off_t offset2, FLAC__off_t offset3, const char **error)
{
	FLAC__bool ok;
	FILE *fin, *fout;
	if(0 == (fin = flac_fopen(infilename, "rb"))) {
		if(error) *error = "can't open FLAC file for reading (000)";
		return false;
	}
	if(0 == (fout = flac_fopen(outfilename, "r+b"))) {
		if(error) *error = "can't open WAVE/AIFF file for updating (001)";
		fclose(fin);
		return false;
	}
	ok = write_to_iff_(fm, fin, fout, offset1, offset2, offset3, error);
	fclose(fin);
	fclose(fout);
	return ok;
}

FLAC__bool flac__foreign_metadata_compare_with_iff(foreign_metadata_t *fm, const char *infilename, const char *outfilename, FLAC__off_t offset3, const char **error)
{
	FLAC__bool ok;
	FILE *fin, *fout;
	if(0 == (fin = flac_fopen(infilename, "rb"))) {
		if(error) *error = "can't open FLAC file for reading";
		return false;
	}
	if(0 == (fout = flac_fopen(outfilename, "rb"))) {
		if(error) *error = "can't open WAVE/AIFF file for comparing";
		fclose(fin);
		return false;
	}
	ok = compare_with_iff_(fm, fin, fout, offset3, error);
	fclose(fin);
	fclose(fout);
	return ok;
}
