#include "bot.hpp"

#include <codeccontext.h>
#include <format.h>
#include <formatcontext.h>

namespace vulkanbot {

void VulkanBot::do_render_animation_internal(const dpp::interaction_create_t& event, animation animation)
{
    long renderTime = 0L;
    auto t1 = std::chrono::high_resolution_clock::now();

    av::OutputFormat ofrmt;
    av::FormatContext octx;
    ofrmt.setFormat(std::string(), "/tmp/render.mp4");
    octx.setFormat(ofrmt);

    av::Codec ocodec = av::findEncodingCodec(ofrmt);
    av::VideoEncoderContext encoder{ocodec};

    av::Rational timebase = {1, static_cast<int>(animation.fps)};
    av::PixelFormat pixelFormat{"yuv420p"};
    encoder.setWidth(m_width);
    encoder.setHeight(m_height);
    encoder.setTimeBase(timebase);
    encoder.setBitRate(animation.bitrate);
    encoder.setGopSize(10);
    encoder.setMaxBFrames(1);
    encoder.setPixelFormat(pixelFormat);

    encoder.open(av::Codec{});

    av::Stream ost = octx.addStream(encoder);
    ost.setFrameRate(timebase);

    octx.openOutput("/tmp/render.mp4");
    octx.dump();
    octx.writeHeader();
    octx.flush();

    event.edit_response(std::format("Rendering... 0.00% (frame 0/{})", animation.frames));

    auto lastProgress = std::chrono::time_point<std::chrono::high_resolution_clock>();
    for(int i=0; i<animation.frames; i++)
    {
        if(m_renderProgress)
        {
            double percent = ((i+1)*100.0)/animation.frames;
            auto now = std::chrono::high_resolution_clock::now();
            // only report progress every n ms to avoid rate limit
            if(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgress).count() >= m_renderProgressDelay)
            {
                lastProgress = now;
                event.edit_response(std::format("Rendering... {:.2f}% (frame {}/{})", percent, (i+1), animation.frames));
            }
        }
        backend.updateUniformObject([this, animation, i](UniformBufferObject* ubo){
            ubo->time = ((animation.tEnd-animation.tStart)/animation.frames)*i + animation.tStart;
            ubo->random = dist(e2);
        });

        backend.renderFrame([&renderTime, pixelFormat, &encoder, &octx, timebase, i]
            (uint8_t* data, vk::DeviceSize size, int width, int height, vk::Result result, long time)
        {
            uint8_t *dataCopy = new uint8_t[size];
            memcpy(dataCopy, data, size);

            av::VideoFrame frame(dataCopy, size, pixelFormat, width, height);
            frame.setTimeBase(timebase);
            frame.setStreamIndex(0);
            frame.setPictureType();
            frame.setPts(av::Timestamp(i, timebase));

            av::Packet packet = encoder.encode(frame);
            packet.setPts(av::Timestamp(i, timebase));

            if(packet)
            {
                packet.setStreamIndex(0);
                octx.writePacket(packet);
            }

            renderTime += time;

            delete [] dataCopy;
        }, true);
    }
    octx.writeTrailer();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    dpp::message msg({}, "Rendering finished in "+std::to_string(duration)+" ms!");
    msg.add_file("render.mp4", dpp::utility::read_file("/tmp/render.mp4"));
    event.edit_response(msg);
}

}
