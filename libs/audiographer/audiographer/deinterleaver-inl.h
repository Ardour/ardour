template<typename T>
DeInterleaver<T>::DeInterleaver()
  : channels (0)
  , max_frames (0)
  , buffer (0)
  {}
  
template<typename T>
void
DeInterleaver<T>::init (unsigned int num_channels, nframes_t max_frames_per_channel)
{
	reset();
	channels = num_channels;
	max_frames = max_frames_per_channel;
	buffer = new T[max_frames];
	
	for (unsigned int i = 0; i < channels; ++i) {
		outputs.push_back (OutputPtr (new IdentityVertex<T>));
	}
}

template<typename T>
typename DeInterleaver<T>::SourcePtr
DeInterleaver<T>::output (unsigned int channel)
{
	if (channel >= channels) {
		throw Exception (*this, "channel out of range");
	}
	
	return boost::static_pointer_cast<Source<T> > (outputs[channel]);
}

template<typename T>
void
DeInterleaver<T>::process (ProcessContext<T> const & c)
{
	nframes_t frames = c.frames();
	T const * data = c.data();
	
	if (frames == 0) { return; }
	
	nframes_t const  frames_per_channel = frames / channels;
	
	if (c.channels() != channels) {
		throw Exception (*this, "wrong amount of channels given to process()");
	}
	
	if (frames % channels != 0) {
		throw Exception (*this, "wrong amount of frames given to process()");
	}
	
	if (frames_per_channel > max_frames) {
		throw Exception (*this, "too many frames given to process()");
	}
	
	unsigned int channel = 0;
	for (typename std::vector<OutputPtr>::iterator it = outputs.begin(); it != outputs.end(); ++it, ++channel) {
		if (!*it) { continue; }
		
		for (unsigned int i = 0; i < frames_per_channel; ++i) {
			buffer[i] = data[channel + (channels * i)];
		}
		
		ProcessContext<T> c_out (c, buffer, frames_per_channel, 1);
		(*it)->process (c_out);
	}
}

template<typename T>
void
DeInterleaver<T>::reset ()
{
	outputs.clear();
	delete [] buffer;
	buffer = 0;
	channels = 0;
	max_frames = 0;
}
