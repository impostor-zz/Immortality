#include "llviewerprecompiledheaders.h"

#include "floatermedialists.h"
#include "llviewerparcelmedia.h"
#include "lluictrlfactory.h" 
#include "llscrolllistctrl.h"
#include "lllineeditor.h"

FloaterMediaLists* FloaterMediaLists::sInstance = NULL;
bool FloaterMediaLists::sIsWhitelist;

FloaterMediaLists::FloaterMediaLists() :  LLFloater(std::string("floater_media_lists"))
{
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_media_lists.xml");
}

void FloaterMediaLists::show(void*)
{
    if (!sInstance)
		sInstance = new FloaterMediaLists();

    sInstance->open();
}

FloaterMediaLists::~FloaterMediaLists()
{
    sInstance=NULL;
}

BOOL FloaterMediaLists::postBuild()
{
	mWhitelistSLC = getChild<LLScrollListCtrl>("whitelist_list");
	mBlacklistSLC = getChild<LLScrollListCtrl>("blacklist_list");

	childSetAction("add_whitelist", onWhitelistAdd,this);
	childSetAction("remove_whitelist", onWhitelistRemove,this);
	childSetAction("add_blacklist", onBlacklistAdd,this);
	childSetAction("remove_blacklist", onBlacklistRemove,this);
	childSetAction("commit_domain", onCommitDomain,this);
	childSetUserData("whitelist_list", this);
	childSetUserData("blacklist_list", this);


	if (!mWhitelistSLC || !mBlacklistSLC)
	{
		return true;
	}
	
	for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
	{
		if (LLViewerParcelMedia::sMediaFilterList[i]["action"].asString() == "allow")
		{	
			LLSD element;
			element["columns"][0]["column"] = "whitelist_col";
			element["columns"][0]["value"] = LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString();
			element["columns"][0]["font"] = "SANSSERIF";
			mWhitelistSLC->addElement(element, ADD_SORTED);
		}
		else if (LLViewerParcelMedia::sMediaFilterList[i]["action"].asString() == "deny")
		{
			LLSD element;
			element["columns"][0]["column"] = "blacklist_col";
			element["columns"][0]["value"] = LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString();
			element["columns"][0]["font"] = "SANSSERIF";
			mBlacklistSLC->addElement(element, ADD_SORTED);
		}
	}

	return TRUE;
}

void FloaterMediaLists::draw()
{
	refresh();

	LLFloater::draw();
}

void FloaterMediaLists::refresh()
{
	
}

void FloaterMediaLists::onWhitelistAdd( void* data )
{
	sInstance->childDisable("blacklist_list");
	sInstance->childDisable("whitelist_list");
	sInstance->childDisable("remove_whitelist");
	sInstance->childDisable("add_whitelist");
	sInstance->childDisable("remove_blacklist");
	sInstance->childDisable("add_blacklist");
	sInstance->childSetVisible("input_domain",true);
	sInstance->childSetVisible("commit_domain",true);
	sInstance->childSetText("add_text",std::string("Enter domain/url to add to domain whitelist:"));
	sInstance->childSetVisible("add_text",true);
	sIsWhitelist = true;
}

void FloaterMediaLists::onWhitelistRemove( void* data )
{
	LLScrollListItem* selected = sInstance->mWhitelistSLC->getFirstSelected();

	if (selected)
	{
		std::string domain = sInstance->mWhitelistSLC->getSelectedItemLabel();

		for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
		{
			if (LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerParcelMedia::sMediaFilterList.erase(i);
				LLViewerParcelMedia::saveDomainFilterList();
				break;
			}
		}

		sInstance->mWhitelistSLC->deleteSelectedItems();
	}
}

void FloaterMediaLists::onBlacklistAdd( void* data )
{
	sInstance->childDisable("blacklist_list");
	sInstance->childDisable("whitelist_list");
	sInstance->childDisable("remove_whitelist");
	sInstance->childDisable("add_whitelist");
	sInstance->childDisable("remove_blacklist");
	sInstance->childDisable("add_blacklist");
	sInstance->childSetVisible("input_domain",true);
	sInstance->childSetVisible("commit_domain",true);
	sInstance->childSetText("add_text",std::string("Enter domain/url to add to domain blacklist:"));
	sInstance->childSetVisible("add_text",true);
	sIsWhitelist = false;
}

void FloaterMediaLists::onBlacklistRemove( void* data )
{	
	LLScrollListItem* selected = sInstance->mBlacklistSLC->getFirstSelected();

	if (selected)
	{
		std::string domain = sInstance->mBlacklistSLC->getSelectedItemLabel();

		for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
		{
			if (LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerParcelMedia::sMediaFilterList.erase(i);
				LLViewerParcelMedia::saveDomainFilterList();
				break;
			}
		}

		sInstance->mBlacklistSLC->deleteSelectedItems();
	}
}	

void FloaterMediaLists::onCommitDomain( void* data )
{
	std::string domain = sInstance->childGetText("input_domain");
	domain = LLViewerParcelMedia::extractDomain(domain);

	if (sIsWhitelist)
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		LLSD element;
		element["columns"][0]["column"] = "whitelist_col";
		element["columns"][0]["value"] = domain;
		element["columns"][0]["font"] = "SANSSERIF";
		sInstance->mWhitelistSLC->addElement(element, ADD_SORTED);
	}
	else
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		LLSD element;
		element["columns"][0]["column"] = "blacklist_col";
		element["columns"][0]["value"] = domain;
		element["columns"][0]["font"] = "SANSSERIF";
		sInstance->mBlacklistSLC->addElement(element, ADD_SORTED);
	}

	sInstance->childEnable("blacklist_list");
	sInstance->childEnable("whitelist_list");
	sInstance->childEnable("remove_whitelist");
	sInstance->childEnable("add_whitelist");
	sInstance->childEnable("remove_blacklist");
	sInstance->childEnable("add_blacklist");
	sInstance->childSetVisible("input_domain",false);
	sInstance->childSetVisible("commit_domain",false);
	sInstance->childSetVisible("add_text",false);

}