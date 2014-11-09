
#pragma once

#include <string>

class ISoundResource
{
public:
   ~ISoundResource() 
    {
    }
	virtual std::string GetNumber(int Number, bool IntonationDown) const abstract;
	virtual std::string GetUnique() const abstract;
	virtual std::string GetLanguage() const abstract;
	virtual std::string GetName() const abstract;
};