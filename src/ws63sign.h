/*
  ws63sign - Hisense WS63 Machine Code Signing Tool
  Copyright (C) 2025  Gong Zhile

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _WS63SIGN_H_
#define _WS63SIGN_H_

#include "config.h"
#include "sha256.h"

#include <endian.h>
#include <stddef.h>
#include <string.h>

#define ROOTPUBK_KEY_AREA_IID		(0x4b0f2d1e)
#define ROOTPUBK_STRUCT_VERSION		(0x00010000)
#define ROOTPUBK_KEY_BRAINPOOL256	(0x2a13c812)
#define ROOTPUBK_KEY_LENGTH		(0x00000004)

#define CODEINFO_KEY_AREA_IID		(0x4b0f2d2d)
#define FLASH_NO_ENCRY_FLAG		(0x3c7896e1)

#pragma pack(push,1)
struct ws63_rootpubk_header {
	uint32_t	image_id;
	uint32_t	struct_ver;
	uint32_t	struct_len;
	uint32_t	key_owner_id;
	uint32_t	_unknown;
	uint32_t	key_id;
	uint32_t	key_alg;
	uint32_t	ecc_curve_type;
	uint32_t	key_len;
};				/* Ofs = x0 */
#pragma pack(pop)

#pragma pack(push,1)
struct ws63_codeinfo_header {
	uint32_t	image_id;
	uint32_t	struct_ver;
	uint32_t	struct_len;
	uint32_t	signature_len;
	uint32_t	version_ext;
	uint32_t	mask_ver_ext;
	uint32_t	msid_ext;
	uint32_t	mask_msid_ext;
	uint32_t	code_addr;
	uint32_t	code_len;
	uint8_t		code_hash[32];	/* sha256 chksum */
	uint32_t	code_enc_flag;
	
};				/* Ofs = 0x100 */
#pragma pack(pop)

struct ws63sign_ctx {
	unsigned char		buf[0x300];
	size_t			len;
	struct sha256_ctx	sha256_ctx;
};

static void
ws63sign_init(struct ws63sign_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	sha256_init_ctx(&ctx->sha256_ctx);

	struct ws63_rootpubk_header *rootpubk = (void *) ctx->buf;

	rootpubk->image_id       = htole32(ROOTPUBK_KEY_AREA_IID);
	rootpubk->struct_ver     = htole32(ROOTPUBK_STRUCT_VERSION);
	rootpubk->struct_len     = htole32(0x100);
	rootpubk->key_owner_id   = htole32(0x40);

	/* An unknown field which doesn't exist in the reference. */
	rootpubk->_unknown       = htole32(0x1);

	rootpubk->key_id	       = htole32(0x1);
	rootpubk->key_alg	       = htole32(ROOTPUBK_KEY_BRAINPOOL256);
	rootpubk->ecc_curve_type = htole32(ROOTPUBK_KEY_BRAINPOOL256);
	rootpubk->key_len	       = htole32(0x40);

	struct ws63_codeinfo_header *codei = (void *) ctx->buf + 0x100;

	codei->image_id	     = htole32(CODEINFO_KEY_AREA_IID);
	codei->struct_ver    = htole32(ROOTPUBK_STRUCT_VERSION);
	codei->struct_len    = htole32(0x200);
	codei->signature_len = htole32(0x40);

	codei->code_enc_flag = htole32(FLASH_NO_ENCRY_FLAG);
}

static void
ws63sign_feed(struct ws63sign_ctx *ctx, const uint8_t *code, size_t len)
{
	sha256_process_bytes(code, len, &ctx->sha256_ctx);
	ctx->len += len;
}

static size_t
ws63sign_finalize(struct ws63sign_ctx *ctx)
{
	struct ws63_codeinfo_header *codei = (void *) ctx->buf + 0x100;
	size_t aligned_len = (ctx->len + 15) & ~15;
	size_t padding = aligned_len - ctx->len;

	unsigned char nil128[16] = { 0 };
	sha256_process_bytes(nil128, aligned_len - ctx->len, &ctx->sha256_ctx);
	sha256_finish_ctx(&ctx->sha256_ctx, codei->code_hash);

	ctx->len	= aligned_len;
	codei->code_len = htole32(ctx->len);
	return padding;
}

#endif	/* _WS63SIGN_H_ */
