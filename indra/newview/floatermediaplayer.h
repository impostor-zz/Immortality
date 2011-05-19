/*
	Copyright Chris Rehor (Liny Odell) 2010.
	Licensed under the artistic license version 2.
	http://www.perlfoundation.org/artistic_license_2_0
*/


#ifndef FLOATER_MEDIA_PLAYER_H
#define FLOATER_MEDIA_PLAYER_H

#include "llviewerprecompiledheaders.h"
#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "llviewermedia.h"

class LLTextureCtrl;

class FloaterMediaPlayer : public LLFloater, public LLViewerMediaObserver
{
private:
	FloaterMediaPlayer();

public:
	~FloaterMediaPlayer();

	virtual BOOL	handleKeyHere(KEY key, MASK mask);

	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void onOpen();
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();

	static void toggle(void*); //Toggles interface visibility
	static void showInstance();
	static void addMediaURL(const std::string& media_url);
	void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);

	static FloaterMediaPlayer* getInstance(){ return sInstance; }

private:
	static FloaterMediaPlayer* sInstance;
	LLScrollListCtrl*	mMPPlayList;
	static void onMPSeek(LLUICtrl* ctrl,void *userdata);
	static void onClickMPPrev( void* userdata );
	static void onClickMPPlay( void* userdata );
	static void onClickMPPause( void* userdata );
	static void onClickMPNext( void* userdata );
	static void onClickMPStop( void* userdata );
	static void onClickMPAddFile( void* userdata );
	static void onClickMPAddURL( void* userdata );
	static void onClickMPRem( void* userdata );
	static void onDoubleClick(void *userdata);
	static F64 media_length;
	static F64 media_time;
	static bool length_check;
	static viewer_media_t sMPMediaImpl;
	static std::string mp_url;
	LLTextureCtrl*	mMPMediaImage;
	LLHandle<LLFloater>	mMPURLEntryFloater;
};
#endif // FLOATER_MEDIA_PLAYER_H
