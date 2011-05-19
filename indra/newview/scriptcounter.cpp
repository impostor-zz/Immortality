/* Copyright (c) 2009
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
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS "AS IS"
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

#include "scriptcounter.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "lltransfersourceasset.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerobject.h"
#include "jc_lslviewerbridge.h"
#include <sstream>

ScriptCounter* ScriptCounter::sInstance;
U32 ScriptCounter::invqueries;
U32 ScriptCounter::status;
U32 ScriptCounter::scriptcount;
U32 ScriptCounter::toCount;
U32 ScriptCounter::scriptMemory;
LLUUID ScriptCounter::reqObjectID;
LLDynamicArray<LLUUID> ScriptCounter::delUUIDS;
bool ScriptCounter::doDelete;
std::set<std::string> ScriptCounter::objIDS;
int ScriptCounter::objectCount;
LLViewerObject* ScriptCounter::foo;
void cmdline_printchat(std::string chat);
std::stringstream ScriptCounter::sstr;
int ScriptCounter::countingDone;

ScriptCounter::ScriptCounter()
{
	llassert_always(sInstance == NULL);
	sInstance = this;

}

ScriptCounter::~ScriptCounter()
{
	sInstance = NULL;
}
void ScriptCounter::init()
{
	if(!sInstance)
		sInstance = new ScriptCounter();
	status = IDLE;
}

LLVOAvatar* find_avatar_from_object( LLViewerObject* object );

LLVOAvatar* find_avatar_from_object( const LLUUID& object_id );
void ScriptCounter::checkCount()
{
  toCount--;
  if(!toCount)
  {
    gMessageSystem->mPacketRing.setOutBandwidth(0.0);
    gMessageSystem->mPacketRing.setUseOutThrottle(FALSE);
    std::string resultStr;
    resultStr="Scripts Counted: ";
    sstr.str("");
    sstr << scriptcount;
    resultStr+=sstr.str();
    sstr.str("");
    sstr << scriptMemory;
    resultStr+=" [";
    resultStr+=sstr.str();
    resultStr+="K]";
    cmdline_printchat(resultStr);
    init();
  }
}

void ScriptCounter::showResult()
{
	sstr << scriptcount;
	cmdline_printchat(sstr.str());
	init();
}

void ScriptCounter::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID,object_id );
	std::string name;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	if(reqObjectID.notNull())
	if(object_id == reqObjectID)
	{
		sstr << "Deleted scripts from object "+name+": ";
		reqObjectID.setNull();
		if(countingDone)
			showResult();
	}
}

void ScriptCounter::processObjectProperties(LLMessageSystem* msg, void** user_data)
{
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID,object_id );
	std::string name;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	if(reqObjectID.notNull())
	if(object_id == reqObjectID)
	{
		sstr << "Deleted scripts from object "+name+": ";
		reqObjectID.setNull();
		if(countingDone)
			showResult();
	}
}
class JCCountCallback : public JCBridgeCallback
{
public:
	JCCountCallback(LLUUID target)
	{
		targetID=target;
	}

	void fire(LLSD data)
	{
	    if(ScriptCounter::toCount)
	    {
	      count=0;
	      memory=0;
		count=atoi(data[0].asString().c_str());
		memory=atoi(data[1].asString().c_str());
		ScriptCounter::scriptcount+=count;
		ScriptCounter::scriptMemory+=memory;
		ScriptCounter::checkCount();
	    }
	}

private:
	std::stringstream sstr;
	U32 count;
	U32 memory;
	std::string temp;
	LLUUID targetID;
};

void ScriptCounter::radarScriptCount(LLUUID target)
{
    scriptcount=0;
    scriptMemory=0;
    toCount=1;
    JCLSLBridge::instance().bridgetolsl("script_count|"+target.asString(), new JCCountCallback(target));
}
void ScriptCounter::serializeSelection(bool delScript)
{
	LLDynamicArray<LLViewerObject*> catfayse;
	foo=LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
	sstr.str("");
	invqueries=0;
	doDelete=false;
	scriptcount=0;
	scriptMemory=0;
	toCount=0;
	objIDS.clear();
	delUUIDS.clear();
	objectCount=0;
	countingDone=false;
	reqObjectID.setNull();
	if(foo)
	{
		if(foo->isAvatar())
		{
			toCount=1;
			LLVOAvatar* av=find_avatar_from_object(foo);
			LLUUID avID=av->getID();
			if(av)
			{
			    toCount=1;
			    JCLSLBridge::instance().bridgetolsl("script_count|"+avID.asString(), new JCCountCallback(avID));
			}
			return;
		}
		else
		{
			for (LLObjectSelection::valid_root_iterator iter = LLSelectMgr::getInstance()->getSelection()->valid_root_begin();
					 iter != LLSelectMgr::getInstance()->getSelection()->valid_root_end(); iter++)
			{
				LLSelectNode* selectNode = *iter;
				LLViewerObject* object = selectNode->getObject();
				if(object)
				{
					catfayse.put(object);
					objectCount++;
					toCount++;
				}
			}
			doDelete=delScript;
		}
		F32 throttle = gSavedSettings.getF32("OutBandwidth");
		if((throttle == 0.f) || (throttle > 128000.f))
		{
			gMessageSystem->mPacketRing.setOutBandwidth(128000);
			gMessageSystem->mPacketRing.setUseOutThrottle(TRUE);
		}
		if(doDelete)
		  cmdline_printchat("Deleting scripts. Please wait.");
		if(objectCount == 1)
		{
			LLViewerObject *reqObject=((LLViewerObject*)foo->getRoot());
			if(!doDelete)
			{
			  LLUUID objID=((LLViewerObject*)foo)->getID();
			  toCount=1;
			  JCLSLBridge::instance().bridgetolsl("script_count|"+objID.asString(), new JCCountCallback(objID));
			  return;
			}
			else
			{
				reqObjectID=reqObject->getID();
				LLMessageSystem* msg = gMessageSystem;
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags, 0 );
				msg->addUUIDFast(_PREHASH_ObjectID, reqObjectID);
				gAgent.sendReliableMessage();
			}
		}
		serialize(catfayse);
	}
}

void ScriptCounter::serialize(LLDynamicArray<LLViewerObject*> objects)
{
	init();
	status = COUNTING;
	for(LLDynamicArray<LLViewerObject*>::iterator itr = objects.begin(); itr != objects.end(); ++itr)
	{
		LLViewerObject* object = *itr;
		if (object)
		  if(!doDelete)
		  {
		    LLUUID objID=object->getID(); 
		    JCLSLBridge::instance().bridgetolsl("script_count|"+objID.asString(), new JCCountCallback(objID));
		  }
		  else

			subserialize(object);
	}
	if(invqueries == 0)
		init();
}

void ScriptCounter::subserialize(LLViewerObject* linkset)
{
	LLViewerObject* object = linkset;
	LLDynamicArray<LLViewerObject*> count_objects;
	count_objects.put(object);
	LLViewerObject::child_list_t child_list = object->getChildren();
	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		LLViewerObject* child = *i;
		if(!child->isAvatar())
			count_objects.put(child);
	}
	S32 object_index = 0;
	while ((object_index < count_objects.count()))
	{
		object = count_objects.get(object_index++);
		LLUUID id = object->getID();
		objIDS.insert(id.asString());
		llinfos << "Counting scripts in prim " << object->getID().asString() << llendl;
		object->registerInventoryListener(sInstance,NULL);
		object->dirtyInventory();
		object->requestInventory();
		invqueries += 1;
	}
}

void ScriptCounter::completechk()
{
	if(invqueries == 0)
	{
		if(reqObjectID.isNull())
		{
			if(sstr.str() == "")
			{
				sstr << "Deleted scripts in " << objectCount << " objects: ";
				F32 throttle = gSavedSettings.getF32("OutBandwidth");
				if(throttle != 0.f)
				{
					gMessageSystem->mPacketRing.setOutBandwidth(throttle);
					gMessageSystem->mPacketRing.setUseOutThrottle(TRUE);
				}
				else
				{
					gMessageSystem->mPacketRing.setOutBandwidth(0.0);
					gMessageSystem->mPacketRing.setUseOutThrottle(FALSE);
				}
			}
			showResult();
		}
		else
			countingDone=true;
	}
}

void ScriptCounter::inventoryChanged(LLViewerObject* obj,
								 InventoryObjectList* inv,
								 S32 serial_num,
								 void* user_data)
{
	if(status == IDLE)
	{
		obj->removeInventoryListener(sInstance);
		return;
	}
	if(objIDS.find(obj->getID().asString()) != objIDS.end())
	{
		if(inv)
		{
			InventoryObjectList::const_iterator it = inv->begin();
			InventoryObjectList::const_iterator end = inv->end();
			for( ;	it != end;	++it)
			{
				LLInventoryObject* asset = (*it);
				if(asset)
				{
					if(asset->getType() == LLAssetType::AT_LSL_TEXT)
					{
						scriptcount+=1;
						delUUIDS.push_back(asset->getUUID());
					}
				}
			}
			while (delUUIDS.count() > 0)
			{
				const LLUUID toDelete=delUUIDS[0];
				delUUIDS.remove(0);
				if(toDelete.notNull())
					obj->removeInventory(toDelete);

			}
		}
		invqueries -= 1;
		objIDS.erase(obj->getID().asString());
		obj->removeInventoryListener(sInstance);
		completechk();
	}
}
