/*
	Copyright Chris Rehor (Liny Odell) 2010.
	Licensed under the artistic license version 2.
	http://www.perlfoundation.org/artistic_license_2_0
*/


#ifndef DIAMONDAOINT_H
#define DIAMONDAOINT_H

#include "llviewerprecompiledheaders.h"

void send_chat_to_object(std::string chat, S32 channel, LLUUID target);

class DiamondAoInt
{
public:
	DiamondAoInt();
	~DiamondAoInt();
	static bool AOCommand(std::string message);
	static void AOStatusUpdate(bool status);
private:
	static S32 regchan;
};

#endif
