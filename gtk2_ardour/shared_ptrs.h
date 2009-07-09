#include <boost/shared_ptr.hpp>

class TimeAxisView;

typedef boost::shared_ptr<TimeAxisView> TimeAxisViewPtr;
typedef boost::shared_ptr<const TimeAxisView> TimeAxisViewConstPtr;

class RouteTimeAxisView;

typedef boost::shared_ptr<RouteTimeAxisView> RouteTimeAxisViewPtr;
typedef boost::shared_ptr<const RouteTimeAxisView> RouteTimeAxisViewConstPtr;

class AutomationTimeAxisView;

typedef boost::shared_ptr<AutomationTimeAxisView> AutomationTimeAxisViewPtr;
typedef boost::shared_ptr<const AutomationTimeAxisView> AutomationTimeAxisViewConstPtr;

class AudioTimeAxisView;

typedef boost::shared_ptr<AudioTimeAxisView> AudioTimeAxisViewPtr;

class MidiTimeAxisView;

typedef boost::shared_ptr<MidiTimeAxisView> MidiTimeAxisViewPtr;
typedef boost::shared_ptr<const MidiTimeAxisView> MidiTimeAxisViewConstPtr;
