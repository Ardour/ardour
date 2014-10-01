#ifndef __safe_delete_h__
	#define __safe_delete_h__
	

/* Copy to include:
#include "safe_delete.h"
*/

#define	safe_delete(__pObject__) {if((__pObject__) != 0) {delete (__pObject__); (__pObject__) = 0;}}

#define	safe_delete_array(__pArray__) {if((__pArray__) != 0) {delete [] (__pArray__); (__pArray__) = 0;}}

template <class T> void safe_delete_from_iterator(T* pToDelete)
{
	safe_delete(pToDelete);
}

#endif // __safe_delete_h__
