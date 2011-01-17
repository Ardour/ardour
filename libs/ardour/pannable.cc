#include "ardour/automation_control.h"
#include "ardour/automation_list.h"
#include "ardour/pannable.h"
#include "ardour/session.h"

using namespace ARDOUR;

Pannable::Pannable (Session& s)
        : Automatable (s)
        , SessionHandleRef (s)
        , pan_azimuth_control (new AutomationControl (s, PanAzimuthAutomation, 
                                                      boost::shared_ptr<AutomationList>(new AutomationList(PanAzimuthAutomation)), ""))
        , pan_elevation_control (new AutomationControl (s, PanElevationAutomation, 
                                                        boost::shared_ptr<AutomationList>(new AutomationList(PanElevationAutomation)), ""))
        , pan_width_control (new AutomationControl (s, PanWidthAutomation, 
                                                    boost::shared_ptr<AutomationList>(new AutomationList(PanWidthAutomation)), ""))
        , pan_frontback_control (new AutomationControl (s, PanFrontBackAutomation, 
                                                        boost::shared_ptr<AutomationList>(new AutomationList(PanFrontBackAutomation)), ""))
        , pan_lfe_control (new AutomationControl (s, PanLFEAutomation, 
                                                  boost::shared_ptr<AutomationList>(new AutomationList(PanLFEAutomation)), ""))
        , _auto_state (Off)
        , _auto_style (Absolute)
{
        add_control (pan_azimuth_control);
        add_control (pan_elevation_control);
        add_control (pan_width_control);
        add_control (pan_frontback_control);
        add_control (pan_lfe_control);
}

void
Pannable::set_automation_state (AutoState state)
{
        if (state != _auto_state) {
                _auto_state = state;

                const Controls& c (controls());
        
                for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
                        boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                        if (ac) {
                                ac->alist()->set_automation_state (state);
                        }
                }
                
                session().set_dirty ();
                automation_state_changed (_auto_state);
        }
}

void
Pannable::set_automation_style (AutoStyle style)
{
        if (style != _auto_style) {
                _auto_style = style;

                const Controls& c (controls());
                
                for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
                        boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                        if (ac) {
                                ac->alist()->set_automation_style (style);
                        }
                }
                
                session().set_dirty ();
                automation_style_changed ();
        }
}

void
Pannable::start_touch (double when)
{
        const Controls& c (controls());
        
        for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
                boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                if (ac) {
                        ac->alist()->start_touch (when);
                }
        }
        g_atomic_int_set (&_touching, 1);
}

void
Pannable::stop_touch (bool mark, double when)
{
        const Controls& c (controls());
        
        for (Controls::const_iterator ci = c.begin(); ci != c.end(); ++ci) {
                boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                if (ac) {
                        ac->alist()->stop_touch (mark, when);
                }
        }
        g_atomic_int_set (&_touching, 0);
}
