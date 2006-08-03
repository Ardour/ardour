#include <ardour/session.h>
#include <ardour/route.h>

namespace ARDOUR {
// solo
Session::GlobalSoloStateCommand::GlobalSoloStateCommand(Session &sess, void *src)
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::soloed);
}
void Session::GlobalSoloStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::soloed);
}
void Session::GlobalSoloStateCommand::operator()()
{
    sess.set_global_solo(after, src);
}
void Session::GlobalSoloStateCommand::undo()
{
    sess.set_global_solo(before, src);
}
XMLNode &Session::GlobalSoloStateCommand::serialize()
{
}

// mute
Session::GlobalMuteStateCommand::GlobalMuteStateCommand(Session &sess, void *src)
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::muted);
}
void Session::GlobalMuteStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::muted);
}
void Session::GlobalMuteStateCommand::operator()()
{
    sess.set_global_mute(after, src);
}
void Session::GlobalMuteStateCommand::undo()
{
    sess.set_global_mute(before, src);
}
XMLNode &Session::GlobalMuteStateCommand::serialize()
{
}

// record enable
Session::GlobalRecordEnableStateCommand::GlobalRecordEnableStateCommand(Session &sess, void *src) 
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::record_enabled);
}
void Session::GlobalRecordEnableStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::record_enabled);
}
void Session::GlobalRecordEnableStateCommand::operator()()
{
    sess.set_global_record_enable(after, src);
}
void Session::GlobalRecordEnableStateCommand::undo()
{
    sess.set_global_record_enable(before, src);
}
XMLNode &Session::GlobalRecordEnableStateCommand::serialize()
{
}

// metering
Session::GlobalMeteringStateCommand::GlobalMeteringStateCommand(Session &sess, void *src) 
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_metering();
}
void Session::GlobalMeteringStateCommand::mark()
{
    after = sess.get_global_route_metering();
}
void Session::GlobalMeteringStateCommand::operator()()
{
    sess.set_global_route_metering(after, src);
}
void Session::GlobalMeteringStateCommand::undo()
{
    sess.set_global_route_metering(before, src);
}
XMLNode &Session::GlobalMeteringStateCommand::serialize()
{
}

} // namespace ARDOUR
