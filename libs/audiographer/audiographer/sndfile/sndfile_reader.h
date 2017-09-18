#ifndef AUDIOGRAPHER_SNDFILE_READER_H
#define AUDIOGRAPHER_SNDFILE_READER_H

#include "audiographer/utils/listed_source.h"
#include "audiographer/process_context.h"
#include "audiographer/sndfile/sndfile_base.h"

namespace AudioGrapher
{

/** Reader for audio files using libsndfile.
  * Only short, int and float are valid template parameters
  */
template<typename T = DefaultSampleType>
class SndfileReader
  : public virtual SndfileBase
  , public ListedSource<T>
  , public Throwing<>
{
  public:

	SndfileReader (std::string const & path) : SndfileHandle (path) {}
	virtual ~SndfileReader () {}

	SndfileReader (SndfileReader const & other) : SndfileHandle (other) {}
	using SndfileHandle::operator=;

	/** Read data into buffer in \a context, only the data is modified (not sample count)
	 *  Note that the data read is output to the outputs, as well as read into the context
	 *  \return number of samples read
	 */
	samplecnt_t read (ProcessContext<T> & context)
	{
		if (throw_level (ThrowStrict) && context.channels() != channels() ) {
			throw Exception (*this, boost::str (boost::format
				("Wrong number of channels given to process(), %1% instead of %2%")
				% context.channels() % channels()));
		}

		samplecnt_t const samples_read = SndfileHandle::read (context.data(), context.samples());
		ProcessContext<T> c_out = context.beginning (samples_read);

		if (samples_read < context.samples()) {
			c_out.set_flag (ProcessContext<T>::EndOfInput);
		}
		this->output (c_out);
		return samples_read;
	}

  protected:
	/// SndfileHandle has to be constructed directly by deriving classes
	SndfileReader () {}
};

} // namespace

#endif // AUDIOGRAPHER_SNDFILE_READER_H
