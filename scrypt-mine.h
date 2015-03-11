#ifndef SCRYPT_MINE_H
#define SCRYPT_MINE_H

#include <stdint.h>

void scrypt_hash( const void* input, size_t inputlen, uint32_t *res, unsigned char Nfactor );

#endif // SCRYPT_MINE_H

