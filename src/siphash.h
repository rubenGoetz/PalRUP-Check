
#pragma once

#include "utils/palrup_utils.h"

void siphash_init(const unsigned char* key_128bit);
void siphash_reset();
void siphash_update(const unsigned char* data, u64 nb_bytes);
void siphash_pad(u64 nb_bytes);
u8* siphash_digest();
void siphash_free();
