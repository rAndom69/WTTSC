
#pragma once

#include <Poco/Path.h>
#include "ResourceCollection.h"
#include "SoundResource.h"
#include <Poco/DirectoryIterator.h>

class CSoundResourceCollection : public BaseResourceCollection<CSoundResource>
{
	Poco::Path m_FSRootPath;

	std::string SoundDirectory()
	{
		return "sound";
	}

	void LoadSoundResources(std::string Language, Poco::Path Path)
	{
		for (Poco::DirectoryIterator it(Path), end; it != end; ++it)
		{
			std::string Name = it.name();
			std::string ClientPath = Language + "/" + Name + "/";
			Poco::Path	ResourcePath(m_FSRootPath, Poco::Path(ClientPath));
			m_Resources.emplace_back(new CSoundResource(ResourcePath, "sounds/" + ClientPath, Language, Name));
		}
	}

public:
	CSoundResourceCollection(Poco::Path FSPath)
		:	m_FSRootPath(FSPath.pushDirectory("sounds"))
	{
		Reload();
	}

	virtual void Reload() override
	{
		m_Resources.clear();
		for (Poco::DirectoryIterator it(m_FSRootPath), end; it != end; ++it)
		{
			if (it->isDirectory())
			{
				std::string Language = it.name();
				LoadSoundResources(Language, it.path());
			}
		}
	}
};