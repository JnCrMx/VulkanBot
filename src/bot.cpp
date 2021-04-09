#include "sleepy_discord/client.h"
#include "vulkan_backend.h"
#include <bits/stdint-uintn.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ratio>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <optional>
#include <csignal>
#include <vulkan/vulkan.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <gif.h>
#include "lodepng.h"
#include "sleepy_discord/sleepy_discord.h"

using namespace vulkanbot;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::vector<unsigned char>* vec = (std::vector<unsigned char>*)userp;
    vec->insert(vec->end(), (unsigned char*)contents, (unsigned char*) contents + size * nmemb);
    return size * nmemb;
}

class MyClientClass : public SleepyDiscord::DiscordClient
{
public:
	using SleepyDiscord::DiscordClient::DiscordClient;
	void initVulkan(nlohmann::json config)
	{
		m_width = config["width"];
		m_height = config["height"];
		m_maxFrames = config["maxFrames"];
		m_defaultFrames = config["defaultFrames"];
		m_defaultFrameDelay = config["defaultFrameDelay"];
		m_defaultStart = config["defaultStart"];
		m_defaultEnd = config["defaultEnd"];

		e2 = std::mt19937(rd());
		dist = std::uniform_real_distribution<>(0.0, 1.0);

		curl_global_init(CURL_GLOBAL_ALL);
		backend.initVulkan(m_width, m_height);
	}

	void onMessage(SleepyDiscord::Message message) override
	{
		if(message.startsWith("```glsl") || message.startsWith("vert```glsl") || message.startsWith("frag```glsl") ||
			message.startsWith("file``") || message.startsWith("vertfile``") || message.startsWith("fragfile``"))
		{
			sendTyping(message.channelID);

			std::optional<std::string> vertexShader;
			std::optional<std::string> fragmentShader;
			std::optional<std::string> vertexPath;
			std::optional<std::string> fragmentPath;

			std::string::size_type end = 0;
			auto vertexStart = message.content.find("vert```glsl");
			if(vertexStart != std::string::npos)
			{
				vertexStart += 11 + 1;
				auto vertexEnd = message.content.find("```", vertexStart);
				if(vertexEnd != std::string::npos)
				{
					vertexShader = message.content.substr(vertexStart, vertexEnd - vertexStart);
					if(vertexEnd > end)
						end = vertexEnd + 3;
				}
			}

			auto fragmentStart = message.content.find("frag```glsl");
			if(fragmentStart != std::string::npos)
			{
				fragmentStart += 11 + 1;
				auto fragmentEnd = message.content.find("```", fragmentStart);
				if(fragmentEnd != std::string::npos)
				{
					fragmentShader = message.content.substr(fragmentStart, fragmentEnd - fragmentStart);
					if(fragmentEnd > end)
						end = fragmentEnd + 3;
				}
			}
			if(!fragmentShader.has_value())
			{
				fragmentStart = message.content.find("```glsl");
				if(fragmentStart != std::string::npos && (fragmentStart += 7 + 1) != vertexStart)
				{
					auto fragmentEnd = message.content.find("```", fragmentStart);
					if(fragmentEnd != std::string::npos)
					{
						fragmentShader = message.content.substr(fragmentStart, fragmentEnd - fragmentStart);
						if(fragmentEnd > end)
							end = fragmentEnd + 3;
					}
				}
			}

			if(!vertexShader.has_value())
			{
				vertexStart = message.content.find("vertfile``");
				if(vertexStart != std::string::npos)
				{
					vertexStart += 10;
					auto vertexEnd = message.content.find("``", vertexStart);
					if(vertexEnd != std::string::npos)
					{
						vertexPath = message.content.substr(vertexStart, vertexEnd - vertexStart);
						if(vertexEnd > end)
							end = vertexEnd + 3;
					}
				}
			}
			if(!fragmentShader.has_value())
			{
				fragmentStart = message.content.find("fragfile``");
				if(fragmentStart != std::string::npos)
				{
					fragmentStart += 10;
					auto fragmentEnd = message.content.find("``", fragmentStart);
					if(fragmentEnd != std::string::npos)
					{
						fragmentPath = message.content.substr(fragmentStart, fragmentEnd - fragmentStart);
						if(fragmentEnd > end)
							end = fragmentEnd + 3;
					}
				}
				if(!fragmentPath.has_value())
				{
					fragmentStart = message.content.find("file``");
					if(fragmentStart != std::string::npos && (fragmentStart += 6) != vertexStart)
					{
						auto fragmentEnd = message.content.find("``", fragmentStart);
						if(fragmentEnd != std::string::npos)
						{
							fragmentPath = message.content.substr(fragmentStart, fragmentEnd - fragmentStart);
							if(fragmentEnd > end)
								end = fragmentEnd + 3;
						}
					}
				}
			}

			auto animated = message.content.find("animated", end);

			auto [result, error] = backend.uploadShaderMix(vertexShader.value_or(vertexPath.value_or("base")), !vertexShader.has_value(), 
				fragmentShader.value_or(fragmentPath.value_or("base")), !fragmentShader.has_value());

			if(result)
			{
				std::string avatarUrl = "https://cdn.discordapp.com/avatars/"+message.author.ID.string()+"/"+message.author.avatar+".png";
				std::vector<unsigned char> png;

				CURL *curl;
				curl = curl_easy_init();
				curl_easy_setopt(curl, CURLOPT_URL, avatarUrl.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png);
				curl_easy_perform(curl);
    			curl_easy_cleanup(curl);
				
				std::vector<unsigned char> image;
				unsigned int w, h;
				lodepng::decode(image, w, h, png);
				backend.readImage(image);

				if(animated != std::string::npos)
				{
					int frames = m_defaultFrames;
					int frameDelay = m_defaultFrameDelay;
					float tStart = m_defaultStart;
					float tEnd = m_defaultEnd;
					try
					{
						std::istringstream stringstream(message.content.substr(animated+9));
						stringstream >> frames;
						stringstream >> frameDelay;
						stringstream >> tStart;
						stringstream >> tEnd;
					}
					catch(const std::out_of_range& ex) {}
					if(frames > m_maxFrames)
						frames = m_maxFrames;

					long renderTime = 0L;
					auto t1 = std::chrono::high_resolution_clock::now();
					GifWriter g;
					GifBegin(&g, "/tmp/render.gif", m_width, m_height, frameDelay);
					for(int i=0; i<frames; i++)
					{
						backend.updateUniformObject([this, frames, tStart, tEnd, i](UniformBufferObject* ubo){
							ubo->time = ((tEnd-tStart)/frames)*i + tStart;
							ubo->random = dist(e2);
						});

						backend.renderFrame([frameDelay, &g, &renderTime](uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
						{
							uint8_t *dataCopy = new uint8_t[size];
							memcpy(dataCopy, data, size);

							GifWriteFrame(&g, dataCopy, width, height, frameDelay);
							renderTime += time;

							delete [] dataCopy;
						});

						struct stat64 stat;
						stat64("/tmp/render.gif", &stat);
						if(stat.st_size > 7000000)
							break;
					}
					GifEnd(&g);
					auto t2 = std::chrono::high_resolution_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
					uploadFile(message.channelID, "/tmp/render.gif", "Finished in "+std::to_string(duration)+" ms! "+
						"Rendering took "+std::to_string(renderTime/1000)+" ms!");
				}
				else
				{
					backend.updateUniformObject([this](UniformBufferObject* ubo){
						ubo->time = 0.0f;
						ubo->random = dist(e2);
					});

					backend.renderFrame([this, message](uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
					{
						std::vector<unsigned char> imageBytes(data, data + size);
						lodepng::encode("/tmp/render.png", imageBytes, width, height);
						uploadFile(message.channelID, "/tmp/render.png", "Rendering finished in "+std::to_string(time)+" μs!");
					});
				}
			}
			else
			{
				sendMessage(message.channelID, error);
			}
		}
	}
private:
	VulkanBackend backend;
	std::random_device rd;
	std::mt19937 e2;
	std::uniform_real_distribution<> dist;

	int m_width;
	int m_height;

	int m_defaultFrames;
	int m_defaultFrameDelay;

	float m_defaultStart;
	float m_defaultEnd;

	int m_maxFrames;
};

static MyClientClass* client;

void INThandler(int sig)
{
	if(client)
		delete client;
	exit(0);
}

int main()
{
	std::signal(SIGINT, INThandler);

	std::ifstream config("config.json");
	nlohmann::json j;
	config >> j;

	client = new MyClientClass(j["token"], SleepyDiscord::USER_CONTROLED_THREADS);
	client->initVulkan(j);
	client->run();

	return 0;
}
