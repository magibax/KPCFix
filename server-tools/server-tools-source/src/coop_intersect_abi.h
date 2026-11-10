#ifndef KPC_COOP_INTERSECT_ABI_H
#define KPC_COOP_INTERSECT_ABI_H

/* Observed in original 1.13 object.lto (SHA-256:
 * 3a03f8796c482b1a7ad72755df33ee8f1f8056bb10957f6f700e64bd006b3e5b).
 * Slot +0x90 of the direct server API pointer: CDECL (query*, info*) -> bool
 * in AL. NOT a C++ vtable slot and NOT a player-volume sweep.
 *
 * This vintage query is NOT the same as newer LithTech SDK revisions:
 * observed KPC fields are flags +0x24, filter +0x28 and filter user +0x2c.
 * The trailing zeros provide overrun protection for experimental diagnostic
 * calls and are NOT part of the proven 0x30-byte input footprint.
 */
#define KPC_INTERSECT_SERVER_SLOT 0x90u
#define KPC_INTERSECT_OBJECTS 1u
#define KPC_INTERSECT_HPOLY 4u
#define KPC_INTERSECT_QUERY_MIN_BYTES 0x30u

typedef struct { float x,y,z; } KpcIntersectVec;
typedef struct { KpcIntersectVec normal; float distance; } KpcIntersectPlane;

typedef struct {
 KpcIntersectVec from;         /* +0x00 */
 KpcIntersectVec to;           /* +0x0c */
 KpcIntersectVec direction;    /* +0x18; unused by segment */
 U32 flags;                    /* +0x24 */
 U32 object_filter;            /* +0x28, 32-bit function address */
 U32 filter_userdata;         /* +0x2c, player HOBJECT used by callback */
 U32 reserved[4];             /* diagnostic padding; not ABI fields */
} KpcIntersectQuery;

typedef struct {
 KpcIntersectVec point;       /* +0x00, observed in live caller */
 KpcIntersectPlane plane;     /* +0x0c */
 U32 hit_object;              /* +0x1c */
 U32 hit_poly;                /* +0x20 */
 U32 surface_flags;           /* +0x24 */
 U32 model_node;              /* +0x28; optional */
 U32 reserved[5];             /* diagnostic padding; not ABI fields */
} KpcIntersectInfo;

_Static_assert(sizeof(KpcIntersectVec)==12,"KPC LTVector must be 12 bytes");
_Static_assert(__builtin_offsetof(KpcIntersectQuery,to)==0x0c,"query To offset");
_Static_assert(__builtin_offsetof(KpcIntersectQuery,direction)==0x18,"query direction offset");
_Static_assert(__builtin_offsetof(KpcIntersectQuery,flags)==0x24,"query flags offset");
_Static_assert(__builtin_offsetof(KpcIntersectQuery,object_filter)==0x28,"query filter offset");
_Static_assert(__builtin_offsetof(KpcIntersectQuery,filter_userdata)==0x2c,"query userdata offset");
_Static_assert(sizeof(KpcIntersectQuery)==0x40,"guarded query storage");
_Static_assert(__builtin_offsetof(KpcIntersectInfo,plane)==0x0c,"info plane offset");
_Static_assert(__builtin_offsetof(KpcIntersectInfo,hit_object)==0x1c,"info object offset");
_Static_assert(__builtin_offsetof(KpcIntersectInfo,hit_poly)==0x20,"info poly offset");
_Static_assert(__builtin_offsetof(KpcIntersectInfo,surface_flags)==0x24,"info flags offset");
_Static_assert(sizeof(KpcIntersectInfo)==0x40,"guarded info storage");

#endif
