#ifndef KPC_SHA256_H
#define KPC_SHA256_H
typedef struct { unsigned int h[8],used,total; unsigned char block[64]; } KpcSha;
void kpc_sha_init(KpcSha *);
void kpc_sha_update(KpcSha *,const unsigned char *,unsigned int);
void kpc_sha_final(KpcSha *,unsigned char out[32]);
#endif
