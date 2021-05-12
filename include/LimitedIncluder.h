#pragma once

#include <fstream>
#include <string>
#include <ShaderLang.h>

namespace vulkan_bot
{
	class LimitedIncluder : public glslang::TShader::Includer
	{
		public:
			LimitedIncluder(std::string directory) : m_directory(directory) {}

			virtual IncludeResult* includeLocal(const char* headerName,
												const char* includerName,
												size_t inclusionDepth) override
			{
				if(std::string(headerName).find("../") != std::string::npos)
					return nullptr;
				std::string path = m_directory + '/' + headerName;

				std::ifstream stream(path, std::ios_base::binary | std::ios_base::ate);
				if(!stream)
					return nullptr;

				int length = stream.tellg();
				char* content = new tUserDataElement [length];
				stream.seekg(0, stream.beg);
				stream.read(content, length);
				return new IncludeResult(path, content, length, content);
			}

			virtual void releaseInclude(IncludeResult* result) override
			{
				if(result != nullptr)
				{
					delete [] static_cast<tUserDataElement*>(result->userData);
					delete result;
				}
			}
		private:
			typedef char tUserDataElement;
			std::string m_directory;
	};
};
