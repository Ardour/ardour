template<typename T>
Interleaver<T>::Interleaver()
  : channels (0)
  , max_frames (0)
  , buffer (0)
{}

template<typename T>
void
Interleaver<T>::init (unsigned int num_channels, nframes_t max_frames_per_channel)
{
	reset();
	channels = num_channels;
	max_frames = max_frames_per_channel;
	
	buffer = new T[channels * max_frames];
	
	for (unsigned int i = 0; i < channels; ++i) {
		inputs.push_back (InputPtr (new Input (*this, i)));
	}
}

template<typename T>
typename Source<T>::SinkPtr
Interleaver<T>::input (unsigned int channel)
{
	if (channel >= channels) {
		throw Exception (*this, "Channel out of range");
	}
	
	return boost::static_pointer_cast<Sink<T> > (inputs[channel]);
}

template<typename T>
void
Interleaver<T>::reset_channels ()
{
	for (unsigned int i = 0; i < channels; ++i) {
		inputs[i]->reset();
	}

}

template<typename T>
void
Interleaver<T>::reset ()
{
	inputs.clear();
	delete [] buffer;
	buffer = 0;
	channels = 0;
	max_frames = 0;
}

template<typename T>
void
Interleaver<T>::write_channel (ProcessContext<T> const & c, unsigned int channel)
{
	if (c.frames() > max_frames) {
		reset_channels();
		throw Exception (*this, "Too many frames given to an input");
	}
	
	for (unsigned int i = 0; i < c.frames(); ++i) {
		buffer[channel + (channels * i)] = c.data()[i];
	}
	
	nframes_t const ready_frames = ready_to_output();
	if (ready_frames) {
		ProcessContext<T> c_out (c, buffer, ready_frames, channels);
		ListedSource<T>::output (c_out);
		reset_channels ();
	}
}

template<typename T>
nframes_t
Interleaver<T>::ready_to_output ()
{
	nframes_t ready_frames = inputs[0]->frames();
	if (!ready_frames) { return 0; }

	for (unsigned int i = 1; i < channels; ++i) {
		nframes_t const frames = inputs[i]->frames();
		if (!frames) { return 0; }
		if (frames != ready_frames) {
			init (channels, max_frames);
			throw Exception (*this, "Frames count out of sync");
		}
	}
	return ready_frames * channels;
}
