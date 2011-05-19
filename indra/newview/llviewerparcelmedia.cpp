/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "llviewerprecompiledheaders.h"
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmediaautoplay.h"
#include "llviewerwindow.h"
#include "llfirstuse.h"
#include "llpluginclassmedia.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llnotify.h"
#include "llsdserialize.h"
#include "llaudioengine.h"
#include "lloverlaybar.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
BOOL LLViewerParcelMedia::sManuallyAllowedScriptedMedia = FALSE;
LLUUID LLViewerParcelMedia::sMediaRegionID;
viewer_media_t LLViewerParcelMedia::sMediaImpl;
LLSD LLViewerParcelMedia::sMediaFilterList;
bool LLViewerParcelMedia::sMediaLastActionPlay = FALSE;
std::string LLViewerParcelMedia::sMediaLastURL = "";
bool LLViewerParcelMedia::sAudioLastActionPlay = FALSE;
std::string LLViewerParcelMedia::sAudioLastURL = "";


bool LLViewerParcelMedia::sMediaFilterAlertActive = FALSE;
std::string LLViewerParcelMedia::sQueuedMusic = "";
std::string LLViewerParcelMedia::sCurrentMusic = "";
LLParcel LLViewerParcelMedia::sQueuedMedia;
LLParcel LLViewerParcelMedia::sCurrentMedia;
LLParcel LLViewerParcelMedia::sCurrentAlertMedia;
bool LLViewerParcelMedia::sMediaQueueEmpty = TRUE;
bool LLViewerParcelMedia::sMusicQueueEmpty = TRUE;
U32 LLViewerParcelMedia::sMediaCommandQueue = 0;
F32 LLViewerParcelMedia::sMediaCommandTime = 0;

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_audio_alert(const LLSD& notification, const LLSD& response, std::string media_url);

// static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", processParcelMediaCommandMessage );
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate );
	LLViewerParcelMediaAutoPlay::initClass();
	loadDomainFilterList();
}

//static 
void LLViewerParcelMedia::cleanupClass()
{
	// This needs to be destroyed before global destructor time.
	sMediaImpl = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				sMediaRegionID = LLUUID() ;
				stop() ;
				LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
				return ;				
			}

			// we're in a parcel
			bool new_parcel = false;
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			if (parcelid != sMediaParcelLocalID || regionid != sMediaRegionID)
			{
				LL_DEBUGS("Media") << "New parcel, parcel id = " << parcelid << ", region id = " << regionid << LL_ENDL;
				sMediaParcelLocalID = parcelid;
				sMediaRegionID = regionid;
				new_parcel = true;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			std::string mediaCurrentUrl = std::string( parcel->getMediaCurrentURL());

			// First use warning
			if(	! mediaUrl.empty() && gSavedSettings.getWarning("FirstStreamingVideo") )
			{
				LLNotifications::instance().add("ParcelCanPlayMedia", LLSD(), LLSD(),
					boost::bind(callback_play_media, _1, _2, parcel));
				return;

			}

			// if we have a current (link sharing) url, use it instead
			if (mediaCurrentUrl != "" && parcel->getMediaType() == "text/html")
			{
				mediaUrl = mediaCurrentUrl;
			}
			
			LLStringUtil::trim(mediaUrl);
			
			// If no parcel media is playing, nothing left to do
			if(sMediaImpl.isNull())

			{
				return;
			}

			// Media is playing...has something changed?
			else if (( sMediaImpl->getMediaURL() != mediaUrl )
				|| ( sMediaImpl->getMediaTextureID() != parcel->getMediaID() )
				|| ( sMediaImpl->getMimeType() != parcel->getMediaType() ))
			{
				if(new_parcel && gSavedSettings.getBOOL("PhoenixStopMusicOnParcelChange"))
				{
					llinfos << "Stopping media because parcel changed." << llendl;
					stop();
					sManuallyAllowedScriptedMedia=FALSE;
				}
				else
				// Only play if the media types are the same.
				if(sMediaImpl->getMimeType() == parcel->getMediaType())
				{
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						llinfos << "Filtering media URL." << llendl;
						filterMediaUrl(parcel);
					}
					else
					{
						llinfos << "Media filter disabled. Playing." << llendl;
						play(parcel);
					}
				}

				else
				{
					stop();
				}
			}
		}
		else
		{
			stop();
		}
	}
	/*
	else
	{
		// no audio player, do a first use dialog if there is media here
		if (parcel)
		{
			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			if (!mediaUrl.empty ())
			{
				if (gSavedSettings.getWarning("QuickTimeInstalled"))
				{
					gSavedSettings.setWarning("QuickTimeInstalled", FALSE);

					LLNotifications::instance().add("NoQuickTime" );
				};
			}
		}
	}
	*/
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel)
{
	lldebugs << "LLViewerParcelMedia::play" << llendl;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingVideo"))
		return;

	std::string media_url = parcel->getMediaURL();
	std::string media_current_url = parcel->getMediaCurrentURL();
	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	// Debug print
	// LL_DEBUGS("Media") << "Play media type : " << mime_type << ", url : " << media_url << LL_ENDL;

	if(!sMediaImpl)
	{
		// There is no media impl, make a new one
		sMediaImpl = LLViewerMedia::newMediaImpl(media_url, placeholder_texture_id,
			media_width, media_height, media_auto_scale,
			media_loop, mime_type);
	}
	// If the url and mime type are the same, call play again
	if(sMediaImpl->getMediaURL() == media_url 
		&& sMediaImpl->getMimeType() == mime_type
		&& sMediaImpl->getMediaTextureID() == placeholder_texture_id)
	{
		LL_DEBUGS("Media") << "playing with existing url " << media_url << LL_ENDL;
		sMediaImpl->play();
	}
	// Else if the texture id's are the same, navigate and rediscover type
	// MBW -- This causes other state from the previous parcel (texture size, autoscale, and looping) to get re-used incorrectly.
	// It's also not really necessary -- just creating a new instance is fine.
//		else if(sMediaImpl->getMediaTextureID() == placeholder_texture_id)
//		{
//			sMediaImpl->navigateTo(media_url, mime_type, true);
//		}
	else
	{
		// Since the texture id is different, we need to generate a new impl
		LL_DEBUGS("Media") << "new media impl with mime type " << mime_type << ", url " << media_url << LL_ENDL;

		// Delete the old one first so they don't fight over the texture.
		sMediaImpl->stop();

		sMediaImpl = LLViewerMedia::newMediaImpl(media_url, placeholder_texture_id,
			media_width, media_height, media_auto_scale,
			media_loop, mime_type);
	}

	
	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::stop()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	
	// We need to remove the media HUD if it is up.
	LLViewerMediaFocus::getInstance()->clearFocus();

	// This will kill the media instance.
	sMediaImpl->stop();
	sMediaImpl = NULL;
}

// static
void LLViewerParcelMedia::pause()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->pause();
}

// static
void LLViewerParcelMedia::start()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->start();

	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->seek(time);
}

// static
void LLViewerParcelMedia::focus(bool focus)
{
	sMediaImpl->focus(focus);
}

// static
LLViewerMediaImpl::EMediaStatus LLViewerParcelMedia::getStatus()
{	
	LLViewerMediaImpl::EMediaStatus result = LLViewerMediaImpl::MEDIA_NONE;
	
	if(sMediaImpl.notNull() && sMediaImpl->hasMedia())
	{
		result = sMediaImpl->getMediaPlugin()->getStatus();
	}
	
	return result;
}

// static
std::string LLViewerParcelMedia::getMimeType()
{
	return sMediaImpl.notNull() ? sMediaImpl->getMimeType() : "none/none";
}
viewer_media_t LLViewerParcelMedia::getParcelMedia()
{
	return sMediaImpl;
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg, void ** )
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );
	
	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
			{
				stop();
			}
			else
			{
				llinfos << "Queueing PARCEL_MEDIA_STOP command." << llendl;
				sMediaCommandQueue = PARCEL_MEDIA_COMMAND_STOP;
			}
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
			{
				pause();
			}
			else
			{
				llinfos << "Queueing PARCEL_MEDIA_PAUSE command." << llendl;
				sMediaCommandQueue = PARCEL_MEDIA_COMMAND_PAUSE;
			}
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if( !(gSavedSettings.getBOOL("PhoenixAllowScriptedMedia2") ||
				sManuallyAllowedScriptedMedia||
				gSavedSettings.getBOOL("ParcelMediaAutoPlayEnable") ))
			{
				return;
			}
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					llinfos << "PARCEL_MEDIA_COMMAND_PLAY: Filtering media URL." << llendl;
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
			{
				stop();
			}
			else
			{
				llinfos << "Queueing PARCEL_MEDIA_UNLOAD command." << llendl;
				sMediaCommandQueue = PARCEL_MEDIA_COMMAND_UNLOAD;
			}
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if( !(gSavedSettings.getBOOL("PhoenixAllowScriptedMedia2") ||
			sManuallyAllowedScriptedMedia||
			gSavedSettings.getBOOL("ParcelMediaAutoPlayEnable") ))
		{
			return;
		}
		if(sMediaImpl.isNull())
		{
			LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if (gSavedSettings.getBOOL("MediaEnableFilter"))
			{
				llinfos << "PARCEL_MEDIA_COMMAND_TIME: Filtering media URL." << llendl;
				filterMediaUrl(parcel);
			}
			else
			{
				llinfos << "PARCEL_MEDIA_COMMAND_TIME: Media filter disabled, playing." << llendl;
				play(parcel);
			}
		}
		if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
		{
			seek(time);
		}
		else
		{
			llinfos << "Queueing PARCEL_MEDIA_TIME command." << llendl;
			sMediaCommandQueue = PARCEL_MEDIA_COMMAND_TIME;
			sMediaCommandTime = time;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg, void ** )
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			// Don't filter or play if not already playing.
			if (sMediaImpl.notNull())
			{
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					llinfos << "Parcel media changed. Filtering media URL." << llendl;
					filterMediaUrl(parcel);
				}
				else
				{
					llinfos << "Parcel media changed. Media filter disabled, playing." << llendl;
					play(parcel);
				}
			}
		}
	}
}
// Static
/////////////////////////////////////////////////////////////////////////////////////////
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	std::string region_url = gAgent.getRegion()->getCapability("ParcelNavigateMedia");
	if (!region_url.empty())
	{
		// send navigate event to sim for link sharing
		LLSD body;
		body["agent-id"] = gAgent.getID();
		body["local-id"] = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
		body["url"] = url;
		LLHTTPClient::post(region_url, body, new LLHTTPClient::Responder);
	}
	else
	{
		llwarns << "can't get ParcelNavigateMedia capability" << llendl;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
// virtual 
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << self->getClickTarget() << "\", uri is " << self->getClickURL() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is " << self->getClickUUID() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_STATUS_CHANGED:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_STATUS_CHANGED" << LL_ENDL;
		}
		break;
	};
}

bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		if (gSavedSettings.getBOOL("MediaEnableFilter"))
		{
			llinfos << "Filtering media URL." << llendl;
			LLViewerParcelMedia::filterMediaUrl(parcel);
		}
		else
		{
			llinfos << "Media filter disabled, playing." << llendl;
			LLViewerParcelMedia::play(parcel);
		}
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", FALSE);
	return false;
}

void LLViewerParcelMedia::filterMediaUrl(LLParcel* parcel)
{
	LLParcel *currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	llinfos << "Current media: "+sCurrentMedia.getMediaURL() << llendl;
	llinfos << "New media: "+parcel->getMediaURL() << llendl;
	// If there is no alert active, filter the media and flag media
	//  queue empty.
	if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
	{
		if (parcel->getMediaURL() == sCurrentMedia.getMediaURL())
		{
			llinfos << "Media URL filter: no active alert, same URL as previous: " +parcel->getMediaURL() << llendl;
			sCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName())
			{
				// Only play if we're still there.
				llinfos << "Still on same parcel, playing." << llendl;
				LLViewerParcelMedia::play(parcel);
			}
			sMediaQueueEmpty = true;
			return;
		}
		llinfos << "Media URL filter: no active alert, filtering new URL: "+parcel->getMediaURL() << llendl;
		sMediaQueueEmpty = true;
	}
	// If an alert is active, place the media in the media queue if not the same as previous request
	else
	{
		if (sMediaQueueEmpty == false)
		{
			if (parcel->getMediaURL() != sQueuedMedia.getMediaURL())
			{	
				llinfos << "Media URL filter: active alert, replacing current queued media URL with: "+sQueuedMedia.getMediaURL() << llendl;
				sQueuedMedia = *parcel;
				sMediaQueueEmpty = false;
			}
			sMediaCommandQueue = 0;
			return;
		}
		else
		{
			if (parcel->getMediaURL() != sCurrentMedia.getMediaURL())
			{
				llinfos << "Media URL filter: active alert, nothing queued, adding new queued media URL: "+sQueuedMedia.getMediaURL() << llendl;
				sQueuedMedia = *parcel;
				sMediaQueueEmpty = false;
			}
			sMediaCommandQueue = 0;
			return;
		}
	}

	std::string media_url = parcel->getMediaURL();
	if (media_url.empty())
	{
		llinfos << "Media URL on parcel " << parcel->getName() << " empty, allowing." << llendl;
		// Treat it as allowed; it'll get stopped elsewhere
		sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			// We haven't moved, so let it run.
			llinfos << "Still on same parcel, playing." << llendl;
			LLViewerParcelMedia::play(parcel);
		}
		return;
	}

	if (media_url == sMediaLastURL)
	{
		llinfos << "Media URL " << media_url << " hasn't changed, not filtering." << llendl;
		// Don't bother the user if all we're doing is repeating
		//  ourselves.
		if (sMediaLastActionPlay)
		{
			// We played it last time...so if we're still there...
			llinfos << "Played last time, approving this time." << llendl;
			sCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName())
			{
				// The parcel hasn't changed (we didn't
				//  teleport, or move), so play it again, Sam.
				llinfos << "Still on same parcel, playing." << llendl;
				LLViewerParcelMedia::play(parcel);
			}
		}
		return;
	}

	sMediaLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);
    
	for(S32 i = 0;i<(S32)sMediaFilterList.size();i++)
	{
		std::string listed_domain = sMediaFilterList[i]["domain"].asString();
		if (domain.length() >= listed_domain.length())
		{
			size_t pos = domain.rfind(listed_domain);
			if ((pos != std::string::npos) && 
				(pos == domain.length()-listed_domain.length()))
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}
	if (media_action=="allow")
	{
		llinfos << "Media filter: URL allowed by whitelist: "+parcel->getMediaURL() << llendl;
		sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			LLViewerParcelMedia::play(parcel);
		}
		sMediaLastActionPlay = true;
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Media blocked - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		sMediaLastActionPlay = false;
	}
	else
	{
		llinfos << "Domain for URL " << media_url << " in neither blacklist nor whitelist, asking user." << llendl;
		LLSD args;
		args["MEDIAURL"] = media_url;
		LLViewerParcelMedia::sMediaFilterAlertActive = true;
		LLViewerParcelMedia::sCurrentAlertMedia = *parcel;
		LLParcel* pParcel = &LLViewerParcelMedia::sCurrentAlertMedia;
		LLNotifications::instance().add("MediaAlert", args,LLSD(),boost::bind(callback_media_alert, _1, _2, pParcel));
	}
}

void callback_media_alert(const LLSD &notification, const LLSD &response, LLParcel* parcel)
{
	LLParcel *currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string media_url = parcel->getMediaURL();
	std::string domain = LLViewerParcelMedia::extractDomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;

	LLViewerParcelMedia::sMediaLastActionPlay = false;
	if (option == 0) //allow
	{
		llinfos << "Media URL " << media_url << " allowed by user." << llendl;
		LLViewerParcelMedia::sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			llinfos << "Still on same parcel, playing." << llendl;
			LLViewerParcelMedia::play(parcel);
		}
		LLViewerParcelMedia::sMediaLastActionPlay = true;	
	}
	else if (option == 2) //Blacklist
	{
		llinfos << "Media URL " << media_url << " allowed by user." << llendl;
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
	}
	else if (option == 3) // Whitelist
	{
		llinfos << "Media URL " << media_url << " allowed by user." << llendl;
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		LLViewerParcelMedia::sCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			llinfos << "Still on same parcel, playing." << llendl;
			LLViewerParcelMedia::play(parcel);
		}
		LLViewerParcelMedia::sMediaLastActionPlay = true;
	}
	else if (option != 1) // not deny, so huh?
	{
		llwarns << "Got unexpected reply from notification: "
			<< option << llendl;
	}

	// We've dealt with the alert, so mark it as inactive.
	LLViewerParcelMedia::sMediaFilterAlertActive = false;

	// Check for any queued alerts.
	if (LLViewerParcelMedia::sMusicQueueEmpty == false)
	{
		// There's a queued audio stream. Ask about it.
		llinfos << "Processing queued audio stream next." << llendl;
		LLViewerParcelMedia::filterAudioUrl(LLViewerParcelMedia::sQueuedMusic);
	}
	else if (LLViewerParcelMedia::sMediaQueueEmpty == false)
	{
		// There's a queued media stream. Ask about it.
		llinfos << "Processing queued media stream next." << llendl;
		LLParcel* pParcel = &LLViewerParcelMedia::sQueuedMedia;
		LLViewerParcelMedia::filterMediaUrl(pParcel);
	}
	else if (LLViewerParcelMedia::sMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_STOP command." << llendl;
			LLViewerParcelMedia::stop();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_PAUSE command." << llendl;
			LLViewerParcelMedia::pause();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_UNLOAD command." << llendl;
			LLViewerParcelMedia::stop();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_TIME command." << llendl;
			LLViewerParcelMedia::seek(LLViewerParcelMedia::sMediaCommandTime);
		}
		LLViewerParcelMedia::sMediaCommandQueue = 0;
	}
}

void LLViewerParcelMedia::filterAudioUrl(std::string media_url)
{
	// If there is no alert active, filter the media and flag the music
	//  queue empty.
	if (LLViewerParcelMedia::sMediaFilterAlertActive == false)
	{
		if (media_url == sCurrentMusic)
		{
			llinfos << "Audio URL filter: no active alert, same URL as previous: " + media_url << llendl;
			// The music hasn't changed, so keep playing.
			if (gAudiop != NULL)
			{
				gAudiop->startInternetStream(media_url);
				LLOverlayBar::audioFilterPlay();
			}
			sMusicQueueEmpty = true;
			return;
		}
		// New music, so flag the queue empty and filter it.
		llinfos << "Audio URL filter: no active alert, filtering new URL: " + media_url << llendl;
		sMusicQueueEmpty = true;
	}
	// If an alert is active, place the media url in the music queue
	//  if not the same as previous request.
	else
	{
		if (sMusicQueueEmpty == false)
		{
			if (media_url != sQueuedMusic)
			{
				llinfos << "Audio URL filter: active alert, replacing existing queue with: " + media_url << llendl;
				sQueuedMusic = media_url;
				sMusicQueueEmpty = false;
			}
			return;
		}
		else
		{
			if (media_url != sCurrentMusic)
			{
				llinfos << "Audio URL filter: active alert, nothing queued, adding queue with: " + media_url << llendl;
				sQueuedMusic = media_url;
				sMusicQueueEmpty = false;
				}
			return;
		}
	}	

	sCurrentMusic = media_url;

	// If the new URL is empty, just play it.
	if (media_url.empty())
	{
		llinfos << "Audio URL empty, allowing." << llendl;
		// Treat it as allowed; it'll get stopped elsewhere
		if (gAudiop != NULL)
		{
			gAudiop->startInternetStream(media_url);
			LLOverlayBar::audioFilterPlay();
		}
		return;
	}

	// If this is the same as the last one we asked about, don't bug the
	//  user with it again.
	if (media_url == sAudioLastURL)
	{
		llinfos << "Audio URL " << media_url << " unchanged, repeating action." << llendl;
		if (sAudioLastActionPlay)
		{
			llinfos << "Played last time, playing again." << llendl;
			if (gAudiop != NULL)
			{
				gAudiop->startInternetStream(media_url);
				LLOverlayBar::audioFilterPlay();
			}
		}
		return;
	}

	sAudioLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);
    
	for(S32 i = 0;i<(S32)sMediaFilterList.size();i++)
	{
		std::string listed_domain = sMediaFilterList[i]["domain"].asString();
		if (domain.length() >= listed_domain.length())
		{
			size_t pos = domain.rfind(listed_domain);
			if ((pos != std::string::npos) && 
				(pos == domain.length()-listed_domain.length()))
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}
	if (media_action=="allow")
	{
		if (gAudiop != NULL)
		{
			llinfos << "Audio filter: URL allowed by whitelist" << llendl;
			gAudiop->startInternetStream(media_url);
			LLOverlayBar::audioFilterPlay();
		}
		sAudioLastActionPlay = true;
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Audio blocked - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		LLOverlayBar::audioFilterStop();
		sAudioLastActionPlay = false;
	}
	else
	{
		llinfos << "Audio URL " << media_url << " neither on whitelist nor blacklist, prompting." << llendl;
		LLSD args;
		args["AUDIOURL"] = media_url;
		LLViewerParcelMedia::sMediaFilterAlertActive = true;
		LLNotifications::instance().add("AudioAlert", args,LLSD(),boost::bind(callback_audio_alert, _1, _2, media_url));
	}
}

void callback_audio_alert(const LLSD &notification, const LLSD &response, std::string media_url)
{
	LLViewerParcelMedia::sMediaFilterAlertActive = true;
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string domain = LLViewerParcelMedia::extractDomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;

	if (option== 0) //allow
	{
		llinfos << "Audio URL " << media_url << " allowed by user." << llendl;
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = media_url;
			gAudiop->startInternetStream(media_url);
			LLOverlayBar::audioFilterPlay();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = true;
	}
	else if (option== 1) //deny
	{
		llinfos << "Audio URL " << media_url << " denied by user." << llendl;
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = "";
			gAudiop->stopInternetStream();
			LLOverlayBar::audioFilterStop();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = false;
	}
	else if (option== 2) //Blacklist
	{
		llinfos << "Audio URL " << media_url << " blacklisted by user." << llendl;
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = "";
			gAudiop->stopInternetStream();
			LLOverlayBar::audioFilterStop();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = false;
	}
	else if (option== 3) // Whitelist
	{
		llinfos << "Audio URL " << media_url << " whitelisted by user." << llendl;
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		if (gAudiop != NULL)
		{
			LLViewerParcelMedia::sCurrentMusic = media_url;
			gAudiop->startInternetStream(media_url);
			LLOverlayBar::audioFilterPlay();
		}
		LLViewerParcelMedia::sAudioLastActionPlay = true;
	}
	else // huh?
	{
		llwarns << "Got unexpected reply from notification: "
			<< option << llendl;
	}

	LLViewerParcelMedia::sMediaFilterAlertActive = false;

	// Check for queues
	if (LLViewerParcelMedia::sMusicQueueEmpty == false)
	{
		llinfos << "Processing queued audio stream next." << llendl;
		LLViewerParcelMedia::filterAudioUrl(LLViewerParcelMedia::sQueuedMusic);
	}
	else if (LLViewerParcelMedia::sMediaQueueEmpty == false)
	{
		llinfos << "Processing queued media stream next." << llendl;
		LLParcel* pParcel = &LLViewerParcelMedia::sQueuedMedia;
		LLViewerParcelMedia::filterMediaUrl(pParcel);
	}
	else if (LLViewerParcelMedia::sMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_STOP command." << llendl;
			LLViewerParcelMedia::stop();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_PAUSE command." << llendl;
			LLViewerParcelMedia::pause();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_UNLOAD command." << llendl;
			LLViewerParcelMedia::stop();
		}
		else if (LLViewerParcelMedia::sMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			llinfos << "Executing Queued PARCEL_MEDIA_TIME command." << llendl;
			LLViewerParcelMedia::seek(LLViewerParcelMedia::sMediaCommandTime);
		}
		LLViewerParcelMedia::sMediaCommandQueue = 0;
	}
}

bool LLViewerParcelMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	llofstream medialistFile(medialist_filename);
	LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
	medialistFile.close();
	return true;
}

bool LLViewerParcelMedia::loadDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	if(!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename);
		LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
		medialistFile.close();
	}

	if(LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename);
		LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
		medialistFile.close();
		return true;
	}
	else
	{
		return false;
	}
}

std::string LLViewerParcelMedia::extractDomain(std::string url)
{
	// First, find and strip any protocol prefix.
	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		S32 count = url.size()-pos+2;
		url = url.substr(pos+2, count);
	}

	// Now, look for a / marking a local part; if there is one,
	//  strip it and anything after.
	pos = url.find("/");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}

	// If there's a user{,:password}@ part, remove it,
	pos = url.find("@");

	if (pos != std::string::npos)
	{
		S32 count = url.size()-pos+1;
		url = url.substr(pos+1, count);
	}

	// Finally, find and strip away any port number. This has to be done
	//  after the previous step, or else the extra : for the password,
	//  if supplied, will confuse things.
	pos = url.find(":");  

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}
	
	// Now map the whole thing to lowercase, since domain names aren't
	//  case sensitive.
	std::transform(url.begin(), url.end(),url.begin(), ::tolower);

	return url;
}

// TODO: observer
/*
void LLViewerParcelMediaNavigationObserver::onNavigateComplete( const EventType& event_in )
{
	std::string url = event_in.getStringValue();

	if (mCurrentURL != url && ! mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}

	mCurrentURL = url;
	mFromMessage = false;

}
*/
