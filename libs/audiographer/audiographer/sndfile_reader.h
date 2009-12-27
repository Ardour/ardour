#ifndef AUDIOGRAPHER_SNDFILE_READER_H
#define AUDIOGRAPHER_SNDFILE_READER_H

#include "sndfile_base.h"
#include "listed_source.h"
#include "process_context.h"

namespace AudioGrapher
{

/** Reader for audio files using libsndfile.
  * Once again only short, int and float are valid template parameters
  */
template<typename T>
class SndfileReader : public virtual SndfileBase, public ListedSource<T>
{
  public:
	  
	enum SeekType {
		SeekBeginning = SEEK_SET, //< Seek from beginning of file
		SeekCurrent = SEEK_CUR, //< Seek from current position
		SeekEnd = SEEK_END //< Seek from end
	};
	
  public:

	SndfileReader (ChannelCount channels, nframes_t samplerate, int format, std::string path);
	
	nframes_t seek (nframes_t frames, SeekType whence);
	nframes_t read (ProcessContext<T> & context);
	
  private:

	void init(); // init read function
	sf_count_t (*read_func)(SNDFILE *, T *, sf_count_t);
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_READER_H