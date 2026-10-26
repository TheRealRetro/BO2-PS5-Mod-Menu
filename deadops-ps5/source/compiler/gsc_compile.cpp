#include "stdinc.hpp"
#include "t6/t6.hpp"

#include "gsc_compile.hpp"

namespace {
std::unique_ptr<xsk::arc::t6::context> g_ctx;
std::vector<std::uint8_t> g_out;
std::string g_error;

auto ensure_context() -> xsk::arc::t6::context&
{
    if (!g_ctx)
    {
        g_ctx = std::make_unique<xsk::arc::t6::context>(xsk::arc::t6::platform::ps4);

        g_ctx->init(xsk::arc::build::prod, [](std::string const& file) -> std::vector<std::uint8_t> {
            throw std::runtime_error("cannot read '" + file + "': no scripts on disk");
        });
    }

    return *g_ctx;
}
}

extern "C" const void* GSC_CompileT6(const char* name, const void* src, size_t srcLen, size_t* outLen)
{
    g_error.clear();

    if (outLen)
        *outLen = 0;

    if (!name || !src)
    {
        g_error = "no source given";
        return nullptr;
    }

    try
    {
        auto& ctx = ensure_context();

        auto const* bytes = static_cast<std::uint8_t const*>(src);
        auto data = std::vector<std::uint8_t>{ bytes, bytes + srcLen };

        auto path = std::string{ name };

        ctx.compiler().compile(path, data);
        auto assembly = ctx.compiler().output();
        ctx.assembler().assemble(path, assembly);

        g_out = ctx.assembler().output();

        if (g_out.empty())
        {
            g_error = "compiler produced nothing";
            return nullptr;
        }

        if (outLen)
            *outLen = g_out.size();

        return g_out.data();
    }
    catch (std::exception const& e)
    {
        g_error = e.what();
    }
    catch (...)
    {
        g_error = "unknown compiler failure";
    }

    return nullptr;
}

extern "C" const char* GSC_CompileError(void)
{
    return g_error.c_str();
}

extern "C" void GSC_CompileShutdown(void)
{
    g_ctx.reset();
    g_out.clear();
    g_out.shrink_to_fit();
    g_error.clear();
}
