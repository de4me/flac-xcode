/* metaflac - Command-line FLAC metadata editor
 * Copyright (C) 2001-2009  Josh Coalson
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

#include "operations.h"
#include "usage.h"
#include "utils.h"
#include "FLAC/assert.h"
#include "FLAC/metadata.h"
#include "share/alloc.h"
#include "share/grabbag.h"
#include "share/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "operations_shorthand.h"

static void show_version(void);
static FLAC__bool do_major_operation(const CommandLineOptions *options);
static FLAC__bool do_major_operation_on_file(const char *filename, const CommandLineOptions *options);
static FLAC__bool do_major_operation__list(const char *filename, FLAC__Metadata_Chain *chain, const CommandLineOptions *options);
static FLAC__bool do_major_operation__append(FLAC__Metadata_Chain *chain, const CommandLineOptions *options);
static FLAC__bool do_major_operation__remove(FLAC__Metadata_Chain *chain, const CommandLineOptions *options);
static FLAC__bool do_major_operation__remove_all(FLAC__Metadata_Chain *chain, const CommandLineOptions *options);
static FLAC__bool do_shorthand_operations(const CommandLineOptions *options);
static FLAC__bool do_shorthand_operations_on_file(const char *filename, const CommandLineOptions *options);
static FLAC__bool do_shorthand_operation(const char *filename, FLAC__bool prefix_with_filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write, FLAC__bool utf8_convert);
static FLAC__bool do_shorthand_operation__add_replay_gain(char **filenames, unsigned num_files, FLAC__bool preserve_modtime, FLAC__bool scan);
static FLAC__bool do_shorthand_operation__add_padding(const char *filename, FLAC__Metadata_Chain *chain, unsigned length, FLAC__bool *needs_write);

static FLAC__bool passes_filter(const CommandLineOptions *options, const FLAC__StreamMetadata *block, unsigned block_number);
static void write_metadata(const char *filename, FLAC__StreamMetadata *block, unsigned block_number, FLAC__bool raw, FLAC__bool hexdump_application);
static void write_metadata_binary(FLAC__StreamMetadata *block, FLAC__byte *block_raw, FLAC__bool headerless);

/* from operations_shorthand_seektable.c */
extern FLAC__bool do_shorthand_operation__add_seekpoints(const char *filename, FLAC__Metadata_Chain *chain, const char *specification, FLAC__bool *needs_write);

/* from operations_shorthand_streaminfo.c */
extern FLAC__bool do_shorthand_operation__streaminfo(const char *filename, FLAC__bool prefix_with_filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write);

/* from operations_shorthand_vorbiscomment.c */
extern FLAC__bool do_shorthand_operation__vorbis_comment(const char *filename, FLAC__bool prefix_with_filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write, FLAC__bool raw);

/* from operations_shorthand_cuesheet.c */
extern FLAC__bool do_shorthand_operation__cuesheet(const char *filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write);

/* from operations_shorthand_picture.c */
extern FLAC__bool do_shorthand_operation__picture(const char *filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write);


FLAC__bool do_operations(const CommandLineOptions *options)
{
	FLAC__bool ok = true;

#ifdef _WIN32
	if(options->utf8_convert) {
		_setmode(fileno(stdout),_O_U8TEXT);
		SetConsoleOutputCP(CP_UTF8);
	}
#endif

	if(options->show_long_help) {
		long_usage(0);
	}
	if(options->show_version) {
		show_version();
	}
	else if(options->args.checks.num_major_ops > 0) {
		FLAC__ASSERT(options->args.checks.num_shorthand_ops == 0);
		FLAC__ASSERT(options->args.checks.num_major_ops == 1);
		FLAC__ASSERT(options->args.checks.num_major_ops == options->ops.num_operations);
		ok = do_major_operation(options);
	}
	else if(options->args.checks.num_shorthand_ops > 0) {
		FLAC__ASSERT(options->args.checks.num_shorthand_ops == options->ops.num_operations);
		ok = do_shorthand_operations(options);
	}

	return ok;
}

/*
 * local routines
 */

void show_version(void)
{
	printf("metaflac %s\n", FLAC__VERSION_STRING);
}

FLAC__bool do_major_operation(const CommandLineOptions *options)
{
	unsigned i;
	FLAC__bool ok = true;

	/* to die after first error,     v---  add '&& ok' here */
	for(i = 0; i < options->num_files; i++)
		ok &= do_major_operation_on_file(options->filenames[i], options);

	return ok;
}

FLAC__bool do_major_operation_on_file(const char *filename, const CommandLineOptions *options)
{
	FLAC__bool ok = true, needs_write = false, is_ogg = false;
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	if(0 == chain)
		die("out of memory allocating chain");

	/*@@@@ lame way of guessing the file type */
	if(strlen(filename) >= 4 && (0 == strcmp(filename+strlen(filename)-4, ".oga") || 0 == strcmp(filename+strlen(filename)-4, ".ogg")))
		is_ogg = true;

	if(! (is_ogg? FLAC__metadata_chain_read_ogg(chain, filename) : FLAC__metadata_chain_read(chain, filename)) ) {
		print_error_with_chain_status(chain, "%s: ERROR: reading metadata", filename);
		FLAC__metadata_chain_delete(chain);
		return false;
	}

	switch(options->ops.operations[0].type) {
		case OP__LIST:
			ok = do_major_operation__list(options->prefix_with_filename? filename : 0, chain, options);
			break;
		case OP__APPEND:
			ok = do_major_operation__append(chain, options);
			needs_write = true;
			break;
		case OP__REMOVE:
			ok = do_major_operation__remove(chain, options);
			needs_write = true;
			break;
		case OP__REMOVE_ALL:
			ok = do_major_operation__remove_all(chain, options);
			needs_write = true;
			break;
		case OP__MERGE_PADDING:
			FLAC__metadata_chain_merge_padding(chain);
			needs_write = true;
			break;
		case OP__SORT_PADDING:
			FLAC__metadata_chain_sort_padding(chain);
			needs_write = true;
			break;
		default:
			FLAC__ASSERT(0);
			return false;
	}

	if(ok && needs_write) {
		if(options->use_padding)
			FLAC__metadata_chain_sort_padding(chain);
		if(options->output_name == 0)
			ok = FLAC__metadata_chain_write(chain, options->use_padding, options->preserve_modtime);
		else
			ok = FLAC__metadata_chain_write_new_file(chain, options->output_name, options->use_padding);
		if(!ok) {
			FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status(chain);
			print_error_with_chain_status(chain, "%s: ERROR: writing FLAC file", filename);
			if(status == FLAC__METADATA_CHAIN_STATUS_RENAME_ERROR)
				flac_fprintf(stderr, "NOTE: rename errors often occur when working with symlinks pointing to a different filesystem\n");
		}
	}

	FLAC__metadata_chain_delete(chain);

	return ok;
}

FLAC__bool do_major_operation__list(const char *filename, FLAC__Metadata_Chain *chain, const CommandLineOptions *options)
{
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
	FLAC__StreamMetadata *block;
	FLAC__bool ok = true;
	unsigned block_number;

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	block_number = 0;
	do {
		block = FLAC__metadata_iterator_get_block(iterator);
		ok &= (0 != block);
		if(!ok)
			flac_fprintf(stderr, "%s: ERROR: couldn't get block from chain\n", filename);
		else if(passes_filter(options, FLAC__metadata_iterator_get_block(iterator), block_number)) {
			if(!options->data_format_is_binary && !options->data_format_is_binary_headerless)
				write_metadata(filename, block, block_number, !options->utf8_convert, options->application_data_format_is_hexdump);
			else {
				FLAC__byte * block_raw = FLAC__metadata_object_get_raw(block);
				if(block_raw == 0) {
					flac_fprintf(stderr, "%s: ERROR: couldn't get block in raw form\n", filename);
					FLAC__metadata_iterator_delete(iterator);
					return false;
				}
				write_metadata_binary(block, block_raw, options->data_format_is_binary_headerless);
#ifdef _WIN32
				if(options->utf8_convert)
					_setmode(fileno(stdout),_O_U8TEXT);
				else
					_setmode(fileno(stdin),_O_TEXT);
#endif
				free(block_raw);
			}
		}
		block_number++;
	} while(ok && FLAC__metadata_iterator_next(iterator));

	FLAC__metadata_iterator_delete(iterator);

	return ok;
}

FLAC__bool do_major_operation__append(FLAC__Metadata_Chain *chain, const CommandLineOptions *options)
{
	FLAC__byte header[FLAC__STREAM_METADATA_HEADER_LENGTH];
	FLAC__byte *buffer;
	FLAC__uint32 buffer_size, num_objects = 0, i, append_after = UINT32_MAX;
	FLAC__StreamMetadata *object;
	FLAC__Metadata_Iterator *iterator = 0;
	FLAC__bool has_vorbiscomment = false;

	/* First, find out after which block appending should take place */
	for(i = 0; i < options->args.num_arguments; i++) {
		if(options->args.arguments[i].type == ARG__BLOCK_NUMBER) {
			if(append_after != UINT32_MAX || options->args.arguments[i].value.block_number.num_entries > 1)	{
				flac_fprintf(stderr, "ERROR: more than one block number specified with --append\n");
				return false;
			}
			append_after = options->args.arguments[i].value.block_number.entries[0];
		}
	}

	iterator = FLAC__metadata_iterator_new();

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	/* Find out whether there is already a vorbis comment block present */
	do {
		FLAC__MetadataType type = FLAC__metadata_iterator_get_block_type(iterator);
		if(type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
			has_vorbiscomment = true;
	}
	while(FLAC__metadata_iterator_next(iterator));

	/* Reset iterator */
	FLAC__metadata_iterator_init(iterator, chain);

	/* Go to requested block */
	for(i = 0; i < append_after; i++) {
		if(!FLAC__metadata_iterator_next(iterator))
			break;
	}

#ifdef _WIN32
	_setmode(fileno(stdin),_O_BINARY);
#endif

	/* Read header from stdin */
	while(fread(header, 1, FLAC__STREAM_METADATA_HEADER_LENGTH, stdin) == FLAC__STREAM_METADATA_HEADER_LENGTH) {

		buffer_size = ((FLAC__uint32)(header[1]) << 16) + ((FLAC__uint32)(header[2]) << 8) + header[3];
		buffer = safe_malloc_(buffer_size + FLAC__STREAM_METADATA_HEADER_LENGTH);
		if(0 == buffer)
			die("out of memory allocating read buffer");
		memcpy(buffer, header, FLAC__STREAM_METADATA_HEADER_LENGTH);

		num_objects++;

		if(fread(buffer+FLAC__STREAM_METADATA_HEADER_LENGTH, 1, buffer_size, stdin) < buffer_size) {
			flac_fprintf(stderr, "ERROR: couldn't read metadata block #%u from stdin\n",(num_objects));
			free(buffer);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}

		if((object = FLAC__metadata_object_set_raw(buffer, buffer_size + FLAC__STREAM_METADATA_HEADER_LENGTH)) == NULL) {
			flac_fprintf(stderr, "ERROR: couldn't parse supplied metadata block #%u\n",(num_objects));
			free(buffer);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}
		free(buffer);

		if(has_vorbiscomment && object->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			flac_fprintf(stderr, "ERROR: can't add another vorbis comment block to file, it already has one\n");
			FLAC__metadata_object_delete(object);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}


		if(object->type == FLAC__METADATA_TYPE_STREAMINFO) {
			flac_fprintf(stderr, "ERROR: can't add streaminfo to file\n");
			FLAC__metadata_object_delete(object);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}

		if(object->type == FLAC__METADATA_TYPE_SEEKTABLE) {
			flac_fprintf(stderr, "ERROR: can't add seektable to file, please use --add-seekpoint instead\n");
			FLAC__metadata_object_delete(object);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}

		if(!FLAC__metadata_iterator_insert_block_after(iterator, object)) {
			flac_fprintf(stderr, "ERROR: couldn't add supplied metadata block #%u to file\n",(num_objects));
			FLAC__metadata_object_delete(object);
			FLAC__metadata_iterator_delete(iterator);
			return false;
		}
		/* Now check whether what type of block was added */
		{
			FLAC__MetadataType type = FLAC__metadata_iterator_get_block_type(iterator);
			if(type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
				has_vorbiscomment = true;
		}
	}

#ifdef _WIN32
	if(options->utf8_convert)
		_setmode(fileno(stdout),_O_U8TEXT);
	else
		_setmode(fileno(stdin),_O_TEXT);
#endif

	if(num_objects == 0)
		flac_fprintf(stderr, "ERROR: unable to find a metadata block in the supplied input\n");

	FLAC__metadata_iterator_delete(iterator);

	return true;
}

FLAC__bool do_major_operation__remove(FLAC__Metadata_Chain *chain, const CommandLineOptions *options)
{
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
	FLAC__bool ok = true;
	unsigned block_number;

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	block_number = 0;
	while(ok && FLAC__metadata_iterator_next(iterator)) {
		block_number++;
		if(passes_filter(options, FLAC__metadata_iterator_get_block(iterator), block_number)) {
			ok &= FLAC__metadata_iterator_delete_block(iterator, options->use_padding);
			if(options->use_padding)
				ok &= FLAC__metadata_iterator_next(iterator);
		}
	}

	FLAC__metadata_iterator_delete(iterator);

	return ok;
}

FLAC__bool do_major_operation__remove_all(FLAC__Metadata_Chain *chain, const CommandLineOptions *options)
{
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
	FLAC__bool ok = true;

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	while(ok && FLAC__metadata_iterator_next(iterator)) {
		ok &= FLAC__metadata_iterator_delete_block(iterator, options->use_padding);
		if(options->use_padding)
			ok &= FLAC__metadata_iterator_next(iterator);
	}

	FLAC__metadata_iterator_delete(iterator);

	return ok;
}

FLAC__bool do_shorthand_operations(const CommandLineOptions *options)
{
	unsigned i;
	FLAC__bool ok = true;

	/* to die after first error,     v---  add '&& ok' here */
	for(i = 0; i < options->num_files; i++)
		ok &= do_shorthand_operations_on_file(options->filenames[i], options);

	/* check if OP__ADD_REPLAY_GAIN requested */
	if(ok && options->num_files > 0) {
		for(i = 0; i < options->ops.num_operations; i++) {
			if(options->ops.operations[i].type == OP__ADD_REPLAY_GAIN)
				ok = do_shorthand_operation__add_replay_gain(options->filenames, options->num_files, options->preserve_modtime, false);
			else if(options->ops.operations[i].type == OP__SCAN_REPLAY_GAIN)
				ok = do_shorthand_operation__add_replay_gain(options->filenames, options->num_files, options->preserve_modtime, true);
		}
	}

	return ok;
}

FLAC__bool do_shorthand_operations_on_file(const char *filename, const CommandLineOptions *options)
{
	unsigned i;
	FLAC__bool ok = true, needs_write = false, use_padding = options->use_padding;
	FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

	if(0 == chain)
		die("out of memory allocating chain");

	if(!FLAC__metadata_chain_read(chain, filename)) {
		print_error_with_chain_status(chain, "%s: ERROR: reading metadata", filename);
		ok = false;
		goto cleanup;
	}

	for(i = 0; i < options->ops.num_operations && ok; i++) {
		/*
		 * Do OP__ADD_SEEKPOINT last to avoid decoding twice if both
		 * --add-seekpoint and --import-cuesheet-from are used.
		 */
		if(options->ops.operations[i].type != OP__ADD_SEEKPOINT)
			ok &= do_shorthand_operation(filename, options->prefix_with_filename, chain, &options->ops.operations[i], &needs_write, options->utf8_convert);

		/* The following seems counterintuitive but the meaning
		 * of 'use_padding' is 'try to keep the overall metadata
		 * to its original size, adding or truncating extra
		 * padding if necessary' which is why we need to turn it
		 * off in this case.  If we don't, the extra padding block
		 * will just be truncated.
		 */
		if(options->ops.operations[i].type == OP__ADD_PADDING)
			use_padding = false;
	}

	/*
	 * Do OP__ADD_SEEKPOINT last to avoid decoding twice if both
	 * --add-seekpoint and --import-cuesheet-from are used.
	 */
	for(i = 0; i < options->ops.num_operations && ok; i++) {
		if(options->ops.operations[i].type == OP__ADD_SEEKPOINT)
			ok &= do_shorthand_operation(filename, options->prefix_with_filename, chain, &options->ops.operations[i], &needs_write, options->utf8_convert);
	}

	if(ok && needs_write) {
		if(use_padding)
			FLAC__metadata_chain_sort_padding(chain);
		if(options->output_name == 0)
			ok = FLAC__metadata_chain_write(chain, use_padding, options->preserve_modtime);
		else
			ok = FLAC__metadata_chain_write_new_file(chain, options->output_name, use_padding);
		if(!ok) {
			FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status(chain);
			print_error_with_chain_status(chain, "%s: ERROR: writing FLAC file", filename);
			if(status == FLAC__METADATA_CHAIN_STATUS_RENAME_ERROR)
				flac_fprintf(stderr, "NOTE: rename errors often occur when working with symlinks pointing to a different filesystem\n");
		}
	}

  cleanup :
	FLAC__metadata_chain_delete(chain);

	return ok;
}

FLAC__bool do_shorthand_operation(const char *filename, FLAC__bool prefix_with_filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write, FLAC__bool utf8_convert)
{
	FLAC__bool ok = true;

	switch(operation->type) {
		case OP__SHOW_MD5SUM:
		case OP__SHOW_MIN_BLOCKSIZE:
		case OP__SHOW_MAX_BLOCKSIZE:
		case OP__SHOW_MIN_FRAMESIZE:
		case OP__SHOW_MAX_FRAMESIZE:
		case OP__SHOW_SAMPLE_RATE:
		case OP__SHOW_CHANNELS:
		case OP__SHOW_BPS:
		case OP__SHOW_TOTAL_SAMPLES:
		case OP__SET_MD5SUM:
		case OP__SET_MIN_BLOCKSIZE:
		case OP__SET_MAX_BLOCKSIZE:
		case OP__SET_MIN_FRAMESIZE:
		case OP__SET_MAX_FRAMESIZE:
		case OP__SET_SAMPLE_RATE:
		case OP__SET_CHANNELS:
		case OP__SET_BPS:
		case OP__SET_TOTAL_SAMPLES:
			ok = do_shorthand_operation__streaminfo(filename, prefix_with_filename, chain, operation, needs_write);
			break;
		case OP__SHOW_VC_VENDOR:
		case OP__SHOW_VC_FIELD:
		case OP__REMOVE_VC_ALL:
		case OP__REMOVE_VC_ALL_EXCEPT:
		case OP__REMOVE_VC_FIELD:
		case OP__REMOVE_VC_FIRSTFIELD:
		case OP__SET_VC_FIELD:
		case OP__IMPORT_VC_FROM:
		case OP__EXPORT_VC_TO:
			ok = do_shorthand_operation__vorbis_comment(filename, prefix_with_filename, chain, operation, needs_write, !utf8_convert);
			break;
		case OP__IMPORT_CUESHEET_FROM:
		case OP__EXPORT_CUESHEET_TO:
			ok = do_shorthand_operation__cuesheet(filename, chain, operation, needs_write);
			break;
		case OP__IMPORT_PICTURE_FROM:
		case OP__EXPORT_PICTURE_TO:
			ok = do_shorthand_operation__picture(filename, chain, operation, needs_write);
			break;
		case OP__ADD_SEEKPOINT:
			ok = do_shorthand_operation__add_seekpoints(filename, chain, operation->argument.add_seekpoint.specification, needs_write);
			break;
		case OP__ADD_REPLAY_GAIN:
		case OP__SCAN_REPLAY_GAIN:
			/* these commands are always executed last */
			ok = true;
			break;
		case OP__ADD_PADDING:
			ok = do_shorthand_operation__add_padding(filename, chain, operation->argument.add_padding.length, needs_write);
			break;
		default:
			ok = false;
			FLAC__ASSERT(0);
			break;
	};

	return ok;
}

FLAC__bool do_shorthand_operation__add_replay_gain(char **filenames, unsigned num_files, FLAC__bool preserve_modtime, FLAC__bool scan)
{
	FLAC__StreamMetadata streaminfo;
	float *title_gains = 0, *title_peaks = 0;
	float album_gain, album_peak;
	unsigned sample_rate = 0;
	unsigned bits_per_sample = 0;
	unsigned channels = 0;
	unsigned i;
	const char *error;
	FLAC__bool first = true;

	FLAC__ASSERT(num_files > 0);

	for(i = 0; i < num_files; i++) {
		FLAC__ASSERT(0 != filenames[i]);
		if(!FLAC__metadata_get_streaminfo(filenames[i], &streaminfo)) {
			flac_fprintf(stderr, "%s: ERROR: can't open file or get STREAMINFO block\n", filenames[i]);
			return false;
		}
		if(first) {
			first = false;
			sample_rate = streaminfo.data.stream_info.sample_rate;
			bits_per_sample = streaminfo.data.stream_info.bits_per_sample;
			channels = streaminfo.data.stream_info.channels;
		}
		else {
			if(sample_rate != streaminfo.data.stream_info.sample_rate) {
				flac_fprintf(stderr, "%s: ERROR: sample rate of %u Hz does not match previous files' %u Hz\n", filenames[i], streaminfo.data.stream_info.sample_rate, sample_rate);
				return false;
			}
			if(bits_per_sample != streaminfo.data.stream_info.bits_per_sample) {
				flac_fprintf(stderr, "%s: ERROR: resolution of %u bps does not match previous files' %u bps\n", filenames[i], streaminfo.data.stream_info.bits_per_sample, bits_per_sample);
				return false;
			}
			if(channels != streaminfo.data.stream_info.channels) {
				flac_fprintf(stderr, "%s: ERROR: # channels (%u) does not match previous files' (%u)\n", filenames[i], streaminfo.data.stream_info.channels, channels);
				return false;
			}
		}
		if(!grabbag__replaygain_is_valid_sample_frequency(sample_rate)) {
			flac_fprintf(stderr, "%s: ERROR: sample rate of %u Hz is not supported\n", filenames[i], sample_rate);
			return false;
		}
		if(channels != 1 && channels != 2) {
			flac_fprintf(stderr, "%s: ERROR: # of channels (%u) is not supported, must be 1 or 2\n", filenames[i], channels);
			return false;
		}
		if(bits_per_sample < FLAC__MIN_BITS_PER_SAMPLE || bits_per_sample > FLAC__MAX_BITS_PER_SAMPLE) {
			flac_fprintf(stderr, "%s: ERROR: resolution (%u) is not supported, must be between %u and %u\n", filenames[i], bits_per_sample, FLAC__MIN_BITS_PER_SAMPLE, FLAC__MAX_BITS_PER_SAMPLE);
			return false;
		}
	}

	if(!grabbag__replaygain_init(sample_rate)) {
		FLAC__ASSERT(0);
		/* double protection */
		flac_fprintf(stderr, "internal error\n");
		return false;
	}

	if(
		0 == (title_gains = safe_malloc_mul_2op_(sizeof(float), /*times*/num_files)) ||
		0 == (title_peaks = safe_malloc_mul_2op_(sizeof(float), /*times*/num_files))
	)
		die("out of memory allocating space for title gains/peaks");

	for(i = 0; i < num_files; i++) {
		if(0 != (error = grabbag__replaygain_analyze_file(filenames[i], title_gains+i, title_peaks+i))) {
			flac_fprintf(stderr, "%s: ERROR: during analysis (%s)\n", filenames[i], error);
			free(title_gains);
			free(title_peaks);
			return false;
		}
	}
	grabbag__replaygain_get_album(&album_gain, &album_peak);

	for(i = 0; i < num_files; i++) {
		if(!scan) {
			if(0 != (error = grabbag__replaygain_store_to_file(filenames[i], album_gain, album_peak, title_gains[i], title_peaks[i], preserve_modtime))) {
				flac_fprintf(stderr, "%s: ERROR: writing tags (%s)\n", filenames[i], error);
				free(title_gains);
				free(title_peaks);
				return false;
			}
		} else {
			flac_fprintf(stdout, "%s: %f %f %f %f\n", filenames[i], album_gain, album_peak, title_gains[i], title_peaks[i]);
		}
	}

	free(title_gains);
	free(title_peaks);
	return true;
}

FLAC__bool do_shorthand_operation__add_padding(const char *filename, FLAC__Metadata_Chain *chain, unsigned length, FLAC__bool *needs_write)
{
	FLAC__StreamMetadata *padding = 0;
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	while(FLAC__metadata_iterator_next(iterator))
		;

	padding = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
	if(0 == padding)
		die("out of memory allocating PADDING block");

	padding->length = length;

	if(!FLAC__metadata_iterator_insert_block_after(iterator, padding)) {
		print_error_with_chain_status(chain, "%s: ERROR: adding new PADDING block to metadata", filename);
		FLAC__metadata_object_delete(padding);
		FLAC__metadata_iterator_delete(iterator);
		return false;
	}

	FLAC__metadata_iterator_delete(iterator);
	*needs_write = true;
	return true;
}

FLAC__bool passes_filter(const CommandLineOptions *options, const FLAC__StreamMetadata *block, unsigned block_number)
{
	unsigned i, j;
	FLAC__bool matches_number = false, matches_type = false;
	FLAC__bool has_block_number_arg = false;

	for(i = 0; i < options->args.num_arguments; i++) {
		if(options->args.arguments[i].type == ARG__BLOCK_TYPE || options->args.arguments[i].type == ARG__EXCEPT_BLOCK_TYPE) {
			for(j = 0; j < options->args.arguments[i].value.block_type.num_entries; j++) {
				if(options->args.arguments[i].value.block_type.entries[j].type == block->type) {
					if(block->type != FLAC__METADATA_TYPE_APPLICATION || !options->args.arguments[i].value.block_type.entries[j].filter_application_by_id || 0 == memcmp(options->args.arguments[i].value.block_type.entries[j].application_id, block->data.application.id, FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8))
						matches_type = true;
				}
			}
		}
		else if(options->args.arguments[i].type == ARG__BLOCK_NUMBER) {
			has_block_number_arg = true;
			for(j = 0; j < options->args.arguments[i].value.block_number.num_entries; j++) {
				if(options->args.arguments[i].value.block_number.entries[j] == block_number)
					matches_number = true;
			}
		}
	}

	if(!has_block_number_arg)
		matches_number = true;

	if(options->args.checks.has_block_type) {
		FLAC__ASSERT(!options->args.checks.has_except_block_type);
	}
	else if(options->args.checks.has_except_block_type)
		matches_type = !matches_type;
	else
		matches_type = true;

	return matches_number && matches_type;
}

void write_metadata(const char *filename, FLAC__StreamMetadata *block, unsigned block_number, FLAC__bool raw, FLAC__bool hexdump_application)
{
	unsigned i, j;

/*@@@ yuck, should do this with a varargs function or something: */
#define PPR if(filename) { if(raw) printf("%s:",filename); else flac_printf("%s:",filename); }
	PPR; flac_printf("METADATA block #%u\n", block_number);
	PPR; flac_printf("  type: %u (%s)\n", (unsigned)block->type, block->type < FLAC__METADATA_TYPE_UNDEFINED? FLAC__MetadataTypeString[block->type] : "UNKNOWN");
	PPR; flac_printf("  is last: %s\n", block->is_last? "true":"false");
	PPR; flac_printf("  length: %u\n", block->length);

	switch(block->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			PPR; flac_printf("  minimum blocksize: %u samples\n", block->data.stream_info.min_blocksize);
			PPR; flac_printf("  maximum blocksize: %u samples\n", block->data.stream_info.max_blocksize);
			PPR; flac_printf("  minimum framesize: %u bytes\n", block->data.stream_info.min_framesize);
			PPR; flac_printf("  maximum framesize: %u bytes\n", block->data.stream_info.max_framesize);
			PPR; flac_printf("  sample_rate: %u Hz\n", block->data.stream_info.sample_rate);
			PPR; flac_printf("  channels: %u\n", block->data.stream_info.channels);
			PPR; flac_printf("  bits-per-sample: %u\n", block->data.stream_info.bits_per_sample);
			PPR; flac_printf("  total samples: %" PRIu64 "\n", block->data.stream_info.total_samples);
			PPR; flac_printf("  MD5 signature: ");
			for(i = 0; i < 16; i++) {
				if(raw)
					printf("%02x", (unsigned)block->data.stream_info.md5sum[i]);
				else
					flac_printf("%02x", (unsigned)block->data.stream_info.md5sum[i]);
			}
			if(raw)
				printf("\n");
			else
				flac_printf("\n");
			break;
		case FLAC__METADATA_TYPE_PADDING:
			/* nothing to print */
			break;
		case FLAC__METADATA_TYPE_APPLICATION:
			PPR; flac_printf("  application ID: ");
			for(i = 0; i < 4; i++)
				flac_printf("%02x", block->data.application.id[i]);
			flac_printf("\n");
			PPR; flac_printf("  data contents:\n");
			if(0 != block->data.application.data) {
				if(hexdump_application)
					hexdump(filename, block->data.application.data, block->length - FLAC__STREAM_METADATA_HEADER_LENGTH, "    ");
				else
					for(i = 0; i < block->length - FLAC__STREAM_METADATA_HEADER_LENGTH; i++)
						if(raw)
							(void) local_fwrite(block->data.application.data, 1, block->length - FLAC__STREAM_METADATA_HEADER_LENGTH, stdout);
						else {
							if(block->data.application.data[i] > 32 && block->data.application.data[i] < 127)
								flac_printf("%c",block->data.application.data[i]);
							else {
								char replacement[4] = {0xef, 0xbf, 0xbd, 0}; /* Unicode replacement character */
								flac_printf("%s",replacement);
							}
						}
			}
			break;
		case FLAC__METADATA_TYPE_SEEKTABLE:
			PPR; flac_printf("  seek points: %u\n", block->data.seek_table.num_points);
			for(i = 0; i < block->data.seek_table.num_points; i++) {
				if(block->data.seek_table.points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER) {
					PPR; flac_printf("    point %u: sample_number=%" PRIu64 ", stream_offset=%" PRIu64 ", frame_samples=%u\n", i, block->data.seek_table.points[i].sample_number, block->data.seek_table.points[i].stream_offset, block->data.seek_table.points[i].frame_samples);
				}
				else {
					PPR; flac_printf("    point %u: PLACEHOLDER\n", i);
				}
			}
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			PPR; flac_printf("  vendor string: ");
			write_vc_field(0, &block->data.vorbis_comment.vendor_string, raw, stdout);
			PPR; flac_printf("  comments: %u\n", block->data.vorbis_comment.num_comments);
			for(i = 0; i < block->data.vorbis_comment.num_comments; i++) {
				PPR; flac_printf("    comment[%u]: ", i);
				write_vc_field(0, &block->data.vorbis_comment.comments[i], raw, stdout);
			}
			break;
		case FLAC__METADATA_TYPE_CUESHEET:
			PPR; flac_printf("  media catalog number: %s\n", block->data.cue_sheet.media_catalog_number);
			PPR; flac_printf("  lead-in: %" PRIu64 "\n", block->data.cue_sheet.lead_in);
			PPR; flac_printf("  is CD: %s\n", block->data.cue_sheet.is_cd? "true":"false");
			PPR; flac_printf("  number of tracks: %u\n", block->data.cue_sheet.num_tracks);
			for(i = 0; i < block->data.cue_sheet.num_tracks; i++) {
				const FLAC__StreamMetadata_CueSheet_Track *track = block->data.cue_sheet.tracks+i;
				const FLAC__bool is_last = (i == block->data.cue_sheet.num_tracks-1);
				const FLAC__bool is_leadout = is_last && track->num_indices == 0;
				PPR; flac_printf("    track[%u]\n", i);
				PPR; flac_printf("      offset: %" PRIu64 "\n", track->offset);
				if(is_last) {
					PPR; flac_printf("      number: %u (%s)\n", (unsigned)track->number, is_leadout? "LEAD-OUT" : "INVALID");
				}
				else {
					PPR; flac_printf("      number: %u\n", (unsigned)track->number);
				}
				if(!is_leadout) {
					PPR; flac_printf("      ISRC: %s\n", track->isrc);
					PPR; flac_printf("      type: %s\n", track->type == 1? "DATA" : "AUDIO");
					PPR; flac_printf("      pre-emphasis: %s\n", track->pre_emphasis? "true":"false");
					PPR; flac_printf("      number of index points: %u\n", track->num_indices);
					for(j = 0; j < track->num_indices; j++) {
						const FLAC__StreamMetadata_CueSheet_Index *indx = track->indices+j;
						PPR; flac_printf("        index[%u]\n", j);
						PPR; flac_printf("          offset: %" PRIu64 "\n", indx->offset);
						PPR; flac_printf("          number: %u\n", (unsigned)indx->number);
					}
				}
			}
			break;
		case FLAC__METADATA_TYPE_PICTURE:
			PPR; flac_printf("  type: %u (%s)\n", block->data.picture.type, block->data.picture.type < FLAC__STREAM_METADATA_PICTURE_TYPE_UNDEFINED? FLAC__StreamMetadata_Picture_TypeString[block->data.picture.type] : "UNDEFINED");
			PPR; flac_printf("  MIME type: %s\n", block->data.picture.mime_type);
			PPR; flac_printf("  description: %s\n", block->data.picture.description);
			PPR; flac_printf("  width: %u\n", (unsigned)block->data.picture.width);
			PPR; flac_printf("  height: %u\n", (unsigned)block->data.picture.height);
			PPR; flac_printf("  depth: %u\n", (unsigned)block->data.picture.depth);
			PPR; flac_printf("  colors: %u%s\n", (unsigned)block->data.picture.colors, block->data.picture.colors? "" : " (unindexed)");
			PPR; flac_printf("  data length: %u\n", (unsigned)block->data.picture.data_length);
			PPR; flac_printf("  data:\n");
			if(0 != block->data.picture.data)
				hexdump(filename, block->data.picture.data, block->data.picture.data_length, "    ");
			break;
		default:
			PPR; flac_printf("  data contents:\n");
			if(0 != block->data.unknown.data)
				hexdump(filename, block->data.unknown.data, block->length, "    ");
			break;
	}
#undef PPR
}

void write_metadata_binary(FLAC__StreamMetadata *block, FLAC__byte *block_raw, FLAC__bool headerless)
{
#ifdef _WIN32
	fflush(stdout);
	_setmode(fileno(stdout),_O_BINARY);
#endif
	if(!headerless)
		local_fwrite(block_raw, 1, block->length+FLAC__STREAM_METADATA_HEADER_LENGTH, stdout);
	else if(block->type == FLAC__METADATA_TYPE_APPLICATION && block->length > 3)
		local_fwrite(block_raw+FLAC__STREAM_METADATA_HEADER_LENGTH+4, 1, block->length-4, stdout);
	else
		local_fwrite(block_raw+FLAC__STREAM_METADATA_HEADER_LENGTH, 1, block->length, stdout);
#ifdef _WIN32
	fflush(stdout);
#endif
}
