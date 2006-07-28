#include <ardour/session.h>

// solo
Session::GlobalSoloStateCommand::GlobalSoloStateCommand(void *src) : src(src)
{
    after = before = get_global_route_boolean(&Route::soloed);
}
void Session::GlobalSoloStateCommand::mark()
{
    after = get_global_route_boolean(&Route::soloed);
}
void operator()()
{
    set_global_solo(after, src);
}
void undo()
{
    set_global_solo(before, src);
}
XMLNode &serialize()
{
}

// mute
Session::GlobalMuteStateCommand::GlobalMuteStateCommand(void *src) : src(src)
{
    after = before = get_global_route_boolean(&Route::muted);
}
void Session::GlobalMuteStateCommand::mark()
{
    after = get_global_route_boolean(&Route::muted);
}
void operator()()
{
    set_global_mute(after, src);
}
void undo()
{
    set_global_mute(before, src);
}
XMLNode &serialize()
{
}

// record enable
Session::GlobalRecordEnableStateCommand::GlobalRecordEnableStateCommand(void *src) : src(src)
{
    after = before = get_global_route_boolean(&Route::record_enabled);
}
void Session::GlobalRecordEnableStateCommand::mark()
{
    after = get_global_route_boolean(&Route::record_enabled);
}
void operator()()
{
    set_global_record_enable(after, src);
}
void undo()
{
    set_global_record_enable(before, src);
}
XMLNode &serialize()
{
}

// metering
Session::GlobalMeteringStateCommand::GlobalMeteringStateCommand(void *src) : src(src)
{
    after = before = get_global_route_metering();
}
void Session::GlobalMeteringStateCommand::mark()
{
    after = get_global_route_metering();
}
void operator()()
{
    set_global_route_metering(after, src);
}
void undo()
{
    set_global_route_metering(before, src);
}
XMLNode &serialize()
{
}

