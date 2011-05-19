/**
 * @file llimagej2coj.cpp
 * @brief This is an implementation of JPEG2000 encode/decode using OpenJPEG.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2008, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

/* Parts of this file is copyrighted by the OpenJPEG 2000 team. Please is openjpeg.h include file for full copyright notice */

#include "linden_common.h"
#include "llimagej2coj.h"

#define USE_OPJ_DEPRECATED
#include "openjpeg.h"

#include "lltimer.h"
#include "llmemory.h"

class OMVJ2KStream
	{
		U32           mStreamPosition ;
		LLImageJ2C*   mImageJ2C ;
		
	public:
		opj_stream_t* stream;
		
		static OPJ_UINT32 _read (void * p_buffer, OPJ_UINT32 p_nb_bytes, void * p_user_data)
		{
			return ((OMVJ2KStream*)p_user_data)->read( (U8*) p_buffer, (U32) p_nb_bytes) ;
		}
		static OPJ_UINT32 _write (void * p_buffer, OPJ_UINT32 p_nb_bytes, void * p_user_data)
		{
			return ((OMVJ2KStream*)p_user_data)->write((U8*) p_buffer, (U32) p_nb_bytes) ;
		}
		static OPJ_SIZE_T _skip (OPJ_SIZE_T p_nb_bytes, void * p_user_data)
		{
			return ((OMVJ2KStream*)p_user_data)->skip((U32) p_nb_bytes) ;
		}
		static bool _seek (OPJ_SIZE_T p_nb_bytes, void * p_user_data)
		{
			return ((OMVJ2KStream*)p_user_data)->seek((U32) p_nb_bytes) ;
		}
		U32 read ( U8 * p_buffer, U32 p_nb_bytes)
		{
			if(!mImageJ2C->getData())
				return -1 ;
			U32 bytes = p_nb_bytes ;
			if( mStreamPosition + p_nb_bytes >= mImageJ2C->getDataSize() )
			{
				if( mImageJ2C->getDataSize() <= mStreamPosition )
					return -1 ;
				bytes = mImageJ2C->getDataSize() - mStreamPosition ;
			}
			memcpy( p_buffer, mImageJ2C->getData() + mStreamPosition, bytes ) ;
			mStreamPosition += bytes ;
			return bytes ;
		}
		U32 write( U8 * p_buffer, U32 p_nb_bytes)
		{
			S32 newpos = mStreamPosition + p_nb_bytes ;
			if( !mImageJ2C->getData() )
				mImageJ2C->allocateData( (S32) p_nb_bytes ) ;
			else if( newpos > mImageJ2C->getDataSize() )
				mImageJ2C->reallocateData( newpos ) ;
			memcpy( mImageJ2C->getData() + mStreamPosition, p_buffer, p_nb_bytes );
			mStreamPosition = newpos ;
			return p_nb_bytes ;
		}
		U32 skip(U32 p_nb_bytes)  // TODO: return value should be size_t, yet opj defines it as U32 value for now
		{
			mStreamPosition += p_nb_bytes ;
			return p_nb_bytes ;
		}
		bool seek(U32 p_nb_bytes)
		{
			mStreamPosition = p_nb_bytes ;
			return true ;
		}
		OMVJ2KStream( bool writable, LLImageJ2C* llimagej2c )
		{
			mStreamPosition = 0 ;
			mImageJ2C       = llimagej2c ;
			
			stream = opj_stream_create(J2K_STREAM_CHUNK_SIZE, writable);
			
			opj_stream_set_user_data(stream, this);
			opj_stream_set_read_function(stream, _read);
			opj_stream_set_write_function(stream, _write);
			opj_stream_set_skip_function(stream, _skip);
			opj_stream_set_seek_function(stream, _seek);
		}
		~OMVJ2KStream()
		{
			opj_stream_destroy(stream);
		}
	};

const char* fallbackEngineInfoLLImageJ2CImpl()
{
	static std::string version_string =
	std::string("OpenJPEG: " OPENJPEG_VERSION ", Runtime: ")
	+ opj_version();
	return version_string.c_str();
}

LLImageJ2CImpl* fallbackCreateLLImageJ2CImpl()
{
	return new LLImageJ2COJ();
}

void fallbackDestroyLLImageJ2CImpl(LLImageJ2CImpl* impl)
{
	delete impl;
	impl = NULL;
}

/**
 sample error callback expecting a LLFILE* client object
 */
void error_callback(const char* msg, void*)
{
	//	printf("[J2K] %s\n", msg) ;
	lldebugs << "LLImageJ2CImpl error_callback: " << msg << llendl;
}
/**
 sample warning callback expecting a LLFILE* client object
 */
void warning_callback(const char* msg, void*)
{
	//	printf("[J2K] %s\n", msg) ;
	lldebugs << "LLImageJ2CImpl warning_callback: " << msg << llendl;
}
/**
 sample debug callback expecting no client object
 */
void info_callback(const char* msg, void*)
{
	//	printf("[J2K] %s\n", msg) ;
	lldebugs << "LLImageJ2CImpl info_callback: " << msg << llendl;
}


LLImageJ2COJ::LLImageJ2COJ() : LLImageJ2CImpl()
{
}


LLImageJ2COJ::~LLImageJ2COJ()
{
}



BOOL LLImageJ2COJ::decodeImpl(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count)
{
	//
	// FIXME: Get the comment field out of the texture
	//
	
	LLTimer decode_timer;
	
	opj_dparameters_t parameters;	/* decompression parameters */
	opj_image_t *image = NULL;
	
	opj_codec_t* dinfo = NULL;	/* handle to a decompressor */
	
	OMVJ2KStream j2k( true, &base ) ;
	
	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);
	
	parameters.cp_reduce = base.getRawDiscardLevel();
	
	
	/* get a decoder handle */
	dinfo = opj_create_decompress(CODEC_J2K);
	
	/* catch events using our callbacks and give a local context */
	opj_set_info_handler(dinfo, info_callback, NULL);
	opj_set_warning_handler(dinfo, warning_callback, NULL);
	opj_set_error_handler(dinfo, error_callback, NULL);
	
	/* setup the decoder decoding parameters using user parameters */
	opj_setup_decoder(dinfo, &parameters);
	
	OPJ_INT32 x0, y0 ;
	OPJ_UINT32 x1, y1, tiles_x, tiles_y;
	
	bool success = opj_read_header( dinfo, &image, &x0, &y0, &x1, &y1, &tiles_x, &tiles_y, j2k.stream) ;
	if(!success)
	{
		fprintf(stderr, "ERROR -> decodeImpl: failed to decode image header!\n");
		if (image)
		{
			opj_image_destroy(image);
		}
		opj_destroy_codec(dinfo);
		base.mDecoding = FALSE;
		
		return TRUE; // done
	}
	
	image = opj_decode(dinfo, j2k.stream);
	
	if(!image)
	{
		opj_destroy_codec(dinfo);
		raw_image.resize( x1/2, y1/2, 4); //TODO:DZ truncated data stream, we set half size to signal there is more to read, and fill white/alpha. this is temporary, for openjpeg v2 alpha version
		U8 *rawp = raw_image.getData();
		for (S32 y = ( (y1/2) - 1); y >= 0; y--)
		{
			for (S32 x = 0; x < (x1/2); x++)
			{
				*(rawp++) = 255;
				*(rawp++) = 255;
				*(rawp++) = 255;
				*(rawp++) = 127;
			}
		}
		return TRUE;
	}
	
	opj_end_decompress(dinfo, j2k.stream);
	//	opj_destroy_codec(dinfo);
	
	
#if 0
	// sometimes we get bad data out of the cache - check to see if the decode succeeded
	int decompdifference = 0;
	if (cinfo.numdecompos) // sanity
	{
		for (int comp = 0; comp < image->numcomps; comp++)
		{	/* get maximum decomposition level difference, first field is from the COD header and the second
		 is what is actually met in the codestream, NB: if everything was ok, this calculation will
		 return what was set in the cp_reduce value! */
			decompdifference = std::max(decompdifference, cinfo.numdecompos[comp] - image->comps[comp].resno_decoded);
		}
		if (decompdifference < 0) // sanity
		{
			decompdifference = 0;
		}
	}
	
	/* if OpenJPEG failed to decode all requested decomposition levels
	 the difference will be greater than this level */
	if (decompdifference > base.getRawDiscardLevel())
	{
		llwarns << "not enough data for requested discard level, setting mDecoding to FALSE, difference: " << (decompdifference - base.getRawDiscardLevel()) << llendl;
		opj_image_destroy(image);
		
		base.mDecoding = FALSE;
		return TRUE;
	}
	
	if(image->numcomps <= first_channel)
	{
		// sanity
		llwarns << "trying to decode more channels than are present in image: numcomps: " << image->numcomps << " first_channel: " << first_channel << llendl;
		opj_destroy_cstr_info(&cinfo);
		opj_image_destroy(image);
		return TRUE;
	}
#endif
	// Copy image data into our raw image format (instead of the separate channel format
	
	S32 img_components = image->numcomps ;
	S32 channels = img_components - first_channel;
	if( channels > max_channel_count )
		channels = max_channel_count;
	
	// Component buffers are allocated in an image width by height buffer.
	// The image placed in that buffer is ceil(width/2^factor) by
	// ceil(height/2^factor) and if the factor isn't zero it will be at the
	// top left of the buffer with black filled in the rest of the pixels.
	// It is integer math so the formula is written in ceildivpo2.
	// (Assuming all the components have the same width, height and
	// factor.)
	S32 comp_width = image->comps[0].w;
	S32 f=image->comps[0].factor;
	S32 width = ceildivpow2(image->x1 - image->x0, f);
	S32 height = ceildivpow2(image->y1 - image->y0, f);
	raw_image.resize(width, height, channels);
	U8 *rawp = raw_image.getData();
	
	
	// first_channel is what channel to start copying from
	// dest is what channel to copy to.  first_channel comes from the
	// argument, dest always starts writing at channel zero.
	for (S32 comp = first_channel, dest=0; comp < first_channel + channels;
		 comp++, dest++)
	{
		if (image->comps[comp].data)
		{
			S32 offset = dest;
			for (S32 y = (height - 1); y >= 0; y--)
			{
				for (S32 x = 0; x < width; x++)
				{
					rawp[offset] = image->comps[comp].data[y*comp_width + x];
					offset += channels;
				}
			}
		}
		else // Some rare OpenJPEG versions have this bug.
		{
			fprintf(stderr, "ERROR -> decodeImpl: failed to decode image! (NULL comp data - OpenJPEG bug)\n");
			opj_image_destroy(image);
			opj_destroy_codec(dinfo);
			
			return TRUE; // done
		}
	}
	
	/* free opj data structures */
	opj_image_destroy(image);
	opj_destroy_codec(dinfo);
	
	return TRUE; // done
}


BOOL LLImageJ2COJ::encodeImpl(LLImageJ2C &base, const LLImageRaw &raw_image, const char* comment_text, F32 encode_time, BOOL reversible)
{
	const S32 MAX_COMPS = 5;
	opj_cparameters_t parameters;	/* compression parameters */
	
	
	/* set encoding parameters to default values */
	opj_set_default_encoder_parameters(&parameters);
	parameters.cod_format = 0;
	parameters.cp_disto_alloc = 1;
	
	if (reversible)
	{
		parameters.tcp_numlayers = 1;
		parameters.tcp_rates[0] = 0.0f;
	}
	else
	{
		parameters.tcp_numlayers = 5;
		parameters.tcp_rates[0] = 1920.0f;
		parameters.tcp_rates[1] = 480.0f;
		parameters.tcp_rates[2] = 120.0f;
		parameters.tcp_rates[3] = 30.0f;
		parameters.tcp_rates[4] = 10.0f;
		parameters.irreversible = 1;
		if (raw_image.getComponents() == 3) // TODO: if 4 comps and opacity plane is fully opaque, then set mct=1 and numcomps=3 (drop opacity plane), otherwise leave as mct=0 to favor rgb/spatial instead of YUV
		{
			parameters.tcp_mct = 1;
		}
	}
	
	if (!comment_text)
	{
		parameters.cp_comment = (char *) "";
	}
	else
	{
		// Awful hacky cast, too lazy to copy right now.
		parameters.cp_comment = (char *) comment_text;
	}
	
	//
	// Fill in the source image from our raw image
	//
	OPJ_COLOR_SPACE color_space = CLRSPC_SRGB;
	opj_image_cmptparm_t cmptparm[MAX_COMPS];
	opj_image_t * image = NULL;
	S32 numcomps = raw_image.getComponents();
	S32 width = raw_image.getWidth();
	S32 height = raw_image.getHeight();
	
	memset(&cmptparm[0], 0, MAX_COMPS * sizeof(opj_image_cmptparm_t));
	for(S32 c = 0; c < numcomps; c++) {
		cmptparm[c].prec = 8;
		cmptparm[c].bpp = 8;
		cmptparm[c].sgnd = 0;
		cmptparm[c].dx = parameters.subsampling_dx;
		cmptparm[c].dy = parameters.subsampling_dy;
		cmptparm[c].w = width;
		cmptparm[c].h = height;
	}
	
	/* create the image */
	image = opj_image_create(numcomps, &cmptparm[0], color_space);
	
	image->x1 = width;
	image->y1 = height;
	
	S32 i = 0;
	const U8 *src_datap = raw_image.getData();
	for (S32 y = height - 1; y >= 0; y--)
	{
		for (S32 x = 0; x < width; x++)
		{
			const U8 *pixel = src_datap + (y*width + x) * numcomps;
			for (S32 c = 0; c < numcomps; c++)
			{
				image->comps[c].data[i] = *pixel;
				pixel++;
			}
			i++;
		}
	}
	
	
	
	/* encode the destination image */
	/* ---------------------------- */
	
	OMVJ2KStream j2k( false, &base ) ;
	
	/* get a J2K compressor handle */
	opj_codec_t* cinfo = opj_create_compress(CODEC_J2K);
	
	/* catch events using our callbacks and give a local context */
	opj_set_info_handler(cinfo, info_callback, NULL);
	opj_set_warning_handler(cinfo, warning_callback, NULL);
	opj_set_error_handler(cinfo, error_callback, NULL);
	
	/* setup the encoder parameters using the current image and using user parameters */
	opj_setup_encoder(cinfo, &parameters, image);
	
	
	/* encode the image */
	/*if (*indexfilename)					// If need to extract codestream information
	 bSuccess = opj_encode_with_info(cinfo, cio, image, &cstr_info);
	 else*/
	bool bSuccess = opj_start_compress( cinfo, image, j2k.stream );
	bSuccess = bSuccess && opj_encode( cinfo, j2k.stream );
	bSuccess = bSuccess && opj_end_compress( cinfo, j2k.stream );
	
	base.updateData(); // set width, height
	
	/* free remaining compression structures */
	opj_destroy_codec(cinfo);
	
	/* free user parameters structure */
	if(parameters.cp_matrice) free(parameters.cp_matrice);
	
	/* free image data */
	opj_image_destroy(image);
	return TRUE;
}

BOOL LLImageJ2COJ::getMetadata(LLImageJ2C &base)
{
	//
	// FIXME: We get metadata by decoding the ENTIRE image.
	//
	
	// Update the raw discard level
	base.updateRawDiscardLevel();
	
	opj_dparameters_t parameters;	/* decompression parameters */
	opj_image_t *image = NULL;
	
	opj_codec_t* dinfo = NULL;	/* handle to a decompressor */
	
	
	/* set decoding parameters to default values */
	opj_set_default_decoder_parameters(&parameters);
	
	//parameters.cp_reduce = mRawDiscardLevel;
	
	/* decode the code-stream */
	/* ---------------------- */
	
	/* JPEG-2000 codestream */
	OMVJ2KStream j2k( true, &base ) ;
	
	/* get a decoder handle */
	dinfo = opj_create_decompress(CODEC_J2K);
	
	/* catch events using our callbacks and give a local context */
	// opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);
	
	/* setup the decoder decoding parameters using user parameters */
	opj_setup_decoder(dinfo, &parameters);
	
	OPJ_INT32 x0, y0 ;
	OPJ_UINT32 x1, y1, tiles_x, tiles_y;
	
	bool bResult = opj_read_header( dinfo, &image, &x0, &y0, &x1, &y1, &tiles_x, &tiles_y, j2k.stream) ;
	
	// TODO:DZ 	try extract numcomps: if(! opj_read_tile_header( dinfo, &l_tile_index, &l_data_size, &l_current_tile_x0, &l_current_tile_y0, &l_current_tile_x1, &l_current_tile_y1, &l_nb_comps, &l_go_on, j2k.stream))
	
	/* free remaining structures */
	if(dinfo)
	{
		opj_destroy_codec(dinfo);
	}
	if(!bResult)
	{
		fprintf(stderr, "ERROR -> getMetadata: failed to decode image!\n");
		return FALSE;
	}
	
	base.setSize( x1, y1, 4);
	
	return TRUE;
}