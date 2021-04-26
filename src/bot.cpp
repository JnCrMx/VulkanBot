#include "sleepy_discord/attachment.h"
#include "sleepy_discord/client.h"
#include "vulkan_backend.h"
#include <bits/stdint-uintn.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <ratio>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <optional>
#include <csignal>
#include <vulkan/vulkan.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "lodepng.h"
#include "sleepy_discord/sleepy_discord.h"
#include <glm/gtx/string_cast.hpp>

#include "rational.h"
#include "av.h"
#include "ffmpeg.h"
#include "codec.h"
#include "packet.h"
#include "videorescaler.h"
#include "audioresampler.h"
#include "avutils.h"
#include "format.h"
#include "formatcontext.h"
#include "codeccontext.h"
#include "timestamp.h"
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>

using namespace vulkanbot;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::vector<unsigned char>* vec = (std::vector<unsigned char>*)userp;
	vec->insert(vec->end(), (unsigned char*)contents, (unsigned char*) contents + size * nmemb);
	return size * nmemb;
}

static size_t WriteStringCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::string* string = (std::string*)userp;
	string->append(std::string((char*)contents));
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
		m_defaultFPS = config["defaultFPS"];
		m_defaultStart = config["defaultStart"];
		m_defaultEnd = config["defaultEnd"];
		m_bitrate = config["bitrate"];
		m_maxBitrate = config["maxBitrate"];

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
							end = vertexEnd + 2;
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
							end = fragmentEnd + 2;
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
								end = fragmentEnd + 2;
						}
					}
				}
			}

			std::optional<SleepyDiscord::Attachment> imageAttachment;
			std::optional<SleepyDiscord::Attachment> meshAttachment;
			for(const SleepyDiscord::Attachment& a : message.attachments)
			{
				if(!imageAttachment.has_value() && a.filename.ends_with(".png"))
					imageAttachment = a;
				if(!meshAttachment.has_value() && a.filename.ends_with(".obj"))
					meshAttachment = a;
			}

			auto animated = message.content.find("animated", end);
			bool nocull = message.content.find("nocull", end) != std::string::npos;
			bool nodepth = message.content.find("nodepth", end) != std::string::npos;

			auto [result, error] = backend.uploadShaderMix(vertexShader.value_or(vertexPath.value_or("base")), !vertexShader.has_value(),
				fragmentShader.value_or(fragmentPath.value_or("base")), !fragmentShader.has_value(),
				nocull ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eFront, !nodepth);

			if(result)
			{
				std::string url = "https://cdn.discordapp.com/avatars/"+message.author.ID.string()+"/"+message.author.avatar+".png";
				if(imageAttachment.has_value())
					url = imageAttachment->url;
				std::vector<unsigned char> png;

				CURL *curl;
				curl = curl_easy_init();
				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png);
				curl_easy_perform(curl);
				curl_easy_cleanup(curl);

				std::vector<unsigned char> image;
				unsigned int w, h;
				lodepng::decode(image, w, h, png);
				std::unique_ptr<ImageData> vkImage = backend.uploadImage(w, h, image);

				std::unique_ptr<Mesh> mesh;
				if(meshAttachment.has_value())
				{
					std::string objString;

					curl = curl_easy_init();
					curl_easy_setopt(curl, CURLOPT_URL, meshAttachment->url.c_str());
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStringCallback);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &objString);
					curl_easy_perform(curl);
					curl_easy_cleanup(curl);

					std::vector<glm::vec3> vertices;
					std::vector<glm::vec2> weirdTexCoords;
					std::vector<glm::vec2> texCoords;
					std::vector<glm::vec3> weirdNormals;
					std::vector<glm::vec3> normals;
					std::vector<uint16_t> indices;

					std::istringstream stream(objString);
					std::string line;
					while(std::getline(stream, line))
					{
						std::istringstream lineStream(line);
						std::string op;
						lineStream >> op;

						if(op == "v")
						{
							float x, y, z;
							lineStream >> x >> y >> z;
							vertices.push_back(glm::vec3(x, y, z));
						}
						else if(op == "vt")
						{
							float u, v;
							lineStream >> u >> v;
							weirdTexCoords.push_back(glm::vec2(u, v));
						}
						else if(op == "vn")
						{
							float x, y, z;
							lineStream >> x >> y >> z;
							weirdNormals.push_back(glm::vec3(x, y, z));
						}
						else if(op == "f")
						{
							std::array<std::string, 3> args;
							lineStream >> args[0] >> args[1] >> args[2];

							for(int i=0; i<3; i++)
							{
								std::istringstream aStream(args[i]);
								int vertex, texCoord, normal;
								aStream >> vertex;
								aStream.ignore(1);
								aStream >> texCoord;
								aStream.ignore(1);
								aStream >> normal;

								texCoords.resize(std::max<int>(vertex, vertices.size()));
								texCoords[vertex-1] = weirdTexCoords[texCoord-1];
								normals.resize(std::max<int>(vertex, vertices.size()));
								normals[vertex-1] = weirdNormals[normal-1];

								indices.push_back(vertex-1);
							}
						}
					}
					mesh = backend.uploadMesh(vertices, texCoords, normals, indices);
					backend.buildCommandBuffer(mesh.get(), animated != std::string::npos);
				}
				else
				{
					backend.buildCommandBuffer(nullptr, animated != std::string::npos);
				}

				if(animated != std::string::npos)
				{
					int frames = m_defaultFrames;
					int fps = m_defaultFPS;
					float tStart = m_defaultStart;
					float tEnd = m_defaultEnd;
					long bitrate = m_bitrate;
					try
					{
						std::istringstream stringstream(message.content.substr(animated+9));
						stringstream >> frames;
						stringstream >> fps;
						stringstream >> tStart;
						stringstream >> tEnd;

						std::string s_bitrate;
						stringstream >> s_bitrate;
						std::stringstream s(s_bitrate.substr(0, s_bitrate.size() - (std::isalpha(s_bitrate.back()) ? 1 : 0)));
						s >> bitrate;
						switch(s_bitrate.back())
						{
							case 'k':
							case 'K':
								bitrate *= 1000;
								break;
							case 'm':
							case 'M':
								bitrate *= 1000000;
								break;
							default:
								break;
						}
					}
					catch(const std::out_of_range& ex) {}
					if(frames > m_maxFrames)
						frames = m_maxFrames;
					if(bitrate > m_maxBitrate)
						bitrate = m_maxBitrate;

					long renderTime = 0L;
					auto t1 = std::chrono::high_resolution_clock::now();

					av::OutputFormat ofrmt;
					av::FormatContext octx;
					ofrmt.setFormat(std::string(), "/tmp/render.mp4");
					octx.setFormat(ofrmt);
					av::Codec ocodec = av::findEncodingCodec(ofrmt);
					av::Stream ost = octx.addStream(ocodec);
					av::VideoEncoderContext encoder {ost, ocodec};

					av::Rational timeBase = {1, fps};
					encoder.setWidth(m_width);
					encoder.setHeight(m_height);
					encoder.setTimeBase(timeBase);
					encoder.setBitRate(bitrate);
					encoder.setGopSize(10);
					encoder.setMaxBFrames(1);

					av::PixelFormat pixelFormat("yuv420p");

					encoder.setPixelFormat(pixelFormat);
					ost.setFrameRate(timeBase);

					octx.openOutput("/tmp/render.mp4");
					encoder.open(ocodec);
					octx.dump();
					octx.writeHeader();
					octx.flush();

					for(int i=0; i<frames; i++)
					{
						backend.updateUniformObject([this, frames, tStart, tEnd, i](UniformBufferObject* ubo){
							ubo->time = ((tEnd-tStart)/frames)*i + tStart;
							ubo->random = dist(e2);
						});

						backend.renderFrame([&renderTime, pixelFormat, &encoder, &octx, timeBase, i]
							(uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
						{
							uint8_t *dataCopy = new uint8_t[size];
							memcpy(dataCopy, data, size);

							av::VideoFrame frame(dataCopy, size, pixelFormat, width, height);
							frame.setTimeBase(timeBase);
							frame.setStreamIndex(0);
							frame.setPictureType();
							frame.setPts(av::Timestamp(i, timeBase));

							av::Packet packet = encoder.encode(frame);
							if(packet)
							{
								octx.writePacket(packet);
							}

							renderTime += time;

							delete [] dataCopy;
						}, true);
					}

					for(;;)
					{
						av::Packet packet = encoder.encode();
						if(packet)
						{
							octx.writePacket(packet);
						}
						else
							break;
					}

					octx.writeTrailer();

					auto t2 = std::chrono::high_resolution_clock::now();
					auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
					uploadFile(message.channelID, "/tmp/render.mp4", "Finished in "+std::to_string(duration)+" ms! "+
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
		else if(message.startsWith("comp```glsl") || message.startsWith("compfile``"))
		{
			sendTyping(message.channelID);
			std::string::size_type end = 0;

			std::optional<std::string> computeShader;
			std::optional<std::string> computePath;

			auto computeStart = message.content.find("comp```glsl");
			if(computeStart != std::string::npos)
			{
				computeStart += 11 + 1;
				auto computeEnd = message.content.find("```", computeStart);
				if(computeEnd != std::string::npos)
				{
					computeShader = message.content.substr(computeStart, computeEnd - computeStart);
					if(computeEnd > end)
						end = computeEnd + 3;
				}
			}
			if(!computeShader.has_value())
			{
				computeStart = message.content.find("compfile``");
				if(computeStart != std::string::npos)
				{
					computeStart += 10;
					auto computeEnd = message.content.find("``", computeStart);
					if(computeEnd != std::string::npos)
					{
						computePath = message.content.substr(computeStart, computeEnd - computeStart);
						if(computeEnd > end)
							end = computeEnd + 2;
					}
				}
			}

			std::optional<SleepyDiscord::Attachment> imageAttachment;
			for(const SleepyDiscord::Attachment& a : message.attachments)
			{
				if(!imageAttachment.has_value() && a.filename.ends_with(".png"))
					imageAttachment = a;
			}

			auto [result, error] = backend.uploadComputeShader(computeShader.value_or(computePath.value_or("base")), !computeShader.has_value());
			if(result)
			{
				std::string url = "https://cdn.discordapp.com/avatars/"+message.author.ID.string()+"/"+message.author.avatar+".png";
				if(imageAttachment.has_value())
					url = imageAttachment->url;
				std::vector<unsigned char> png;

				CURL *curl;
				curl = curl_easy_init();
				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &png);
				curl_easy_perform(curl);
				curl_easy_cleanup(curl);

				std::vector<unsigned char> image;
				unsigned int w, h;
				lodepng::decode(image, w, h, png);
				std::unique_ptr<ImageData> vkImage = backend.uploadImage(w, h, image);

				backend.buildComputeCommandBuffer(1, 1, 1);

				backend.updateUniformObject([this](UniformBufferObject* ubo){
						ubo->time = 0.0f;
						ubo->random = dist(e2);
				});

				backend.doComputation([this, message](OutputStorageObject* data, vk::Result result, long time)
				{
					std::string value =
						"float: " + std::to_string(data->as_float) + "\n" +
						"int  : " + std::to_string(data->as_int) + "\n" +
						"vec4 : " + glm::to_string(data->as_vec4) + "\n" +
						"ivec4: " + glm::to_string(data->as_ivec4) + "\n" +
						"chars: " + data->charsToString();
					sendMessage(message.channelID, "Computation finished in "+std::to_string(time)+" μs!```"+value+"```");
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
	std::random_device rd;
	std::mt19937 e2;
	std::uniform_real_distribution<> dist;

	int m_width;
	int m_height;

	int m_defaultFrames;
	int m_defaultFPS;

	float m_defaultStart;
	float m_defaultEnd;

	int m_maxFrames;
	long m_bitrate;
	long m_maxBitrate;
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
	av::init();
	av::setFFmpegLoggingLevel(AV_LOG_ERROR);

	std::ifstream config("config.json");
	nlohmann::json j;
	config >> j;

	client = new MyClientClass(j["token"], SleepyDiscord::USER_CONTROLED_THREADS);
	client->initVulkan(j);
	client->run();

	return 0;
}
