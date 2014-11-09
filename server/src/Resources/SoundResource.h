
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
		throw Poco::FileNotFoundException("Sound resource not found [" + Name + "]");
	}
	
public:
	CSoundResource(Poco::Path FSPath, std::string ClientPath, std::string Language, std::string Name)
		:	m_FSPath(FSPath)
		,	m_ClientPath(ClientPath)
		,	m_Language(Language)
		,	m_Name(Name)
	{
	}

	virtual std::string GetNumber(int Number, bool IntonationDown) const override
	{	//	Try to find intonation down resource. If not found try intonation up
		try
		{
			return GetSoundFile(Poco::format(IntonationDown ? "%i@" : "%i", Number));
		}
		catch (Poco::FileNotFoundException&)
		{
			if (IntonationDown)
			{
				return GetSoundFile(Poco::format("%i", Number));
			}
			throw;
		}		
	}

	virtual std::string GetUnique() const override
	{
		return m_ClientPath;
	}

	virtual std::string GetLanguage() const override
	{
		return m_Language;
	}

	virtual std::string GetName() const override
	{
		return m_Name;
	}
};

