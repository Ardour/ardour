#ifndef __pbd_fpu_h__
#define __pbd_fpu_h__

namespace PBD {


class FPU {
  private:
	enum Flags {
		HasFlushToZero = 0x1,
		HasDenormalsAreZero = 0x2,
		HasSSE = 0x4,
		HasSSE2 = 0x8
	};

  public:
	FPU ();
	~FPU ();

	bool has_flush_to_zero () const { return _flags & HasFlushToZero; }
	bool has_denormals_are_zero () const { return _flags & HasDenormalsAreZero; }
	bool has_sse () const { return _flags & HasSSE; }
	bool has_sse2 () const { return _flags & HasSSE2; }
	
  private:
	Flags _flags;
};

}

#endif /* __pbd_fpu_h__ */
