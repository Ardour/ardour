#ifndef __libmisc_restartable_rw__h__
#define __libmisc_restartable_rw__h__

extern int restartable_write (int fd, unsigned char *buf, size_t cnt);
extern int restartable_read (int fd, unsigned char *buf, size_t cnt);

#endif // __libmisc_restartable_rw__h__
