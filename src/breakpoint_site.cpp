#include <libsdb/breakpoint_site.hpp>
#include <sys/ptrace.h>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

namespace
{
    auto get_next_id()
    {
        static sdb::breakpoint_site::id_type id = 0;
        return ++id;
    }
}

sdb::breakpoint_site::breakpoint_site(process& proc, virt_addr address, bool is_hardware, bool is_internal)
    : process_{ &proc }, address_{ address }, is_enabled_{ false }, saved_data_{},
    is_hardware_(is_hardware), is_internal_(is_internal)
{
    id_ = is_internal_ ? -1 : get_next_id();
}

void sdb::breakpoint_site::enable()
{
    if (is_enabled_)
    {
        return;
    }
    
    if (is_hardware_)
    {
        hardware_register_index_ = process_->set_hardware_breakpoint(id_, address_);
    }
    else
    {
        errno = 0;
        std::uint64_t data = ptrace(PTRACE_PEEKDATA, process_->pid(), address_, nullptr);
        if (errno != 0)
        {
            error::send_errno("Enabling breakpoint site failed");
        }

        saved_data_ = static_cast<std::byte>(data & 0xff);

        std::uint64_t int3 = 0xcc;
        std::uint64_t data_with_int3 = ((data & ~0xff) | int3);

        if (ptrace(PTRACE_POKEDATA, process_->pid(), address_, data_with_int3) < 0)
        {
            error::send_errno("Enabling breakpoing site failed");
        }
    }

    is_enabled_ = true;
}

void sdb::breakpoint_site::disable()
{
    if (!is_enabled_)
    {
        return;
    }

    if (is_hardware_)
    {
        process_->clear_hardware_stoppoint(hardware_register_index_);
        hardware_register_index_ = -1;
    }
    else
    {
        errno = 0;
        std::uint64_t data = ptrace(PTRACE_PEEKDATA, process_->pid(), address_, nullptr);
        if (errno != 0)
        {
            error::send_errno("Disabling breakpoint site failed");
        }

        auto restored_data = ((data & ~0xff) | static_cast<std::uint8_t>(saved_data_));
        if (ptrace(PTRACE_POKEDATA, process_->pid(), address_, restored_data) < 0)
        {
            error::send_errno("Disabling breakpoint site failed");
        }
    }

    is_enabled_ = false;
}