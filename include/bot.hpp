#pragma once

#include "cluster.h"
#include "vulkan_backend.h"

#include <random>

namespace vulkanbot {

enum class shader_type {
    vert, frag, comp, unknown
};
struct shader {
    std::string data;
    shader_type type;
    bool file;
};

struct animation {
	int frames;
	int fps;
	float tStart = 0.0;
	float tEnd = 1.0;
	long bitrate;
};

class VulkanBot
{
public:
    VulkanBot(const nlohmann::json& config);
    void run();
private:
	std::vector<unsigned char> download_image(const std::string& url);

    void do_compute(const dpp::interaction_create_t& event, const dpp::message& message, const shader& shader);
    void do_render(const dpp::interaction_create_t& event, const dpp::message& message, const shader& vertex, const shader& fragment,
		std::optional<animation> animation = std::nullopt);
	void do_render_animation_internal(const dpp::interaction_create_t& event, animation animation);

    void initVulkan(const nlohmann::json& config);

    dpp::cluster bot;

	VulkanBackend backend;
	std::random_device rd;
	std::mt19937 e2;
	std::uniform_real_distribution<> dist;

	bool m_renderProgress;
	unsigned int m_renderProgressDelay;

	int m_width;
	int m_height;

	int m_defaultFrames;
	int m_defaultFPS;

	float m_defaultStart;
	float m_defaultEnd;

	int m_maxFrames;
	long m_bitrate;
	long m_maxBitrate;

	struct animation_render_data {
		shader vert;
		shader frag;
		dpp::message_context_menu_t event;
	};
	unsigned long long int next_animation_id = 0;
	std::map<unsigned long long int, animation_render_data> pending_animations;

	std::mutex render_lock;
};

}
