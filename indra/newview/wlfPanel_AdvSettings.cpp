/* Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "wlfPanel_AdvSettings.h"

#include "llbutton.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "lliconctrl.h"
#include "lloverlaybar.h"
#include "lltextbox.h"
#include "llcombobox.h"
#include "llsliderctrl.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llstartup.h"

#include "llfloaterwindlight.h"
#include "llfloaterwater.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

BOOL firstBuildDone;
void* fixPointer;
std::string ButtonState;

wlfPanel_AdvSettings::wlfPanel_AdvSettings()
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);
	build();
}
void wlfPanel_AdvSettings::build()
{
	deleteAllChildren();
	if (!gSavedSettings.getBOOL("wlfAdvSettingsPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "wlfPanel_AdvSettings_expanded.xml", &getFactoryMap());
		ButtonState = "arrow_up.tga";
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "wlfPanel_AdvSettings.xml", &getFactoryMap());
		ButtonState = "arrow_down.tga";
	}
}

void wlfPanel_AdvSettings::refresh()
{
// [RLVa:KB] - Checked: 2009-09-19
	if ( (rlv_handler_t::isEnabled()) && (gSavedSettings.getBOOL("wlfAdvSettingsPopup")) )
	{
		childSetEnabled("EnvAdvancedWaterButton", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WWPresetsCombo", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WWprev", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WWnext", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("EnvAdvancedSkyButton", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WLPresetsCombo", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WLprev", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("WLnext", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
		childSetEnabled("EnvTimeSlider", !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
	}
// [/RLVa:KB]
}

void wlfPanel_AdvSettings::fixPanel()
{
	if(!firstBuildDone)
	{
		llinfos << "firstbuild done" << llendl;
		firstBuildDone = TRUE;
		onClickExpandBtn(fixPointer);
	}
}

BOOL wlfPanel_AdvSettings::postBuild()
{
	childSetAction("expand", onClickExpandBtn, this);
	
	LLComboBox* WWcomboBox = getChild<LLComboBox>("WWPresetsCombo");
	if(WWcomboBox != NULL) {
		std::map<std::string, LLWaterParamSet>::iterator mIt =
			LLWaterParamManager::instance()->mParamList.begin();
		for(; mIt != LLWaterParamManager::instance()->mParamList.end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				WWcomboBox->add(mIt->first);
		}
		WWcomboBox->add(LLStringUtil::null);
		WWcomboBox->setSimple(LLWaterParamManager::instance()->mCurParams.mName);
		WWcomboBox->setCommitCallback(onChangeWWPresetName);
	}

	LLComboBox* WLcomboBox = getChild<LLComboBox>("WLPresetsCombo");
	if(WLcomboBox != NULL) {
		std::map<std::string, LLWLParamSet>::iterator mIt = 
			LLWLParamManager::instance()->mParamList.begin();
		for(; mIt != LLWLParamManager::instance()->mParamList.end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				WLcomboBox->add(mIt->first);
		}
		WLcomboBox->add(LLStringUtil::null);
		WLcomboBox->setSimple(LLWLParamManager::instance()->mCurParams.mName);
		WLcomboBox->setCommitCallback(onChangeWLPresetName);
	}
	
	// next/prev buttons
	childSetAction("WWnext", onClickWWNext, this);
	childSetAction("WWprev", onClickWWPrev, this);
	childSetAction("WLnext", onClickWLNext, this);
	childSetAction("WLprev", onClickWLPrev, this);
	
	childSetAction("EnvAdvancedSkyButton", onOpenAdvancedSky, NULL);
	childSetAction("EnvAdvancedWaterButton", onOpenAdvancedWater, NULL);
	
	childSetCommitCallback("EnvTimeSlider", onChangeDayTime, NULL);
	
	fixPointer = this;
	return TRUE;
}

void wlfPanel_AdvSettings::draw()
{
	LLButton* expand_button = getChild<LLButton>("expand");
/*	if (expand_button)
	{
		if (expand_button->getToggleState())
		{
			expand_button->setImageOverlay("arrow_down.tga");
		}
		else
		{
			expand_button->setImageOverlay("arrow_up.tga");
		}
	} */
			expand_button->setImageOverlay(ButtonState);
	refresh();
	LLPanel::draw();
}

wlfPanel_AdvSettings::~wlfPanel_AdvSettings ()
{
}

void wlfPanel_AdvSettings::onClickExpandBtn(void* user_data)
{
	gSavedSettings.setBOOL("wlfAdvSettingsPopup",!gSavedSettings.getBOOL("wlfAdvSettingsPopup"));
	wlfPanel_AdvSettings* remotep = (wlfPanel_AdvSettings*)user_data;
	remotep->build();
	gOverlayBar->layoutButtons();
}

void wlfPanel_AdvSettings::onChangeWWPresetName(LLUICtrl* ctrl, void * userData)
{
	LLComboBox * combo_box = static_cast<LLComboBox*>(ctrl);
	
	if(combo_box->getSimple() == "")
	{
		return;
	}

	// LLWaterParamManager::instance()->mAnimator.mIsRunning = false;
	// LLWaterParamManager::instance()->mAnimator.mUseLindenTime = false;
	LLWaterParamManager::instance()->loadPreset(
		combo_box->getSelectedValue().asString());
}

void wlfPanel_AdvSettings::onChangeWLPresetName(LLUICtrl* ctrl, void * userData)
{
	LLComboBox * combo_box = static_cast<LLComboBox*>(ctrl);
	
	if(combo_box->getSimple() == "")
	{
		return;
	}

	LLWLParamManager::instance()->mAnimator.mIsRunning = false;
	LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
	LLWLParamManager::instance()->loadPreset(
		combo_box->getSelectedValue().asString());
}

void wlfPanel_AdvSettings::onClickWWNext(void* user_data)
{
	wlfPanel_AdvSettings* self = (wlfPanel_AdvSettings*) user_data;
	
	LLWaterParamManager * param_mgr = LLWaterParamManager::instance();
	LLWaterParamSet& currentParams = param_mgr->mCurParams;

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = 
		param_mgr->mParamList.find(currentParams.mName);

	// if at the end, loop
	std::map<std::string, LLWaterParamSet>::iterator last = param_mgr->mParamList.end(); --last;
	if(mIt == last) 
	{
		mIt = param_mgr->mParamList.begin();
	}
	else
	{
		mIt++;
	}
	/*param_mgr->mAnimator.mIsRunning = false;
	param_mgr->mAnimator.mUseLindenTime = false;*/
	param_mgr->loadPreset(mIt->first, true);
	LLComboBox* comboBox = self->getChild<LLComboBox>("WWPresetsCombo");
	comboBox->setSimple(mIt->first);
}

void wlfPanel_AdvSettings::onClickWWPrev(void* user_data)
{
	wlfPanel_AdvSettings* self = (wlfPanel_AdvSettings*) user_data;
	
	LLWaterParamManager * param_mgr = LLWaterParamManager::instance();
	LLWaterParamSet & currentParams = param_mgr->mCurParams;

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = 
		param_mgr->mParamList.find(currentParams.mName);

	// if at the beginning, loop
	if(mIt == param_mgr->mParamList.begin()) 
	{
		std::map<std::string, LLWaterParamSet>::iterator last = param_mgr->mParamList.end(); --last;
		mIt = last;
	}
	else
	{
		mIt--;
	}
	/*param_mgr->mAnimator.mIsRunning = false;
	param_mgr->mAnimator.mUseLindenTime = false;*/
	param_mgr->loadPreset(mIt->first, true);
	LLComboBox* comboBox = self->getChild<LLComboBox>("WWPresetsCombo");
	comboBox->setSimple(mIt->first);
}

void wlfPanel_AdvSettings::onClickWLNext(void* user_data)
{
	wlfPanel_AdvSettings* self = (wlfPanel_AdvSettings*) user_data;
	
	// find place of current param
	std::map<std::string, LLWLParamSet>::iterator mIt = 
		LLWLParamManager::instance()->mParamList.find(LLWLParamManager::instance()->mCurParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == LLWLParamManager::instance()->mParamList.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the end, loop
	std::map<std::string, LLWLParamSet>::iterator last = LLWLParamManager::instance()->mParamList.end(); --last;
	if(mIt == last) 
	{
		mIt = LLWLParamManager::instance()->mParamList.begin();
	}
	else
	{
		mIt++;
	}
	LLWLParamManager::instance()->mAnimator.mIsRunning = false;
	LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
	LLWLParamManager::instance()->loadPreset(mIt->first, true);
	LLComboBox* comboBox = self->getChild<LLComboBox>("WLPresetsCombo");
	comboBox->setSimple(mIt->first);
}

void wlfPanel_AdvSettings::onClickWLPrev(void* user_data)
{
	wlfPanel_AdvSettings* self = (wlfPanel_AdvSettings*) user_data;
	
	// find place of current param
	std::map<std::string, LLWLParamSet>::iterator mIt = 
		LLWLParamManager::instance()->mParamList.find(LLWLParamManager::instance()->mCurParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == LLWLParamManager::instance()->mParamList.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the beginning, loop
	if(mIt == LLWLParamManager::instance()->mParamList.begin()) 
	{
		std::map<std::string, LLWLParamSet>::iterator last = LLWLParamManager::instance()->mParamList.end(); --last;
		mIt = last;
	}
	else
	{
		mIt--;
	}
	LLWLParamManager::instance()->mAnimator.mIsRunning = false;
	LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
	LLWLParamManager::instance()->loadPreset(mIt->first, true);
	LLComboBox* comboBox = self->getChild<LLComboBox>("WLPresetsCombo");
	comboBox->setSimple(mIt->first);
}

void wlfPanel_AdvSettings::onOpenAdvancedSky(void* userData)
{
	LLFloaterWindLight::show();
}

void wlfPanel_AdvSettings::onOpenAdvancedWater(void* userData)
{
	LLFloaterWater::show();
}

void wlfPanel_AdvSettings::onChangeDayTime(LLUICtrl* ctrl, void* userData)
{
	LLSliderCtrl* sldr = (LLSliderCtrl*) ctrl;

	if (sldr) {
		// deactivate animator
		LLWLParamManager::instance()->mAnimator.mIsRunning = false;
		LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;

		F32 val = sldr->getValueF32() + 0.25f;
		if(val > 1.0) 
		{
			val--;
		}

		LLWLParamManager::instance()->mAnimator.setDayTime((F64)val);
		LLWLParamManager::instance()->mAnimator.update(
			LLWLParamManager::instance()->mCurParams);
	}
}
