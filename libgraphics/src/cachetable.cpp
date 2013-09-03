#include "graphics.h"
#include "graphics-internal.h"

////////////////////////////////////////////////////////////////////////////////

#ifndef UINDEX_MAX
#define UINDEX_MAX 4294967295U
#endif

#define ELF_STEP(B) T1 = (H << 4) + B; T2 = T1 & 0xF0000000; if (T2) T1 ^= (T2 >> 24); T1 &= (~T2); H = T1;

hash_t MCGHashBytes(const void *p_bytes, size_t length)
{
	uint8_t *bytes = (uint8_t *)p_bytes;
	
    /* The ELF hash algorithm, used in the ELF object file format */
    uint32_t H = 0, T1, T2;
    int32_t rem = length;
	
    while (3 < rem)
	{
		ELF_STEP(bytes[length - rem]);
		ELF_STEP(bytes[length - rem + 1]);
		ELF_STEP(bytes[length - rem + 2]);
		ELF_STEP(bytes[length - rem + 3]);
		rem -= 4;
    }
	
    switch (rem)
	{
		case 3:  ELF_STEP(bytes[length - 3]);
		case 2:  ELF_STEP(bytes[length - 2]);
		case 1:  ELF_STEP(bytes[length - 1]);
		case 0:  ;
    }
	
    return H;
}

#undef ELF_STEP

////////////////////////////////////////////////////////////////////////////////

struct __MCGCacheTableEntry
{
	hash_t		hash;
	uint32_t	key_length;
	void		*key;
	void		*value;
};

struct __MCGCacheTable
{
	uindex_t				total_buckets;
	uint32_t				used_buckets;
	__MCGCacheTableEntry	*pairs;
};

////////////////////////////////////////////////////////////////////////////////

bool MGCCacheTableCreate(uindex_t p_size, MCGCacheTableRef &r_cache_table)
{
	bool t_success;
	t_success = true;
	
	__MCGCacheTable *t_cache_table;
	t_cache_table = NULL;
	if (t_success)
		t_success = MCMemoryNew(t_cache_table);
	
	__MCGCacheTableEntry *t_pairs;
	t_pairs = NULL;
	if (t_success)
		t_success = MCMemoryNewArray(p_size, t_pairs);
	
	if (t_success)
	{
		t_cache_table -> total_buckets = p_size;
		t_cache_table -> used_buckets = 0;
		t_cache_table -> pairs = t_pairs;
		r_cache_table = t_cache_table;
	}
	else
	{
		MCMemoryDeleteArray(t_pairs);
		MCMemoryDelete(t_cache_table);
	}
	
	return t_success;
}

void MGCCacheTableDestroy(MCGCacheTableRef self)
{
	if (self = NULL)
		return;
	
	if (self -> pairs != NULL)
	{
		for (uindex_t i = 0; i < self -> total_buckets; i++)
		{
			MCMemoryDelete(self -> pairs[i] . key);
			MCMemoryDelete(self -> pairs[i] . value);
		}
		
		MCMemoryDeleteArray(self -> pairs);
	}
	
	MCMemoryDelete(self);
}

static uindex_t MCGCacheTableLookup(MCGCacheTableRef self, void *p_key, uint32_t p_key_length, hash_t p_hash)
{
	uindex_t t_probe;
	t_probe = p_hash % self -> total_buckets;	
	
	if (self -> used_buckets != self -> total_buckets)
	{
		for (uindex_t i = 0; i < self -> total_buckets; i++)
		{
			__MCGCacheTableEntry *t_pair;
			t_pair = &self -> pairs[t_probe];
			
			if (t_pair -> value == NULL || t_pair -> hash == p_hash && t_pair -> key_length == p_key_length && MCMemoryEqual(p_key, t_pair -> key, p_key_length))
				return t_probe;
			
			t_probe++;
			if (t_probe > self -> total_buckets)
				t_probe -= self -> total_buckets;
		}
	}
	
	return UINDEX_MAX;
}

void MCGCacheTableSet(MCGCacheTableRef self, void *p_key, uint32_t p_key_length, void *p_value)
{
	if (self == NULL)
		return;	
	
	hash_t t_hash;
	t_hash = MCGHashBytes(p_key, p_key_length);
	
	uindex_t t_target_bucket;
	t_target_bucket = MCGCacheTableLookup(self, p_key, p_key_length, t_hash);
	
	if (t_target_bucket == UINDEX_MAX)
	{
		t_target_bucket = t_hash % self -> total_buckets;
		MCMemoryDelete(self -> pairs[t_target_bucket] . key);
		MCMemoryDelete(self -> pairs[t_target_bucket] . value);
		
		self -> pairs[t_target_bucket] . hash = t_hash;
		self -> pairs[t_target_bucket] . key = p_key;
		self -> pairs[t_target_bucket] . key_length = p_key_length;
		self -> pairs[t_target_bucket] . value = p_value;
		
		//MCLog("MCGCacheTableSet: Cache table overflow. Hash %d thrown out.", t_target_bucket);
	}
	else if (self -> pairs[t_target_bucket] . value != NULL)
	{
		MCMemoryDelete(p_key);
		MCMemoryDelete(self -> pairs[t_target_bucket] . value);
		
		self -> pairs[t_target_bucket] . value = p_value;
		
		//MCLog("MCGCacheTableSet: Cache table overwrite. Hash %d rewritten.", t_target_bucket);
	}
	else
	{
		self -> pairs[t_target_bucket] . hash = t_hash;
		self -> pairs[t_target_bucket] . key = p_key;
		self -> pairs[t_target_bucket] . key_length = p_key_length;
		self -> pairs[t_target_bucket] . value = p_value;
		
		self -> used_buckets++;
		
		//MCLog("MCGCacheTableSet: Cache table write. Hash %d written.", t_target_bucket);
	}	
}

void *MCGCacheTableGet(MCGCacheTableRef self, void *p_key, uint32_t p_key_length)
{
	if (self == NULL)
		return NULL;	
	
	hash_t t_hash;
	t_hash = MCGHashBytes(p_key, p_key_length);
	
	uindex_t t_target_bucket;
	t_target_bucket = MCGCacheTableLookup(self, p_key, p_key_length, t_hash);
	
	if (t_target_bucket == UINDEX_MAX)
	{
		//MCLog("MCGCacheTableGet: No value found for key. Table full.", NULL);
		return NULL;
	}
	else
	{
		//if (self -> pairs[t_target_bucket] . value == NULL)
		//	MCLog("MCGCacheTableGet: No value found for key. Value empty", NULL);
		//else
		//	MCLog("MCGCacheTableGet: Value found for key.", NULL);
		return self -> pairs[t_target_bucket] . value;
	}
}