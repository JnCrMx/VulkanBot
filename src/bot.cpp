#include "sleepy_discord/client.h"
#include "vulkan_backend.h"
#include <bits/stdint-uintn.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
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
	void initVulkan(int width, int height)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		backend.initVulkan(width, height);
	}

	void onMessage(SleepyDiscord::Message message) override
	{
		if(message.startsWith("```glsl"))
		{
			sendTyping(message.channelID);
			std::string code = message.content.substr(8, message.content.length() - 8 - 3);
			auto [result, error] = backend.uploadShader(code);
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

				backend.renderFrame([this, message](uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
				{
					std::vector<unsigned char> imageBytes(data, data + size);
					lodepng::encode("/tmp/render.png", imageBytes, width, height);
					uploadFile(message.channelID, "/tmp/render.png", "Rendering finished in "+std::to_string(time)+" μs!");
				});
			}
			else
			{
				sendMessage(message.channelID, error);
			}
		}
	}
private:
	VulkanBackend backend;
};

int main()
{
	std::ifstream config("config.json");
	nlohmann::json j;
	config >> j;

	MyClientClass client(j["token"], SleepyDiscord::USER_CONTROLED_THREADS);
	client.initVulkan(j["width"], j["height"]);
	client.run();

	return 0;
}
