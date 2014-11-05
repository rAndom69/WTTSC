
#pragma once

class IResourceCollection
{
public:
	virtual ~IResourceCollection()
	{
	}
	virtual void Reload() abstract;
};

template <typename TResource>
class BaseResourceCollection : public IResourceCollection
{
protected:
	typedef std::vector<std::shared_ptr<TResource>> TCollection; 
	TCollection m_Resources;
public:
	typename TCollection::const_iterator begin() const {
		return m_Resources.begin();
	}
	typename TCollection::const_iterator end() const {
		return m_Resources.end();
	}
};