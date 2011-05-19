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

#include "jc_lslviewerbridge.h"
#include "llchatbar.h"
#include "llagent.h"
#include "llappviewer.h"
#include "stdtypes.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "lluuid.h"
#include "llfilepicker.h"
#include "llassetstorage.h"
#include "llviewerobjectlist.h"
#include "llviewerinventory.h"
#include "importtracker.h"
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "llinventorymodel.h"
#include "lltooldraganddrop.h"
#include "llmd5.h"
#include "llstartup.h"
#include "llcontrol.h"
#include "llviewercontrol.h"
#include "llviewergenericmessage.h"
#include "llviewerwindow.h"
#include "floaterao.h"
#include "llattachmentsmgr.h"
#include <boost/regex.hpp>
#include <string>

// The old version checking code depends on the version number being the last
//  thing in the name, separated by something that isn't a digit or a dot; on
//  the major and minor version numbers being the same in the defines below;
//  and on it being in the form of "<major>.<minor>". If you change any of
//  these, you MUST update the IsAnOldBridge function to match. -- TS
#define phoenix_bridge_name "#LSL<->Client Bridge v0.13"
#define PHOENIX_BRIDGE_MAJOR_VERSION 0
#define PHOENIX_BRIDGE_MINOR_VERSION 13

const boost::regex AnyBridgePattern("^#LSL<->Client Bridge.*");

void cmdline_printchat(std::string message);

U8 JCLSLBridge::sBridgeStatus;
JCLSLBridge* JCLSLBridge::sInstance;
BOOL JCLSLBridge::sBuildBridge;

LLViewerObject* JCLSLBridge::sBridgeObject;

U32 JCLSLBridge::lastcall;

std::map<U32,JCBridgeCallback*> JCLSLBridge::callback_map;

S32 JCLSLBridge::l2c;
bool JCLSLBridge::l2c_inuse;

LLViewerInventoryItem*  JCLSLBridge::mBridge;


JCLSLBridge::JCLSLBridge() : LLEventTimer( (F32)1.0 )
{
	sBuildBridge = gSavedSettings.getBOOL("PhoenixBuildBridge");
	gSavedSettings.getControl("PhoenixBuildBridge")->getSignal()->connect(boost::bind(&updateBuildBridge));
	//cmdline_printchat("--instanciated bridge");
	lastcall = 0;
	l2c = 0;
	l2c_inuse = false;
	//getPermissions();
}

bool JCLSLBridge::updateBuildBridge()
{
	BOOL newvalue = gSavedSettings.getBOOL("PhoenixBuildBridge");
	if(sBuildBridge != newvalue)
	{
		if(newvalue)
		{
			instance().Reset();
		}
		else
		{
			//cmdline_printchat("--LSLBridge process terminated.");
			sBridgeStatus = FAILED;
		}
		sBuildBridge = newvalue;
	}
	return true;
}


JCLSLBridge::~JCLSLBridge()
{
}

void JCLSLBridge::send_chat_to_object(std::string& chat, S32 channel, LLUUID target)
{
	if(target.isNull())target = gAgent.getID();
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage(_PREHASH_ScriptDialogReply);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgent.getID());
	msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_ObjectID, target);
	msg->addS32(_PREHASH_ChatChannel, channel);
	msg->addS32(_PREHASH_ButtonIndex, 0);
	msg->addString(_PREHASH_ButtonLabel, chat);
	gAgent.sendReliableMessage();
}
struct n2kdat
{
	LLUUID source;
	S32 channel;
	std::string reply;
};
void callbackname2key(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data)
{
	n2kdat* dat = (n2kdat*)data; 
	std::string send = dat->reply + first+" "+last;
	JCLSLBridge::send_chat_to_object(send, dat->channel, dat->source);
	delete dat;
	//if(id == subjectA.owner_id)sInstance->childSetValue("owner_a_name", first + " " + last);
	//else if(id == subjectB.owner_id)sInstance->childSetValue("owner_b_name", first + " " + last);
}

bool JCLSLBridge::lsltobridge(std::string message, std::string from_name, LLUUID source_id, LLUUID owner_id)
{
	if(message == "someshit")
	{
		//cmdline_printchat("--got someshit from "+source_id.asString());
		
		return true;
	}else
	{
		std::string clip = message.substr(0,5);
		if(clip == "#@#@#")
		{
			std::string rest = message.substr(5);
			LLSD arguments = JCLSLBridge::parse_string_to_list(rest, '|');
			//cmdline_printchat(std::string(LLSD::dumpXML(arguments)));
			U32 call = atoi(arguments[0].asString().c_str());
			if(call)
			{
				arguments.erase(0);
				//cmdline_printchat(std::string(LLSD::dumpXML(arguments)));
				callback_fire(call, arguments);
				return true;
			}
		}else if(clip == "#@#@!")
		{
			std::string rest = message.substr(5);
			LLSD args = JCLSLBridge::parse_string_to_list(rest, '|');
			std::string cmd = args[0].asString();
			//std::string uniq = args[1].asString();
			if(cmd == "getsex")
			{
				std::string uniq = args[1].asString();
				LLUUID target = LLUUID(args[2].asString());
				S32	channel = atoi(args[3].asString().c_str());
				LLViewerObject* obj = gObjectList.findObject(target);
				if(obj && obj->isAvatar())
				{
					LLVOAvatar* av = (LLVOAvatar*)obj;
					std::string msg = llformat("getsexreply|%s|%d",uniq.c_str(),av->getVisualParamWeight("male"));
					send_chat_to_object(msg, channel, source_id);
				}else
				{
					std::string msg = llformat("getsexreply|%s|-1",uniq.c_str());
					send_chat_to_object(msg, channel, source_id);
				}
				return true;
			}
			else if(cmd == "preloadanim")
			{
				//logically, this is no worse than lltriggersoundlimited used on you, 
				//and its enherently restricted to owner due to ownersay
				//therefore, I don't think a permission is necessitated
				LLUUID anim = LLUUID(args[1].asString());
				gAssetStorage->getAssetData(anim,
						LLAssetType::AT_ANIMATION,
						(LLGetAssetCallback)NULL,
						NULL,
						TRUE);
				//maybe add completion callback later?
				return true;
			}
			else if(cmd == "getwindowsize")
			{
				std::string uniq = args[1].asString();
				S32	channel = atoi(args[2].asString().c_str());
				const U32 height = gViewerWindow->getWindowDisplayHeight();
				const U32 width = gViewerWindow->getWindowDisplayWidth();
				std::string msg = llformat("getwindowsizereply|%s|%d|%d",uniq.c_str(),height,width);
				send_chat_to_object(msg, channel, source_id);
				return true;
			}
			else if(cmd == "key2name")
			{
				//same rational as preload impactwise
				std::string uniq = args[1].asString();
				n2kdat* data = new n2kdat;
				data->channel = atoi(args[3].asString().c_str());
				bool group = (bool)atoi(args[4].asString().c_str());
				data->reply = llformat("key2namereply|%s|",uniq.c_str());
				data->source = source_id;
				gCacheName->get(LLUUID(args[2].asString()), group, callbackname2key, data);
				return true;
			}
			else if(cmd == "emao")
			{
				std::istringstream i(args[1].asString());
				i >> cmd;
				if (args[1].asString() == "on")
				{
					gSavedPerAccountSettings.setBOOL("PhoenixAOEnabled",TRUE);
					LLFloaterAO::run();
				}
				else if (args[1].asString() == "off")
				{
					gSavedPerAccountSettings.setBOOL("PhoenixAOEnabled",FALSE);
					LLFloaterAO::run();
				}
				else if (cmd == "state")
				{
					S32 chan = atoi(args[2].asString().c_str());
					std::string tmp="off";
					if(gSavedPerAccountSettings.getBOOL("PhoenixAOEnabled"))tmp="on";
					send_chat_to_object(tmp,chan,gAgent.getID());
				}
				return true;
			}
			else if(cmd == "emdd")
			{
				S32 chan = atoi(args[2].asString().c_str());
				std::string tmp = llformat("%f",gSavedSettings.getF32("RenderFarClip"));
				send_chat_to_object(tmp,chan,gAgent.getID());
				return true;
			}
			/*else if(cmd == "gettext")
			{
				S32	channel = atoi(args[2].asString().c_str());
				LLUUID target = LLUUID(args[1].asString());
				std::string floating_text;
				LLViewerObject *obj = gObjectList.findObject(target);
				if(obj)
				{
					llinfos << "gettext got obj" << llendl;
					LLHUDText *hud_text = (LLHUDText *)obj->mText.get();
					if(hud_text)
					{
						llinfos << "gettext got hud_text" << llendl;
						floating_text = hud_text->getString();
					}
				}
				llinfos << "gettext returning: " << floating_text << llendl;
				send_chat_to_object(floating_text,channel,source_id);
				return true;
			}*/ //rendered obsolete by llGetLinkPrimitiveParams and/or PRIM_TEXT in 1.38+
		}else if(message.substr(0,3) == "l2c")
		{
			std::string lolnum = message.substr(3);
			//cmdline_printchat("--num="+lolnum);
			l2c = atoi(lolnum.c_str());
			//cmdline_printchat("--rnum="+llformat("%d",l2c));
			l2c_inuse = true;
			return true;
		}
	}
	return false;
}
//void send_chat_from_viewer(const std::string& utf8_out_text, EChatType type, S32 channel);
// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d) | Modified: RLVa-0.2.2a
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);
// [/RLVa:KB]
void JCLSLBridge::bridgetolsl(std::string cmd, JCBridgeCallback* cb)
{
	if(sBridgeStatus == ACTIVE)
	{
		std::string chat = llformat("%d",registerCB(cb)) + "|"+cmd;
		send_chat_from_viewer(chat, CHAT_TYPE_WHISPER, l2c_inuse ? l2c : JCLSLBridge::bridge_channel(gAgent.getID()));
	}else
	{
		//cmdline_printchat("--bridge not RECHANNEL");
		delete cb;
	}
}

std::string md5hash(const std::string &text, U32 thing)
{
	char temp[MD5HEX_STR_SIZE];
	LLMD5 toast((const U8*)text.c_str(), thing);
	toast.hex_digest(temp);
	return std::string(temp);
}

S32 JCLSLBridge::bridge_channel(LLUUID user)
{
	std::string tmps = md5hash(user.asString(),1);
	int i = (int)strtol((tmps.substr(0, 7) + "\n").c_str(),(char **)NULL,16);
	return (S32)i;
}

class ObjectBNameMatches : public LLInventoryCollectFunctor
{
public:
	ObjectBNameMatches(std::string name)
	{
		sName = name;
	}
	virtual ~ObjectBNameMatches() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			//LLViewerInventoryCategory* folderp = gInventory.getCategory((item->getParentUUID());
			return (item->getName() == sName);// && cat->getName() == "#v");
		}
		return false;
	}
private:
	std::string sName;
};


LLUUID JCLSLBridge::findCategoryByNameOrCreate(std::string name)
{
	LLUUID phoenix_category;
	phoenix_category = gInventory.findCategoryByName(phoenix_category_name);
	if(phoenix_category.isNull())
	{
		phoenix_category = gInventory.createNewCategory(gAgent.getInventoryRootID(), LLAssetType::AT_NONE, phoenix_category_name);
	}
	return phoenix_category;
}


const LLUUID& JCLSLBridge::findInventoryByName(const std::string& object_name, std::string catname)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	ObjectBNameMatches objectnamematches(object_name);
	LLUUID category;
	if(catname.length() > 0)
	{
		category = findCategoryByNameOrCreate(catname);
	}else
	{
		category = gAgent.getInventoryRootID();
	}

	gInventory.collectDescendentsIf(category,cats,items,FALSE,objectnamematches);

	for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
	{
		const LLViewerInventoryItem* itemp = items.get(idxItem);
		if (!itemp->getIsLinkType())
			return itemp->getUUID();
	}
	return LLUUID::null;
}

bool isworn(LLUUID item)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar && avatar->isWearingAttachment(item) )
	{
		return true;
	}
	return false;
}

bool JCLSLBridge::bridgeworn()
{
	return (mBridge) && isworn(mBridge->getUUID());
}

LLViewerInventoryItem* JCLSLBridge::findbridge()
{
	//cmdline_printchat("--looking for bridge");
	LLUUID item_id = findInventoryByName(phoenix_bridge_name);
	if (item_id.notNull())
	{
		//cmdline_printchat("--bridge found");
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		return item;
	}
	return NULL;
}

//KC: A valid bridge will pass AnyBridgePattern but not OldBridgePattern (maybe check if version is newer than ours?)
//     and the bridge object is in the #Phoenix folder
bool JCLSLBridge::ValidateBridge(LLViewerInventoryItem* item)
{
	//cmdline_printchat("--validating bridge");
	if (item && !IsAnOldBridge(item))
	{
		//cmdline_printchat("---have item");
		LLUUID phoenix_category = findCategoryByNameOrCreate(phoenix_category_name);
		if (gInventory.isObjectDescendentOf(item->getUUID(), phoenix_category))
		{
			//cmdline_printchat("---bridge validated");
			return true;
		}
	}
	//cmdline_printchat("--bridge not valid");
	return FALSE;
}

void JCLSLBridge::attachbridge(LLViewerInventoryItem* item)
{
	//cmdline_printchat("--bridge is ready to attach");
	if (!isworn(item->getUUID()))
	{
		l2c = 0;
		l2c_inuse = false;
		//cmdline_printchat("--attaching");
		// Queue bridge for attachment
		llinfos << "attaching bridge : " << item->getUUID() << llendl;
		LLAttachmentsMgr::instance().addAttachment(item->getUUID(), PH_BRIDGE_POINT, FALSE, TRUE);
	}
	sBridgeStatus = RECHANNEL;
}

//KC: Called by LLVOAvatar::detachObject
void JCLSLBridge::CheckForBridgeDetach(const LLUUID& item_id)
{
	// Check for disconnect. --Techwolf Lupindo 2011-5-1
	if (!gAgent.getRegion())
	{
		return;
	}
	
	if (sBuildBridge && item_id.notNull() && mBridge) // do sanity checks first....
	{
		if (item_id == mBridge->getUUID())
		{
			//cmdline_printchat("--bridge="+bridge.asString()+" || "+(!isworn(bridge) ? "1" : "0"));
			//cmdline_printchat("--reattaching");
			sBridgeStatus = UNINITIALIZED;
			mPeriod = 5.f;
			mEventTimer.start();
			llinfos << "bridge was removed, setting for reattach " << llendl;
		}
	}
}

// This function depends on not being passed a null item. Since it's always
//  called after making that check, that's not an issue. -- TS
bool JCLSLBridge::IsAnOldBridge(LLViewerInventoryItem* item)
{
        // Is this even a bridge? If not, don't bother checking.
        if (!IsABridge(item))
        {
                return false;
        }

        // Extract the version number from the name. Assumes that the name
        //  ends with the version number, in the form <major>.<minor>.
        std::string name = item->getName();
        std::string version_digits ("0123456789.");
        size_t version_pos = name.find_last_not_of(version_digits);
        if (version_pos >= name.length())
        {
                llinfos <<
                        "No version number found at end of bridge name string."
                        << llendl;
                return false;
        }
        std::string version = name.substr(version_pos+1);
        
        // Get major and minor version numbers.
        size_t dot_pos = version.find('.');
        if (dot_pos == std::string::npos)
        {
                llinfos << "No dot found in bridge version number." << llendl;
                return false;
        }
        int major = atoi(version.substr(0,dot_pos).c_str());
        int minor = atoi(version.substr(dot_pos+1,std::string::npos).c_str());

        // Compare the major and minor versions.
	return ((major < PHOENIX_BRIDGE_MAJOR_VERSION) ||
	        ((major == PHOENIX_BRIDGE_MAJOR_VERSION) &&
	        (minor < PHOENIX_BRIDGE_MINOR_VERSION)));
}

bool JCLSLBridge::IsABridge(LLViewerInventoryItem* item)
{
	return (item) && boost::regex_match(item->getName(), AnyBridgePattern);
}

void JCLSLBridge::ChangeBridge(LLViewerInventoryItem* item)
{
	//cmdline_printchat("--ChangeBridge");
	mBridge = item;
	LLVOAvatar* pAvatar = gAgent.getAvatarObject();
	if (pAvatar)
	{
		LLViewerJointAttachment* attachment = get_if_there(pAvatar->mAttachmentPoints, (S32)PH_BRIDGE_POINT, (LLViewerJointAttachment*)NULL);
		if (attachment)
		{
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				 attachment_iter != attachment->mAttachedObjects.end();
				 ++attachment_iter)
			{
				LLViewerObject *attached_object = (*attachment_iter);
				if (attached_object)
				{
					LLUUID item_id = attached_object->getAttachmentItemID();
					if (item_id.notNull() && (item_id != item->getUUID()))
					{
						//cmdline_printchat("---removing non active bridge");
						LLVOAvatar::detachAttachmentIntoInventory(item_id);
					}
				}
			}
		}
	}
	Reset();
}

//KC: Called by LLAttachmentsMgr::addAttachment
void JCLSLBridge::Reset()
{
	llinfos << "resetting bridge for recheck" << llendl;
	//cmdline_printchat("--resetting bridge");
	sBridgeStatus = UNINITIALIZED;
	mPeriod = 5.f;
	mEventTimer.start();
	
}

// bool IsCurrentBridge(const LLViewerObject* pObj)
// {
	// return (pObj == mBridge)
// }


static const std::string bridgeprefix = std::string("#LSL<->Client Bridge v");
static const U32 bridgeprefix_length = U32(bridgeprefix.length());

void callbackBridgeCleanup(const LLSD &notification, const LLSD &response, LLViewerInventoryItem::item_array_t items)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	
	if ( option == 0 )
	{
		if(items.count())
		{
			//cmdline_printchat("--Moving out-of-date bridge objects to your trash folder.");
			//delete
			LLUUID trash_cat = gInventory.findCategoryUUIDForType(LLAssetType::AT_TRASH);
			for(LLDynamicArray<LLPointer<LLViewerInventoryItem> >::iterator itr = items.begin(); itr != items.end(); ++itr)
			{
				LLViewerInventoryItem* item = *itr;
				if(item)
				{
					move_inventory_item(gAgent.getID(),gAgent.getSessionID(),item->getUUID(),trash_cat,item->getName(), NULL);
					//cmdline_printchat("--Moved item "+item->getName());
				}
			}
			//cmdline_printchat("--Items moved.");
		}
	}
	else
	{
		gSavedSettings.setBOOL("PhoenixOldBridgeCleanup", FALSE);
	}
}
class BridgeCleanupMatches : public LLInventoryCollectFunctor
{
public:
	BridgeCleanupMatches()
	{
		trash_cat = gInventory.findCategoryUUIDForType(LLAssetType::AT_TRASH);
	}
	virtual ~BridgeCleanupMatches() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			if(item->getName().substr(0,bridgeprefix_length) == bridgeprefix)
			{
				LLUUID parent_id = item->getParentUUID();
				BOOL in_trash = (parent_id == trash_cat);
				if(in_trash == FALSE)
				{
					LLViewerInventoryCategory* parent = gInventory.getCategory(parent_id);
					while(parent)
					{
						parent_id = parent->getParentUUID();
						if(parent_id == trash_cat)
						{
							in_trash = TRUE;
							break;
						}else if(parent_id.isNull())break;

						parent = gInventory.getCategory(parent_id);
					}
					if(in_trash == FALSE)
					{
						return true;
					}
				}
			}
		}
		return false;
	}
private:
	LLUUID trash_cat;
};

void bridge_trash_check()
{
	//cmdline_printchat("--bridge_trash_check");
	if (gSavedSettings.getBOOL("PhoenixOldBridgeCleanup"))
	{
		//cmdline_printchat("--doing cleaner scan");
		BridgeCleanupMatches prefixmatcher;
		LLViewerInventoryCategory::cat_array_t cats;
		LLViewerInventoryItem::item_array_t items;

		gInventory.collectDescendentsIf(gAgent.getInventoryRootID(),cats,items,FALSE,prefixmatcher);

		LLViewerInventoryItem::item_array_t delete_queue;

		if(items.count())
		{
			//cmdline_printchat("--items.count()");
			for(LLDynamicArray<LLPointer<LLViewerInventoryItem> >::iterator itr = items.begin(); itr != items.end(); ++itr)
			{
				LLViewerInventoryItem* item = *itr;
				if ((item) && JCLSLBridge::IsAnOldBridge(item))
				{
					//cmdline_printchat("--found old bridge");
					llinfos << "found old bridge: " << item->getName() << llendl;
					delete_queue.push_back(item);
				}
			}
			int dqlen = delete_queue.count();
			if(dqlen > 0)
			{
				//cmdline_printchat("--dqlen > 0");
				std::string bridges = llformat("%d",delete_queue.count())+" older Phoenix LSL Bridge object";
				if(dqlen > 1)bridges += "s";
				LLSD args;
				args["BRIDGES"] = bridges;
				LLNotifications::instance().add("QueryBridgeCleanup", args,LLSD(), boost::bind(callbackBridgeCleanup, _1, _2, delete_queue));
			}
		}
	}
}


BOOL JCLSLBridge::tick()
{
	if (LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		static BOOL firstsim = TRUE;
		if (firstsim && gInventory.isInventoryUsable())
		{
			//cmdline_printchat("--firstsim fetching #Phoenix");
			firstsim = FALSE;
			LLUUID phoenix_category = findCategoryByNameOrCreate(phoenix_category_name);
			gInventory.fetchDescendentsOf(phoenix_category);
		}
		
		static BOOL first_full_load = TRUE;
		if (first_full_load && gInventory.isEverythingFetched()) // when the inv is done fetching, check for old bridges
		{
			//cmdline_printchat("--first full inv load");
			first_full_load = FALSE;
			bridge_trash_check();
		}
		
		switch(sBridgeStatus)
		{
		case UNINITIALIZED:
			{
				if (!sBuildBridge)
				{
					//cmdline_printchat("--PhoenixBuildBridge is false");
					sBridgeStatus = FAILED;
					break;
				}
				
				//cmdline_printchat("--initializing");
				LLUUID phoenix_category = findCategoryByNameOrCreate(phoenix_category_name);
				if (gInventory.isCategoryComplete(phoenix_category))
				{
					mPeriod = 1.f;
					//cmdline_printchat("--#Phoenix is fetched");
					if (bridgeworn())
					{
						//cmdline_printchat("---bridge attached");
						if (ValidateBridge(mBridge))
						{
							//cmdline_printchat("---bridge is good, moving on");
							sBridgeStatus = RECHANNEL;
						}
						else //KC: Something is amiss... remove this thing and look 
						{
							//cmdline_printchat("---attached bridge is bad, gonna relook");
							LLVOAvatar::detachAttachmentIntoInventory(mBridge->getLinkedUUID());
						}
					}
					else
					{
						LLViewerInventoryItem* item = findbridge();
						if (ValidateBridge(item)) // bridge found
						{
							attachbridge(item);
						}
						else // bridge not found, make a new one
						{
							//sBridgeStatus = BUILDING;
							std::string directory = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,"bridge.xml");
							if (!LLFile::isfile(directory.c_str()))
							{
								//cmdline_printchat("--bridge failed: file not there o.o");
								sBridgeStatus = FAILED;
							}
							else
							{
								if (LLStartUp::getStartupState() >= STATE_STARTED)
								{
									//cmdline_printchat("--building bridge: bridge.xml located. importing..");
									gImportTracker.importer(directory,&setBridgeObject);
									sBridgeStatus = BUILDING;
								}
								else
								{
									//cmdline_printchat("--state is not quite ready");
								}
							}
						}
					}
				}
				else
				{
					//cmdline_printchat("--#Phoenix is not fetched");
				}
			}
			break;
		case RENAMING:
			{
				//cmdline_printchat("--renaming");
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_ObjectAttach);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->addU8Fast(_PREHASH_AttachmentPoint, PH_BRIDGE_POINT);
				
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, sBridgeObject->getLocalID());
				msg->addQuatFast(_PREHASH_Rotation, LLQuaternion(0.0f, 0.0f, 0.0f, 1.0f));
				
				msg->sendReliable(gAgent.getRegion()->getHost());
				sBridgeStatus = FOLDERING;
			}
			break;
		case FOLDERING:
			{
				//cmdline_printchat("--foldering");
				LLUUID phoenix_category = findCategoryByNameOrCreate(phoenix_category_name);
				LLUUID bridge_id = findInventoryByName(phoenix_bridge_name);
				//cmdline_printchat("--bridge_id="+bridge_id.asString());
				//cmdline_printchat("--id="+bridge_id.asString());
				LLViewerInventoryItem* bridge = gInventory.getItem(bridge_id);
				if(bridge)
				{
					//cmdline_printchat("--bridge exists, moving to #Phoenix.");
					move_inventory_item(gAgent.getID(),gAgent.getSessionID(),bridge->getUUID(),phoenix_category,phoenix_bridge_name, NULL);
					sBridgeStatus = RECHANNEL;
				}
			}
			break;
		case RECHANNEL:
			{
				{
					//cmdline_printchat("--sending rechannel cmd");
					send_chat_from_viewer(std::string("0|l2c"), CHAT_TYPE_WHISPER, JCLSLBridge::bridge_channel(gAgent.getID()));
					mPeriod = 160.f; // set high cause it takes a really long time for the inv to fully load :/
					sBridgeStatus = ACTIVE;
				}
			}
			break;
		case ACTIVE:
		case FAILED: // running, broken, or disabled, in any case, stop the timer if were done
			{
				//cmdline_printchat("--Active state tick");
				//llinfos << "bridge ACTIVE state tick" << llendl;
				// give old bridge cleanup a chance to finish if it hasnt
				if (!first_full_load)
				{
					//cmdline_printchat("--stopping timer");
					mEventTimer.stop();
				}
				break;
			}
		}
	}
	return FALSE;
}
void JCLSLBridge::setBridgeObject(LLViewerObject* obj)
{
	if(obj)
	{
		sBridgeObject = obj;
		//cmdline_printchat("--callback reached");
		sBridgeStatus = RENAMING;
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ObjectName);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID,gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID,obj->getLocalID());
		msg->addStringFast(_PREHASH_Name,phoenix_bridge_name);
		gAgent.sendReliableMessage();
	}
}

void JCLSLBridge::callback_fire(U32 callback_id, LLSD data)
{
	if (!callback_id)
		return;

	std::map<U32, JCBridgeCallback*>::iterator i;

	i = callback_map.find(callback_id);
	if (i != callback_map.end())
	{
		(*i).second->fire(data);
		callback_map.erase(i);
	}
}

U32 JCLSLBridge::registerCB(JCBridgeCallback* cb)
{
	if (!cb)
		return 0;

	lastcall++;
	if (!lastcall)
		lastcall++;

	callback_map[lastcall] = cb;
	//no while(callback_map.size() > 100)callback_map.erase(0);
	return lastcall;
}

LLSD JCLSLBridge::parse_string_to_list(std::string list, char sep)
{
	LLSD newlist;
	U32 count = 0;
	std::string token;
	std::istringstream iss(list);
	while ( getline(iss, token, sep) )
	{
		newlist[count] = token;
		count += 1;
	}
	return newlist;
}


void JCLSLBridge::processSoundTrigger(LLMessageSystem* msg,void**)
{
	LLUUID	sound_id,owner_id,object_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ObjectID, object_id);
	if(owner_id == gAgent.getID())
	{
		
		if(sound_id == LLUUID("420d76ad-c82b-349d-7b81-f00d0ca0f38f"))
		{
			if(sBridgeStatus == ACTIVE)
			{
                std::string mess = "phoenix_bridge_rdy";
				send_chat_to_object(mess, JCLSLBridge::bridge_channel(gAgent.getID()), object_id);
			}else if(sBridgeStatus == FAILED)
			{
                std::string mess = "phoenix_bridge_failed";
				send_chat_to_object(mess, JCLSLBridge::bridge_channel(gAgent.getID()), object_id);
			}else
			{
                std::string mess = "phoenix_bridge_working";
				send_chat_to_object(mess, JCLSLBridge::bridge_channel(gAgent.getID()), object_id);
			}
		}
		
	}
}
/*
LLUUID BridgePermissions_Phoenix("c78f9f3f-56ac-4442-a0b9-8b41dad455ae");

void JCLSLBridge::loadPermissions()
{
	std::vector<std::string> strings;
	strings.push_back( BridgePermissions_Phoenix.asString() );//BridgePermissions Phoenix
	send_generic_message("avatarnotesrequest", strings);
}

void JCLSLBridge::storePermissions()
{
	LLMessageSystem *msg = gMessageSystem;
	msg->newMessage("AvatarNotesUpdate");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlock("Data");
	msg->addUUID("TargetID", BridgePermissions_Phoenix);
	msg->addString("Notes", "");//RECHANNEL_permissions);
	gAgent.sendReliableMessage();
}
*/
void JCLSLBridge::processAvatarNotesReply(LLMessageSystem *msg, void**)
{
	// extract the agent id
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	LLUUID target_id;
	msg->getUUID("Data", "TargetID", target_id);

	/*if(target_id == BridgePermissions_Phoenix)
	{
		std::string text;
		msg->getString("Data", "Notes", text);
		//LLSD arguments = JCLSLBridge::parse_string_to_list(text, '|');
		//RECHANNEL_permissions = text;//arguments;
	}*/
}
