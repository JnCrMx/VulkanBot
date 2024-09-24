#include "bot.hpp"

#include <dpp/dpp.h>
#include <av.h>
#include <avutils.h>
#include <filesystem>
#include <glm/gtx/string_cast.hpp>

#include "vulkan_backend.h"

using namespace vulkanbot;

namespace vulkanbot {
	std::vector<shader> find_shaders(const std::string& message, shader_type default_type = shader_type::frag) {
		std::vector<shader> shaders{};

		std::string::size_type pos = 0;
		while(pos < message.size() && pos != std::string::npos) {
			pos = message.find("``", pos);
			if(pos == std::string::npos) {
				break;
			}

			shader shader = {.type = shader_type::unknown};
			if(pos >= 4) {
				std::string t = message.substr(pos-4, 4);
				if(t == "vert") {
					shader.type = shader_type::vert;
				} else if(t == "frag") {
					shader.type = shader_type::frag;
				} else if(t == "comp") {
					shader.type = shader_type::comp;
				}
			}

			std::string quotes = message.substr(pos, 3);
			if(quotes == "```") {
				pos = message.find("\n", pos);
				auto begin = pos+1;

				pos = message.find("```", pos);

				std::string code = message.substr(begin, pos-begin);
				pos += 3;

				shader.data = code;
				shader.file = false;
				if(shader.type == shader_type::unknown) {
					if(code.contains("gl_Position")) {
						shader.type = shader_type::vert;
					} else {
						shader.type = default_type;
					}
				}
			} else {
				pos += 2;

				auto begin = pos;
				pos = message.find("``", pos);

				std::string file = message.substr(begin, pos-begin);
				pos += 2;

				std::filesystem::path p(file);
				shader.data = p;
				shader.file = true;

				if(shader.type == shader_type::unknown) {
					auto t = p.extension().string();
					if(t == "vert") {
						shader.type = shader_type::vert;
					} else if(t == "frag") {
						shader.type = shader_type::frag;
					} else if(t == "comp") {
						shader.type = shader_type::comp;
					} else {
						shader.type = default_type;
					}
				}
			}
			shaders.push_back(shader);
		}
		return shaders;
	}

	std::optional<std::filesystem::path> find_first_existing(std::initializer_list<std::filesystem::path> paths) {
		for(auto& p : paths) {
			if(std::filesystem::exists(p)) {
				return p;
			}
		}
		return std::nullopt;
	}

	VulkanBot::VulkanBot(const nlohmann::json& config) : bot(config["discord"]["token"], dpp::i_default_intents) {
		m_maxFrames = config["video"]["max"]["frames"];

		bot.on_log(dpp::utility::cout_logger());

		std::filesystem::path executable_path = std::filesystem::read_symlink("/proc/self/exe");
		std::filesystem::path shaders_path;
		if(config.contains("paths") && config["paths"].contains("shaders")) {
			shaders_path = config["paths"]["shaders"].get<std::string>();
		} else {
			shaders_path = find_first_existing({
				std::filesystem::current_path() / "shaders",
				executable_path.parent_path().parent_path() / "share" / "vulkan_bot" / "shaders"
			}).value_or("shaders");
		}
		if(!std::filesystem::exists(shaders_path)) {
			throw std::runtime_error("Shaders path does not exist");
		}
		std::cout << "Shaders path: " << shaders_path << std::endl;

		std::filesystem::path shader_include_path;
		if(config.contains("paths") && config["paths"].contains("shader_include")) {
			shader_include_path = config["shader_include_path"].get<std::string>();
		} else {
			shader_include_path = find_first_existing({
				std::filesystem::current_path() / "shader_include",
				executable_path.parent_path().parent_path() / "share" / "vulkan_bot" / "shader_include"
			}).value_or("shader_include");
		}
		if(!std::filesystem::exists(shader_include_path)) {
			throw std::runtime_error("Shader include path does not exist");
		}
		std::cout << "Shader include path: " << shader_include_path << std::endl;

		initVulkan(config, shaders_path, shader_include_path);
		bot.on_ready([this](const dpp::ready_t & event) {
	        if (dpp::run_once<struct register_bot_commands>()) {
				//bot.global_bulk_command_delete_sync();

				bot.global_command_create(dpp::slashcommand("render image", "Render as an image", bot.me.id).set_type(dpp::ctxm_message));
				bot.global_command_create(dpp::slashcommand("render video", "Render as an animation", bot.me.id).set_type(dpp::ctxm_message));
				bot.global_command_create(dpp::slashcommand("compute", "Execute as compute shader", bot.me.id).set_type(dpp::ctxm_message));
	        }
	    });
		bot.on_message_context_menu([this](const dpp::message_context_menu_t& event){
			shader_type def = event.command.get_command_name() == "compute" ? shader_type::comp : shader_type::frag;
			auto shaders = find_shaders(event.get_message().content, def);
			if(shaders.empty()) {
				event.reply("Error: No shaders found in message");
				return;
			}
			if(event.command.get_command_name() == "compute") {
				if(shaders.size() != 1 || shaders[0].type != shader_type::comp) {
					event.reply("Error: Exactly one compute shader required");
					return;
				}
				std::thread t([this, event, shaders](){
					do_compute(event, event.get_message(), shaders[0]);
				});
				t.detach();
			}
			else {
				if(shaders.size() > 2) {
					event.reply("Error: No more than two shaders allowed");
					return;
				}
				shader vert{.data = "base", .type = shader_type::vert, .file = true};
				shader frag{.data = "base", .type = shader_type::frag, .file = true};
				for(auto& s : shaders) {
					if(s.type == shader_type::vert) { vert = s; }
					else if(s.type == shader_type::frag) { frag = s; }
					else {
						event.reply("Error: Only vertex and fragment shaders allowed");
						return;
					}
				}
				if(event.command.get_command_name() == "render image") {
					std::thread t([this, event, vert, frag](){
						do_render(event, event.get_message(), vert, frag);
					});
					t.detach();
				} else if(event.command.get_command_name() == "render video") {
					unsigned long long int id = next_animation_id++;
					pending_animations[id] = {
						.vert = vert, .frag = frag, .event = event
					};
					dpp::interaction_modal_response modal(std::to_string(id), "Animation Settings");
					modal.add_component(dpp::component()
						.set_label("Frames") .set_id("frames")
						.set_type(dpp::cot_text)
						.set_placeholder(std::to_string(m_defaultFrames))
						.set_min_length(1) .set_max_length(5) .set_text_style(dpp::text_short));
					modal.add_row();
					modal.add_component(dpp::component()
						.set_label("FPS").set_id("fps")
						.set_type(dpp::cot_text)
						.set_placeholder(std::to_string(m_defaultFPS))
						.set_min_length(1).set_max_length(5).set_text_style(dpp::text_short));
					modal.add_row();
					modal.add_component(dpp::component()
						.set_label("Start value of t").set_id("t_start")
						.set_type(dpp::cot_text)
						.set_placeholder(std::to_string(m_defaultStart))
						.set_min_length(1).set_max_length(5).set_text_style(dpp::text_short));
					modal.add_row();
					modal.add_component(dpp::component()
						.set_label("End value of t").set_id("t_end")
						.set_type(dpp::cot_text)
						.set_placeholder(std::to_string(m_defaultEnd))
						.set_min_length(1).set_max_length(5).set_text_style(dpp::text_short));
					event.dialog(modal);
				} else {
					event.reply("Error: Unknown command");
				}
			}
		});
		bot.on_form_submit([this](const dpp::form_submit_t & event) {
			unsigned long long int id = std::stoull(event.custom_id);
	        animation_render_data data = pending_animations.at(id);
			pending_animations.erase(id);

			auto parse = []<typename T>(const std::string s)->std::optional<T> {
				T value{};
				if(std::from_chars(s.data(), s.data()+s.size(), value).ec == std::errc{}) {
					return value;
				} else {
					return std::nullopt;
				}
			};

			int frames = parse.template operator()<int>(std::get<std::string>(event.components[0].components[0].value)).value_or(m_defaultFrames);
			int fps = parse.template operator()<int>(std::get<std::string>(event.components[1].components[0].value)).value_or(m_defaultFPS);
			float tStart = parse.template operator()<float>(std::get<std::string>(event.components[2].components[0].value)).value_or(m_defaultStart);
			float tEnd = parse.template operator()<float>(std::get<std::string>(event.components[3].components[0].value)).value_or(m_defaultEnd);

			frames = std::min(frames, m_maxFrames);
			long bitrate = m_bitrate;

			animation a{frames, fps, tStart, tEnd, bitrate};

			std::thread t([this, event, data, a](){
				do_render(event, data.event.get_message(), data.vert, data.frag, a);
			});
			t.detach();
	    });
	}
	void VulkanBot::run() {
		bot.start(dpp::st_wait);
	}
	void VulkanBot::initVulkan(const nlohmann::json& config, const std::filesystem::path& shaders_path, const std::filesystem::path& shader_include_path) {
		m_width = config["image"]["width"];
		m_height = config["image"]["height"];

		m_maxFrames = config["video"]["max"]["frames"];
		m_maxBitrate = config["video"]["max"]["bitrate"];

		m_defaultFrames = config["video"]["default"]["frames"];
		m_defaultFPS = config["video"]["default"]["fps"];
		m_defaultStart = config["video"]["default"]["time"]["start"];
		m_defaultEnd = config["video"]["default"]["time"]["end"];
		m_bitrate = config["video"]["default"]["bitrate"];

		m_renderProgress = config["video"]["renderprogress"]["enable"];
		m_renderProgressDelay = config["video"]["renderprogress"]["delay"];

		e2 = std::mt19937(rd());
		dist = std::uniform_real_distribution<>(0.0, 1.0);

		bool vulkanValidate = false;
		int vulkanDebugSeverity = 0;
		int vulkanDebugType = 0;
		int avLogLevel = AV_LOG_ERROR;

		if(config.contains("debug"))
		{
			nlohmann::json debugStuff = config["debug"];
			if(debugStuff.contains("vulkan"))
			{
				vulkanValidate = debugStuff["vulkan"].contains("validation") ? (bool)debugStuff["vulkan"]["validation"] : false;
				vulkanDebugSeverity = debugStuff["vulkan"].contains("severity") ? (int)debugStuff["vulkan"]["severity"] :
					((uint32_t)vk::DebugUtilsMessageSeverityFlagsEXT(
						vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
						vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
						vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
						vk::DebugUtilsMessageSeverityFlagBitsEXT::eError));
				vulkanDebugType = debugStuff["vulkan"].contains("type") ? (int)debugStuff["vulkan"]["type"] :
					((uint32_t)vk::DebugUtilsMessageTypeFlagsEXT(
						vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation));
			}
			if(debugStuff.contains("av"))
			{
				if(debugStuff["av"].contains("verbose") && debugStuff["av"]["verbose"])
					avLogLevel = AV_LOG_VERBOSE;
				if(debugStuff["av"].contains("logLevel"))
					avLogLevel = debugStuff["av"]["logLevel"];
			}
		}
		av::init();
		av::setFFmpegLoggingLevel(avLogLevel);

		backend.initVulkan(m_width, m_height, shaders_path, shader_include_path, 
			vulkanValidate, vulkanDebugSeverity, vulkanDebugType);
	}
}

void INThandler(int sig)
{
	exit(0);
}

int main(int argc, char** argv)
{
	std::signal(SIGINT, INThandler);

	std::filesystem::path config_path;
	if(argc > 1) {
		config_path = argv[1];
	} else if(const char* env = std::getenv("VULKAN_BOT_CONFIG")) {
		config_path = env;
	} else {
		config_path = find_first_existing({
			"/etc/vulkan_bot/config.json",
			std::filesystem::current_path() / "config.json",
		}).value_or("config.json");
	}

	nlohmann::json j;
	{
		std::ifstream config(config_path);
		if(!config) {
			std::cerr << "Could not open config file at " << config_path << std::endl;
			return 1;
		}
		config >> j;
	}

	VulkanBot client(j);
	client.run();

	return 0;
}
