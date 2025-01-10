#include "bot.hpp"

#include <lodepng.h>
#include <glm/gtx/string_cast.hpp>

namespace vulkanbot {

void VulkanBot::do_render(const dpp::interaction_create_t& event, const dpp::message& message, const shader& vert, const shader& frag, const std::string& texture, std::optional<animation> animation) {
    event.thinking();

    std::cout << "Acquiring render lock..." << std::endl;
    std::unique_lock lock(render_lock);
    std::cout << "Start rendering..." << std::endl;

    auto [result, error] = backend.uploadShaderMix(vert.data, vert.file, frag.data, frag.file, vk::CullModeFlagBits::eFront, true);
    if(!result) {
        event.edit_response("Error failed to upload shaders: "+error);
        return;
    }
    std::promise<std::string> p;
    std::future<std::string> f = p.get_future();
    bot.request(texture, dpp::http_method::m_get, [&p](const dpp::http_request_completion_t& c)mutable{
        p.set_value(c.body);
    });

    std::vector<unsigned char> image;
    unsigned int w, h;
    auto body = f.get();
    lodepng::decode(image, w, h, reinterpret_cast<unsigned char*>(body.data()), body.size());
    std::unique_ptr<ImageData> vkImage = backend.uploadImage(w, h, image);

    backend.buildCommandBuffer(nullptr, animation.has_value());

    if(animation) {
        do_render_animation_internal(event, *animation);
    }
    else {
        backend.updateUniformObject([this](UniformBufferObject* ubo){
            ubo->time = 0.0f;
            ubo->random = dist(e2);
        });

        backend.renderFrame([this, event](uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
        {
            std::vector<unsigned char> imageBytes(data, data + size);
            std::vector<unsigned char> png;
            lodepng::encode(png, imageBytes, width, height);

            dpp::message msg({}, "Rendering finished in "+std::to_string(time)+" μs!");
            msg.add_file("render.png", std::string(png.begin(), png.end()));
            event.edit_response(msg);
        });
    }
    std::cout << "Rendering finished!" << std::endl;
}

}
