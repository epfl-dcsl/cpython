/*
 * Support for sandboxes
 */
#ifndef _SANDBOX_H
#define _SANDBOX_H

int sb_init_backend(void); // TODO not void but I don't know what
int sb_prolog(const char *); 
int sb_epilog(const char *);

#endif
