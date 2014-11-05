
#pragma once

#include <string>
#include <Poco/Path.h>
#include "Resources/ISoundResource.h"

enum SideIndex
{	//	Do not change. Players are used as indices
	kSideOne	= 0,
	kSideTwo	= 1,
};

class IGameController
{
public:
	virtual ~IGameController()
	{}
	//	Control inputs
	virtual void OnInputId(SideIndex button, const std::string& Id) abstract;
	virtual void OnInputPress(SideIndex button, int miliseconds) abstract;

	//	This function determines what is displayed on frontend.
	//	Frontend may only send it's timestamps
	virtual std::string GetInterfaceState(const std::string& message) abstract;

	//	This function handles remote procedure calls
	virtual std::string RpcCall(const std::string& command) abstract;
};