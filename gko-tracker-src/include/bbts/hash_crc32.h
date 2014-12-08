#ifndef OP_OPED_NOAH_BBTS_HASHCRC32_H_
#define OP_OPED_NOAH_BBTS_HASHCRC32_H_


/**
 * The crc32 functions and data was originally written by Spencer
 * Garrett <srg@quick.com> and was gleaned from the PostgreSQL source
 * tree via the files contrib/ltree/crc32.[ch] and from FreeBSD at
 * src/usr.bin/cksum/crc32.c.
 */

namespace bbts {

uint32_t hash_crc32(const char *key, size_t key_length, void *context __attribute__((unused)));

}

#endif // OP_OPED_NOAH_BBTS_HASHCRC32_H_
