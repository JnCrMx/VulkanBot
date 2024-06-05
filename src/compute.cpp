#include "bot.hpp"

#include <lodepng.h>
#include <glm/gtx/string_cast.hpp>

namespace vulkanbot {

void VulkanBot::do_compute(const dpp::interaction_create_t& event, const dpp::message& message, const shader& shader) {
    event.thinking();

    std::unique_lock lock(render_lock);

    auto [result, error] = backend.uploadComputeShader(shader.data, shader.file);
    if(!result) {
        event.edit_response("Error failed to upload shader: "+error);
        return;
    }
    std::string url = message.author.get_avatar_url();

    std::promise<std::string> p;
    std::future<std::string> f = p.get_future();
    bot.request(url, dpp::http_method::m_get, [&p](const dpp::http_request_completion_t& c)mutable{
        p.set_value(c.body);
    });

    std::vector<unsigned char> image;
    unsigned int w, h;
    auto body = f.get();
    lodepng::decode(image, w, h, reinterpret_cast<unsigned char*>(body.data()), body.size());
    std::unique_ptr<ImageData> vkImage = backend.uploadImage(w, h, image);

    backend.buildComputeCommandBuffer(1, 1, 1);

    backend.updateUniformObject([this](UniformBufferObject* ubo){
            ubo->time = 0.0f;
            ubo->random = dist(e2);
    });
    backend.doComputation([this, event](OutputStorageObject* data, vk::Result result, long time)
    {
        std::string value =
            "float: " + std::to_string(data->as_float) + "\n" +
            "int  : " + std::to_string(data->as_int) + "\n" +
            "vec4 : " + glm::to_string(data->as_vec4) + "\n" +
            "ivec4: " + glm::to_string(data->as_ivec4) + "\n" +
            "chars: " + data->charsToString();
        dpp::message msg({}, "Computation finished in "+std::to_string(time)+" μs!```"+value+"```");
        event.edit_response(msg);
    });
}

}
