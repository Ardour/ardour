#ifndef __ardour_analyser_h__
#define __ardour_analyser_h__

#include <glibmm/threads.h>
#include <boost/shared_ptr.hpp>

namespace ARDOUR {

class AudioFileSource;
class Source;
class TransientDetector;

class Analyser {

  public:
	Analyser();
	~Analyser ();

	static void init ();
	static void queue_source_for_analysis (boost::shared_ptr<Source>, bool force);
	static void work ();

  private:
	static Analyser* the_analyser;
        static Glib::Threads::Mutex analysis_queue_lock;
        static Glib::Threads::Cond  SourcesToAnalyse;
	static std::list<boost::weak_ptr<Source> > analysis_queue;

	static void analyse_audio_file_source (boost::shared_ptr<AudioFileSource>);
};


}

#endif /* __ardour_analyser_h__ */
