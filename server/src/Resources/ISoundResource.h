
#pragma once

#include <string>

class ISoundResource
{
public:
   ~ISoundResource() 
    {
    }
	virtual std::string GetNumber(int Number) const abstract;
};