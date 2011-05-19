/*
	Copyright Chris Rehor (Liny Odell) 2010.
	Licensed under the artistic license version 2.
	http://www.perlfoundation.org/artistic_license_2_0
*/


#include "llviewerprecompiledheaders.h"

#include "floatermediaplayer.h"

#include "lltexturectrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llfloaterurlentry_mp.h"
#include "llfilepicker.h"
#include "llpluginclassmedia.h"



F64 FloaterMediaPlayer::media_length;
F64 FloaterMediaPlayer::media_time;
bool FloaterMediaPlayer::length_check;
viewer_media_t FloaterMediaPlayer::sMPMediaImpl;
FloaterMediaPlayer* FloaterMediaPlayer::sInstance = NULL;
std::string FloaterMediaPlayer::mp_url;

FloaterMediaPlayer::FloaterMediaPlayer() :  LLFloater(std::string("PhoenixMediaPlayer"))
{
}

FloaterMediaPlayer::~FloaterMediaPlayer()
{
}

BOOL FloaterMediaPlayer::handleKeyHere(KEY key, MASK mask)
{
	if (( KEY_DELETE == key ) && (MASK_NONE == mask))
	{
 		sInstance->mMPPlayList->deleteSelectedItems();
		return TRUE;
	}
	else if(( 'A' == key ) && (MASK_CONTROL == mask))
	{
 		sInstance->mMPPlayList->selectAll();
		return TRUE;
	}
	return false;
}

void FloaterMediaPlayer::showInstance()
{
	if (sInstance)
	{
		if(!sInstance->getVisible())
		{
			sInstance->open();
		}
	}
	else
	{
		sInstance = new FloaterMediaPlayer();
		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_media_player.xml");
	}
}
								
void FloaterMediaPlayer::draw()
{
	LLFloater::draw();
}

void FloaterMediaPlayer::onOpen()
{
	sInstance->setVisible(TRUE);
}

void FloaterMediaPlayer::onClose(bool app_quitting)
{
	sInstance->setVisible(FALSE);
	if( app_quitting )
	{
		onClickMPStop(NULL);
		destroy();
	}
}

BOOL FloaterMediaPlayer::postBuild()
{
	mMPMediaImage = getChild<LLTextureCtrl>("mp_media_image");
	if (mMPMediaImage)
	{
		mMPMediaImage->setImageAssetID(LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361"));
	}
	childSetCommitCallback("mp_seek",onMPSeek,this);
	childSetAction("mp_prev", onClickMPPrev,this);
	childSetAction("mp_play", onClickMPPlay,this);
	childSetAction("mp_pause", onClickMPPause,this);
	childSetAction("mp_next", onClickMPNext,this);
	childSetAction("mp_stop", onClickMPStop,this);
	childSetAction("mp_add_file", onClickMPAddFile,this);
	childSetAction("mp_add_URL", onClickMPAddURL,this);
	childSetAction("mp_rem", onClickMPRem,this);
	mMPPlayList = getChild<LLScrollListCtrl>("mp_playlist");
	mMPPlayList->setDoubleClickCallback(onDoubleClick);
	return TRUE;
}

void FloaterMediaPlayer::onMPSeek(LLUICtrl* ctrl,void *userdata)
{
	if(sMPMediaImpl)
	{
		F32 seek_pos = sInstance->childGetValue("mp_seek").asReal();
		//seek_pos = (seek_pos/100) * media_length;
		sMPMediaImpl->seek(seek_pos);
	}
}

void FloaterMediaPlayer::onClickMPPrev( void* userdata )
{
	sInstance->mMPPlayList->selectPrevItem(FALSE);
	if(sMPMediaImpl)
	{
		sInstance->childSetValue("mp_seek",0.0f);
		mp_url = sInstance->mMPPlayList->getSelectedValue().asString();
		sMPMediaImpl->navigateTo(mp_url, "video/*");
		sMPMediaImpl->play();
		length_check = false;
		sInstance->childSetVisible("mp_play",false);
		sInstance->childSetVisible("mp_pause",true);
	}
}

void FloaterMediaPlayer::onClickMPPlay( void* userdata )
{
	if(!sMPMediaImpl)
	{
		mp_url = sInstance->mMPPlayList->getSelectedValue().asString();
		// There is no media impl, make a new one
		sMPMediaImpl = LLViewerMedia::newMediaImpl(mp_url, LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361"),
			599, 359, 1,
			0, "video/*");
		sMPMediaImpl->play();
		length_check = false;
		sMPMediaImpl->addObserver(sInstance);
	}
	else
	{
		//There is one, re-use it and resume play
		sMPMediaImpl->start();
	}
	sInstance->childSetVisible("mp_play",false);
	sInstance->childSetVisible("mp_pause",true);
}

void FloaterMediaPlayer::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	if(event == MEDIA_EVENT_NAME_CHANGED)
	{
		sInstance->childSetValue("mp_np_text",self->getMediaName());
	}
	else if(event == MEDIA_EVENT_STATUS_CHANGED)
	{
		EMediaStatus status = self->getStatus();
		if(status == MEDIA_DONE || status == MEDIA_PAUSED || status == MEDIA_ERROR)
		{
			if(status == MEDIA_DONE || status == MEDIA_ERROR)
			{
				length_check = false;
			}
			if(status == MEDIA_ERROR)
			{
				onClickMPStop(NULL);
			}
			//else if(status == MEDIA_DONE)
#if 0
			{
				if(sInstance->mMPPlayList->getFirstSelectedIndex() == sInstance->mMPPlayList->getItemCount())
				{
					if(1)//Loop (repeat) code
					{
						sInstance->mMPPlayList->selectFirstItem();
						onClickMPPlay(NULL);
					}
					else
					{
						onClickMPStop(NULL);
					}
				}
				else
				{
					onClickMPNext(NULL);
					onClickMPPlay(NULL);
				}
			}
#endif
		}
	}
	else if(event == MEDIA_EVENT_TIME_DURATION_UPDATED)
	{
		if(sInstance)
		{
			media_length = self->getDuration();
			media_time = self->getCurrentTime();
			if(!length_check && (media_length != -1.0F))
			{
				sInstance->childSetMaxValue("mp_seek",media_length);
				length_check = true;
			}
			sInstance->childSetValue("mp_seek",media_time);
		}
	}
}

void FloaterMediaPlayer::onClickMPPause( void* userdata )
{
	if(sMPMediaImpl)
	{
		sMPMediaImpl->pause();
		sInstance->childSetVisible("mp_pause",false);
		sInstance->childSetVisible("mp_play",true);
	}
}

void FloaterMediaPlayer::onClickMPNext( void* userdata )
{
	sInstance->mMPPlayList->selectNextItem(FALSE);
	if(sMPMediaImpl)
	{
		mp_url = sInstance->mMPPlayList->getSelectedValue().asString();
		sMPMediaImpl->navigateTo(mp_url, "video/*");
		sMPMediaImpl->play();
		length_check = false;
		sInstance->childSetVisible("mp_play",false);
		sInstance->childSetVisible("mp_pause",true);
		sInstance->childSetValue("mp_seek",0.0f);
	}
}

void FloaterMediaPlayer::onClickMPStop( void* userdata )
{
	if(sMPMediaImpl)
	{
		length_check = false;
		sInstance->childSetValue("mp_seek",0.0f);
		sInstance->childSetVisible("mp_pause",false);
		sInstance->childSetVisible("mp_play",true);
		sMPMediaImpl->stop();
		sMPMediaImpl->destroyMediaSource();
		sMPMediaImpl = NULL;
	}
}

void FloaterMediaPlayer::onClickMPAddFile( void* userdata )
{
	LLFilePicker& picker = LLFilePicker::instance();
	if ( !picker.getMultipleOpenFiles(LLFilePicker::FFLOAD_MEDIA) ) 
	{
		return;
	}
	std::string filename = picker.getFirstFile();	
	while( !filename.empty() )
	{
		filename = "file://"+filename;
		sInstance->mMPPlayList->addSimpleElement(filename);
		filename = picker.getNextFile();
	}
}

void FloaterMediaPlayer::onClickMPAddURL( void* userdata )
{
	FloaterMediaPlayer *self = (FloaterMediaPlayer *)userdata;
	self->mMPURLEntryFloater = LLFloaterMPURLEntry::show( self->getHandle() );
	LLFloater* parent_floater = gFloaterView->getParentFloater(self);
	if (parent_floater)
	{
		parent_floater->addDependentFloater(self->mMPURLEntryFloater.get());
	}
}

void FloaterMediaPlayer::addMediaURL(const std::string& media_url)
{
	sInstance->mMPPlayList->addSimpleElement(media_url);
}

void FloaterMediaPlayer::onClickMPRem( void* userdata )
{
	sInstance->mMPPlayList->deleteSelectedItems();
}

void FloaterMediaPlayer::onDoubleClick( void* userdata )
{
	if(!sMPMediaImpl)
	{
		onClickMPPlay(NULL);
	}
	else
	{
		mp_url = sInstance->mMPPlayList->getSelectedValue().asString();
		sMPMediaImpl->navigateTo(mp_url, "video/*");
		sMPMediaImpl->play();
		length_check = false;
		sInstance->childSetVisible("mp_play",false);
		sInstance->childSetVisible("mp_pause",true);
	}
}
