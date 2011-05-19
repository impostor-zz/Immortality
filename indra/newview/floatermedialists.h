
#ifndef LL_LLFLOATERMEDIALISTS_H
#define LL_LLFLOATERMEDIALISTS_H

#include "llfloater.h"


class LLScrollListCtrl;
class LLButton;

class FloaterMediaLists : public LLFloater
{
public:
    FloaterMediaLists();
	BOOL postBuild();
    virtual ~FloaterMediaLists();

    static void show(void*);
	virtual void draw();
	void refresh();

	static void onWhitelistAdd(void*);
	static void onWhitelistRemove(void*);
	static void onBlacklistAdd(void*);
	static void onBlacklistRemove(void*);
	static void onCommitDomain(void*);

private:
	static bool sIsWhitelist;
	LLScrollListCtrl*	mWhitelistSLC;
	LLScrollListCtrl*	mBlacklistSLC;

    static FloaterMediaLists* sInstance;

};
#endif