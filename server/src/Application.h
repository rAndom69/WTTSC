
#pragma once

#include <Poco/Util/ServerApplication.h>

class Application : public Poco::Util::ServerApplication
{
	typedef Poco::Util::ServerApplication _super;

public:
	virtual void initialize(Poco::Util::Application& self) override;
	virtual int main(const std::vector < std::string > & args) override;

};