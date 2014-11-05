
#pragma once

#include "ISoundResource.h"
#include <string>
#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Format.h>

class CSoundResource : public ISoundResource
{
	Poco::Path		m_FSPath;
	std::string		m_ClientPath;
	std::string		m_Language;
	std::string		m_Name;

	std::string GetSoundFile(std::string Name) const
	{
		const char * SupportedExtensions[] = {".wav", ".mp3", NULL};

		const char ** Extension = SupportedExtensions;
		while (*Extension != NULL)
		{
			Poco::Path	Path(m_FSPath, Name + *Extension);
			if (Poco::File(Path).exists())
			{
				return m_ClientPath + Name + *Extension;
			}
			++Extension;
		}
		throw std::runtime_error("Sound resource not found [" + Name + "]");
	}
	
public:
	CSoundResource(Poco::Path FSPath, std::string ClientPath, std::string Language, std::string Name)
		:	m_FSPath(FSPath)
		,	m_ClientPath(ClientPath)
		,	m_Language(Language)
		,	m_Name(Name)
	{
	}

	virtual std::string GetNumber(int Number) const override
	{
		return GetSoundFile(Poco::format("%i", Number));
	}
};

